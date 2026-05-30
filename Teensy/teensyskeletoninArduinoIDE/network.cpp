#include <Arduino.h>
#include <QNEthernet.h>
#include "network.h"
#include "comms.h"
#include "webpage.h"

using namespace qindesign::network;

// ---- Static fallback IP (used if DHCP times out) ----
static const IPAddress STATIC_IP (192, 168, 1, 200);
static const IPAddress STATIC_SUB(255, 255, 255,   0);
static const IPAddress STATIC_GW (192, 168, 1,   1);

static EthernetServer http_server(80);
static EthernetServer telnet_server(23);
static EthernetClient telnet_client;

static char ip_buf[20];     // "xxx.xxx.xxx.xxx\0"

// Telnet line buffer
static char  tln_buf[128];
static int   tln_len = 0;

// ============================================================
// Helpers
// ============================================================

// Read one header line (up to \n) with a timeout.
// Returns true if a full line was placed in buf (stripped of \r\n).
static bool read_line(EthernetClient& c, char* buf, int maxlen, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  int i = 0;
  while (millis() - t0 < timeout_ms) {
    if (c.available()) {
      char ch = (char)c.read();
      if (ch == '\n') {
        if (i > 0 && buf[i - 1] == '\r') i--;
        buf[i] = '\0';
        return true;
      }
      if (i < maxlen - 1) buf[i++] = ch;
    }
  }
  return false;  // timeout
}

// Read exactly `n` bytes into buf with timeout.
static int read_bytes(EthernetClient& c, char* buf, int n, uint32_t timeout_ms) {
  uint32_t t0 = millis();
  int got = 0;
  while (got < n && millis() - t0 < timeout_ms) {
    if (c.available()) buf[got++] = (char)c.read();
  }
  buf[got] = '\0';
  return got;
}

static void send_headers(EthernetClient& c,
                          const char* status,
                          const char* content_type,
                          bool cors = false) {
  c.print(F("HTTP/1.0 ")); c.println(status);
  c.print(F("Content-Type: ")); c.println(content_type);
  if (cors) c.println(F("Access-Control-Allow-Origin: *"));
  c.println();
}

// ============================================================
// HTTP request handler
// ============================================================
static void handle_http(EthernetClient& c,
                         LockChannel ch[2], PID pid[2],
                         DispModel& disp,
                         bool pll_lock, float refclk_mhz) {
  char line[128];

  // Request line: "METHOD /path HTTP/x.x"
  if (!read_line(c, line, sizeof(line), 500)) { c.stop(); return; }

  char method[8] = {}, path[64] = {};
  sscanf(line, "%7s %63s", method, path);

  // Consume remaining headers; capture Content-Length
  int content_length = 0;
  while (read_line(c, line, sizeof(line), 200)) {
    if (line[0] == '\0') break;  // blank line = end of headers
    if (strncasecmp(line, "Content-Length:", 15) == 0)
      content_length = atoi(line + 15);
  }

  // CORS preflight
  if (strcmp(method, "OPTIONS") == 0) {
    c.println(F("HTTP/1.0 204 No Content"));
    c.println(F("Access-Control-Allow-Origin: *"));
    c.println(F("Access-Control-Allow-Methods: GET, POST, OPTIONS"));
    c.println(F("Access-Control-Allow-Headers: Content-Type"));
    c.println();
    c.stop();
    return;
  }

  // GET /
  if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
    send_headers(c, "200 OK", "text/html; charset=utf-8");
    c.print(WEBPAGE_HTML);
    c.stop();
    return;
  }

  // GET /api/status
  if (strcmp(method, "GET") == 0 && strcmp(path, "/api/status") == 0) {
    send_headers(c, "200 OK", "application/json", true);
    comms_json_status(c, ch, pid, disp, pll_lock, refclk_mhz, ip_buf, millis());
    c.stop();
    return;
  }

  // POST /api/cmd
  if (strcmp(method, "POST") == 0 && strcmp(path, "/api/cmd") == 0) {
    char body[128] = {};
    if (content_length > 0)
      read_bytes(c, body, min(content_length, (int)sizeof(body) - 1), 300);

    String cmd(body);
    cmd.trim();

    send_headers(c, "200 OK", "text/plain; charset=utf-8", true);
    comms_process(cmd, c, ch, pid, disp);
    c.stop();
    return;
  }

  // 404
  send_headers(c, "404 Not Found", "text/plain");
  c.println(F("Not found"));
  c.stop();
}

// ============================================================
// Telnet handler
// ============================================================
static void handle_telnet(LockChannel ch[2], PID pid[2], DispModel& disp) {
  // Accept new connection if none active
  if (!telnet_client || !telnet_client.connected()) {
    EthernetClient nc = telnet_server.accept();
    if (nc) {
      telnet_client = nc;
      tln_len = 0;
      telnet_client.println(F("Dual-PDH Controller CLI v1.0"));
      telnet_client.println(F("Type 'help' for commands."));
      telnet_client.print(F("> "));
    }
    return;
  }

  // Read characters, process on newline
  while (telnet_client.available()) {
    char c = (char)telnet_client.read();
    if (c == '\n' || c == '\r') {
      if (tln_len > 0) {
        tln_buf[tln_len] = '\0';
        String ln(tln_buf);
        comms_process(ln, telnet_client, ch, pid, disp);
        telnet_client.print(F("> "));
        tln_len = 0;
      }
    } else if (c == 127 || c == '\b') {   // backspace
      if (tln_len > 0) tln_len--;
    } else if (c >= 0x20 && tln_len < 127) {
      tln_buf[tln_len++] = c;
    }
  }
}

// ============================================================
// Public API
// ============================================================

void network_init() {
  Ethernet.setHostname("dual-pdh");
  Serial.print(F("Ethernet: DHCP..."));
  bool dhcp = Ethernet.begin();
  if (!dhcp || Ethernet.localIP() == INADDR_NONE) {
    Serial.print(F(" failed, using static "));
    Ethernet.begin(STATIC_IP, STATIC_SUB, STATIC_GW);
  }
  Ethernet.waitForLocalIP(3000);

  IPAddress ip = Ethernet.localIP();
  snprintf(ip_buf, sizeof(ip_buf), "%d.%d.%d.%d",
           ip[0], ip[1], ip[2], ip[3]);
  Serial.print(F(" IP: ")); Serial.println(ip_buf);
  Serial.println(F("  mDNS: dual-pdh.local"));

  http_server.begin();
  telnet_server.begin();
  Serial.println(F("  HTTP port 80 | Telnet port 23"));
}

void network_poll(LockChannel ch[2], PID pid[2],
                  DispModel& disp,
                  bool pll_lock, float refclk_mhz) {
  Ethernet.loop();  // drive QNEthernet internal stack

  // Handle up to 4 pending HTTP connections per call
  for (int i = 0; i < 4; i++) {
    EthernetClient client = http_server.accept();
    if (!client) break;
    handle_http(client, ch, pid, disp, pll_lock, refclk_mhz);
  }

  handle_telnet(ch, pid, disp);
}

const char* network_ip() { return ip_buf; }
