#include <Arduino.h>
#include <math.h>
#include "display.h"

// ============================================================
// RGB565 colour palette — matches the HTML prototype tokens
// ============================================================
#define COL_BG        0x0000u  // #070b11  deep background
#define COL_SCREEN    0x0020u  // #0a0f17  TFT field
#define COL_PANEL     0x08A3u  // #0e141e  channel panel
#define COL_PANEL2    0x10C4u  // #121a26  stat cell bg
#define COL_RULE      0x1926u  // #1b2533  hairline border
#define COL_GRID      0x1105u  // #16202e  grid lines

#define COL_INK       0xEF7Eu  // #e8eef6  primary text
#define COL_INK_DIM   0x84B5u  // #8696aa  dim labels
#define COL_INK_MUTE  0x4ACDu  // #4a586b  muted / axis

#define COL_CH1       0x2EB9u  // #2dd4cf  teal
#define COL_CH2       0xF524u  // #f5a524  amber
#define COL_CH1_FILL  0x0924u  // teal 18%
#define COL_CH2_FILL  0x28E0u  // amber 18%

#define COL_ST_IDLE   0x63B1u  // #64748b  slate
#define COL_ST_SWEEP  0x5D5Fu  // #5aa9ff  blue
#define COL_ST_ACQ    0xFDE4u  // #fbbf24  amber
#define COL_ST_LOCK   0x3714u  // #34e0a1  green
#define COL_ST_HOLD   0xA45Fu  // #a78bfa  violet
#define COL_ST_FAULT  0xFACDu  // #ff5a6a  red

// ============================================================
// Layout — 320×240 landscape, 2-channel stacked
// ============================================================
static const int16_t TOP_H     = 22;   // top status bar height
static const int16_t CH_H      = 109;  // (240-22)/2
static const int16_t CH1_Y     = 22;
static const int16_t CH2_Y     = 131;

static const int16_t RAIL_W    = 4;
static const int16_t PLOT_X    = 12;   // RAIL_W + 8 left pad
static const int16_t PLOT_W    = 144;
static const int16_t PLOT_H    = 100;  // CH_H - 4top - 5bot - 1border*2 ... outer h
static const int16_t PI_W      = 142;  // inner width  (PLOT_W - 2)
static const int16_t PI_H      = 98;   // inner height (PLOT_H - 2)

static const int16_t STATS_X   = 163;  // PLOT_X + PLOT_W + 7
static const int16_t STATS_W   = 150;
static const int16_t CELL_W    = 73;   // (STATS_W - CELL_GAP) / 2
static const int16_t CELL_GAP  = 4;
static const int16_t CELL_H    = 34;
static const int16_t HEALTH_H  = 14;

// Y offsets relative to channel top (ch_y)
static const int16_t ROW0_OFF  = 8;    // cy + ROW0_OFF
static const int16_t ROW1_OFF  = 46;   // cy + ROW1_OFF
static const int16_t HLTH_OFF  = 84;   // cy + HLTH_OFF

// Plot inner top-left: (PLOT_X+1, cy+5)
// Coordinate space used for trace math (matching pdh-screen.js):
//   PW=200, PH=80, BASE=70
static const float PW = 200.0f;
static const float PH = 80.0f;
static const float BASE = 70.0f;

// ============================================================
// Helpers
// ============================================================

static uint16_t color_dim(uint16_t c, uint8_t f) {
  uint8_t r = ((c >> 11) & 0x1F) * f / 255;
  uint8_t g = ((c >> 5)  & 0x3F) * f / 255;
  uint8_t b = ( c        & 0x1F) * f / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static uint32_t lcg(uint32_t s) { return s * 1664525UL + 1013904223UL; }

static uint16_t state_color(LockState s) {
  switch (s) {
    case LOCK_IDLE:    return COL_ST_IDLE;
    case LOCK_SEARCH:  return COL_ST_SWEEP;
    case LOCK_ACQUIRE: return COL_ST_ACQ;
    case LOCK_LOCKED:  return COL_ST_LOCK;
    case LOCK_HOLD:    return COL_ST_HOLD;
    case LOCK_RELOCK:  return COL_ST_FAULT;
    default:           return COL_ST_IDLE;
  }
}

static const char* state_label(LockState s) {
  switch (s) {
    case LOCK_IDLE:    return "IDLE";
    case LOCK_SEARCH:  return "SWEEP";
    case LOCK_ACQUIRE: return "ACQUIRE";
    case LOCK_LOCKED:  return "LOCKED";
    case LOCK_HOLD:    return "HOLD";
    case LOCK_RELOCK:  return "LOST";
    default:           return "?";
  }
}

static const char* state_axis(LockState s) {
  switch (s) {
    case LOCK_IDLE:    return "STANDBY";
    case LOCK_SEARCH:  return "TEC/LAS-I SWEEP";
    case LOCK_ACQUIRE: return "PEAK LOCK-IN";
    case LOCK_LOCKED:  return "PDH ERROR IN-LOCK";
    case LOCK_HOLD:    return "PDH ERROR HOLD";
    case LOCK_RELOCK:  return "ERROR RE-ACQUIRE";
    default:           return "";
  }
}

// ============================================================
// Trace functions — all operate in inner-plot pixel space
// px0,py0 = absolute screen top-left of inner plot area
// ============================================================

static void draw_grid(Adafruit_ILI9341 &tft, int16_t px0, int16_t py0) {
  // Vertical lines at original x = 40,80,120,160 → scaled to PI_W
  static const uint8_t vx[4] = {28, 57, 85, 113};
  for (int i = 0; i < 4; i++)
    tft.drawFastVLine(px0 + vx[i], py0, PI_H, COL_GRID);
  // Horizontal lines at original y = 20,40,60 → scaled to PI_H
  static const uint8_t hy[3] = {25, 49, 74};
  for (int i = 0; i < 3; i++)
    tft.drawFastHLine(px0, py0 + hy[i], PI_W, COL_GRID);
}

// Lorentzian (transmission) trace — SEARCH / ACQUIRE states
static void draw_lorentzian(Adafruit_ILI9341 &tft,
                             int16_t px0, int16_t py0,
                             float peak_ph,
                             bool wide,                // coarse (wider) vs fine
                             uint16_t accent,
                             uint16_t fill) {
  float w_orig = wide ? 24.0f : 12.0f;
  float amp    = wide ? 46.0f : 56.0f;
  float base_py = BASE * PI_H / PH;        // ~86 px from inner top
  float x0 = peak_ph * PW;

  int16_t prev_py = (int16_t)base_py;
  for (int16_t px = 0; px <= PI_W; px += 2) {
    float xo = px * PW / (float)PI_W;
    float u  = (xo - x0) / w_orig;
    float yo = BASE - amp / (1.0f + u * u);
    int16_t tpy = (int16_t)(yo * PI_H / PH);
    tpy = constrain(tpy, 0, PI_H - 1);

    // filled area under curve
    int16_t fill_h = (int16_t)base_py - tpy;
    if (fill_h > 0)
      tft.drawFastVLine(px0 + px, py0 + tpy, fill_h, fill);

    // trace line
    if (px > 0)
      tft.drawLine(px0 + px - 2, py0 + prev_py,
                   px0 + px,     py0 + tpy, accent);
    prev_py = tpy;
  }
}

// Dispersive PDH S-curve — ACQUIRE state
static void draw_error_curve(Adafruit_ILI9341 &tft,
                              int16_t px0, int16_t py0,
                              float peak_ph) {
  const float d = 1.9f, A = 24.0f, mid = 38.0f;
  float x0 = peak_ph * PW;

  int16_t prev_py = (int16_t)(mid * PI_H / PH);
  for (int16_t px = 0; px <= PI_W; px += 2) {
    float xo = px * PW / (float)PI_W;
    float u  = (xo - x0) / 12.0f;
    float d2 = 1.0f + u * u;
    float central = -2.0f * u / (d2 * d2);
    float sbL = 0.8f * (-(u + d) / (1.0f + (u + d) * (u + d)));
    float sbR = 0.8f * (-(u - d) / (1.0f + (u - d) * (u - d)));
    float yo = mid - A * central - 5.0f * (sbL + sbR);
    int16_t tpy = (int16_t)(yo * PI_H / PH);
    tpy = constrain(tpy, 0, PI_H - 1);
    if (px > 0)
      tft.drawLine(px0 + px - 2, py0 + prev_py,
                   px0 + px,     py0 + tpy, COL_CH2);
    prev_py = tpy;
  }
}

// Zero line (dashed horizontal at mid-plot)
static void draw_zero_line(Adafruit_ILI9341 &tft, int16_t px0, int16_t py0) {
  int16_t zy = (int16_t)(38.0f * PI_H / PH); // ~46 px
  for (int16_t x = 0; x < PI_W; x += 5)
    tft.drawPixel(px0 + x, py0 + zy, COL_INK_MUTE);
}

// Noise trace — LOCKED / HOLD
static void draw_noise_trace(Adafruit_ILI9341 &tft,
                              int16_t px0, int16_t py0,
                              float amp_norm,
                              uint16_t color,
                              uint32_t seed) {
  // amp_norm in original 0..PH space
  float mid = 40.0f;
  int16_t prev_py = (int16_t)(mid * PI_H / PH);
  uint32_t s = seed;
  for (int16_t px = 0; px <= PI_W; px += 2) {
    s = lcg(s);
    float rnd  = (int32_t)(s >> 16) / 32768.0f - 1.0f;  // -1..1
    float xo   = px * PW / (float)PI_W;
    float yo   = mid + rnd * amp_norm * 0.5f + sinf(xo / 9.0f) * amp_norm * 0.12f;
    int16_t tpy = (int16_t)(yo * PI_H / PH);
    tpy = constrain(tpy, 0, PI_H - 1);
    if (px > 0)
      tft.drawLine(px0 + px - 2, py0 + prev_py,
                   px0 + px,     py0 + tpy, color);
    prev_py = tpy;
  }
}

// Railing trace — RELOCK / fault
static void draw_rail_trace(Adafruit_ILI9341 &tft,
                             int16_t px0, int16_t py0,
                             uint32_t seed) {
  float y = 40.0f;
  uint32_t s = seed ^ 0xCAFEBABEUL;
  int16_t prev_py = (int16_t)(y * PI_H / PH);
  for (int16_t px = 0; px <= PI_W; px += 2) {
    s = lcg(s);
    float r1 = (int32_t)(s >> 16) / 32768.0f - 1.0f;
    s = lcg(s);
    float r2 = (s >> 16) / 65535.0f;
    y += r1 * 17.0f;
    if (r2 > 0.78f) y = (s & 0x8000) ? 8.0f : (PH - 8.0f);
    int16_t tpy = (int16_t)(y * PI_H / PH);
    if (tpy < 0) tpy = 0;
    if (tpy >= PI_H) tpy = PI_H - 1;
    if (px > 0)
      tft.drawLine(px0 + px - 2, py0 + prev_py,
                   px0 + px,     py0 + tpy, COL_ST_FAULT);
    prev_py = tpy;
  }
}

// ============================================================
// Channel panel sub-elements
// ============================================================

static void draw_state_badge(Adafruit_ILI9341 &tft,
                              LockState s,
                              int16_t bx, int16_t by) {
  const char* lbl = state_label(s);
  uint16_t sc = state_color(s);
  int16_t bw = (int16_t)(strlen(lbl) * 6 + 16);
  int16_t bh = 12;
  tft.fillRoundRect(bx, by, bw, bh, 2, COL_PANEL2);
  tft.drawRoundRect(bx, by, bw, bh, 2, color_dim(sc, 110));
  tft.fillCircle(bx + 5, by + 6, 2, sc);
  tft.setTextColor(sc);
  tft.setTextSize(1);
  tft.setCursor(bx + 10, by + 2);
  tft.print(lbl);
}

static void draw_stat_cell(Adafruit_ILI9341 &tft,
                            int16_t cx, int16_t cy,
                            const char* key,
                            const char* val,
                            const char* unit,
                            bool accent,
                            uint16_t accent_col) {
  tft.fillRect(cx, cy, CELL_W, CELL_H, COL_PANEL2);
  tft.drawRect(cx, cy, CELL_W, CELL_H, COL_RULE);

  // Key label — top
  tft.setTextColor(COL_INK_MUTE);
  tft.setTextSize(1);
  tft.setCursor(cx + 4, cy + 3);
  tft.print(key);

  // Value — middle (size 2 = 12×16 per char)
  uint16_t vcol = accent ? accent_col : COL_INK;
  tft.setTextColor(vcol);
  tft.setTextSize(2);
  tft.setCursor(cx + 3, cy + 13);
  tft.print(val);

  // Unit appended at size 1
  if (unit && unit[0]) {
    tft.setTextColor(COL_INK_DIM);
    tft.setTextSize(1);
    // position: after last char of val at size 2
    int16_t ux = cx + 3 + (int16_t)(strlen(val) * 12);
    tft.setCursor(ux, cy + 14);
    tft.print(unit);
  }
}

static void draw_health_bar(Adafruit_ILI9341 &tft,
                             int16_t hx, int16_t hy,
                             float qual,
                             uint16_t sc) {
  tft.fillRect(hx, hy, STATS_W, HEALTH_H, COL_PANEL2);
  tft.drawRect(hx, hy, STATS_W, HEALTH_H, COL_RULE);

  // "SERVO" label
  tft.setTextColor(COL_INK_MUTE);
  tft.setTextSize(1);
  tft.setCursor(hx + 4, hy + 3);
  tft.print("SERVO");

  // Bar track
  int16_t bx = hx + 42;
  int16_t by = hy + 4;
  int16_t bw = 90;
  int16_t bh = 6;
  tft.fillRect(bx, by, bw, bh, COL_SCREEN);
  tft.drawRect(bx, by, bw, bh, COL_RULE);

  // Fill
  int16_t fill_w = (int16_t)(qual * bw);
  if (fill_w > 0)
    tft.fillRect(bx, by, fill_w, bh, sc);

  // Percentage text
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)(qual * 100.0f));
  tft.setTextColor(sc);
  tft.setTextSize(1);
  tft.setCursor(bx + bw + 4, hy + 3);
  tft.print(buf);
}

// ============================================================
// Top status bar
// ============================================================

static void draw_topbar(Adafruit_ILI9341 &tft, bool pll_lock, float refclk_mhz) {
  tft.fillRect(0, 0, 320, TOP_H, COL_BG);
  tft.drawFastHLine(0, TOP_H - 1, 320, COL_RULE);

  // PLL dot
  uint16_t dot_col = pll_lock ? COL_ST_LOCK : COL_ST_ACQ;
  tft.fillCircle(9, 11, 3, dot_col);

  // "CLK PLL" / "CLK UNLK"
  tft.setTextColor(COL_INK_DIM);
  tft.setTextSize(1);
  tft.setCursor(16, 7);
  tft.print(pll_lock ? "CLK PLL" : "CLK UNLK");

  // Refclk
  char buf[16];
  snprintf(buf, sizeof(buf), "%.3f MHz", refclk_mhz);
  tft.setCursor(90, 7);
  tft.print(buf);

  // Right: "PDH" branding
  tft.setTextColor(COL_CH1);
  tft.setCursor(278, 7);
  tft.print("PDH");
}

// ============================================================
// Full channel panel draw
// ============================================================

static uint32_t disp_frame = 0;

static void draw_channel(Adafruit_ILI9341 &tft,
                          int16_t cy,
                          int16_t ch_idx,
                          LockState state,
                          const DispChannel &d) {
  uint16_t accent  = (ch_idx == 0) ? COL_CH1      : COL_CH2;
  uint16_t fill    = (ch_idx == 0) ? COL_CH1_FILL : COL_CH2_FILL;
  uint16_t sc      = state_color(state);
  uint32_t seed    = disp_frame ^ (ch_idx ? 0xDEADBEEFUL : 0xBAADF00DUL);

  // --- Rail ---
  tft.fillRect(0, cy, RAIL_W, CH_H, accent);

  // --- Channel background ---
  tft.fillRect(RAIL_W, cy, 320 - RAIL_W, CH_H, COL_PANEL);

  // Divider between channels
  if (ch_idx == 0)
    tft.drawFastHLine(0, cy + CH_H - 1, 320, COL_RULE);

  // --- Plot outer border ---
  tft.drawRect(PLOT_X, cy + 4, PLOT_W, PLOT_H, COL_RULE);

  // --- Plot inner background ---
  int16_t px0 = PLOT_X + 1;
  int16_t py0 = cy + 5;
  tft.fillRect(px0, py0, PI_W, PI_H, COL_SCREEN);

  // --- Grid ---
  draw_grid(tft, px0, py0);

  // --- Trace ---
  switch (state) {
    case LOCK_IDLE:
      // Flat dashed baseline
      for (int16_t x = 0; x < PI_W; x += 5)
        tft.drawPixel(px0 + x, py0 + (int16_t)(72.0f * PI_H / PH), COL_INK_MUTE);
      break;

    case LOCK_SEARCH:
      draw_lorentzian(tft, px0, py0, d.peak_phase, true, accent, fill);
      // Scan marker
      {
        int16_t mx = (int16_t)(d.scan_phase * PI_W);
        tft.drawFastVLine(px0 + mx, py0, PI_H, sc);
      }
      break;

    case LOCK_ACQUIRE:
      // Transmission + S-curve overlay
      draw_lorentzian(tft, px0, py0, d.peak_phase, false, accent, fill);
      draw_zero_line(tft, px0, py0);
      draw_error_curve(tft, px0, py0, d.peak_phase);
      // Setpoint vertical marker
      {
        int16_t spx = (int16_t)(d.peak_phase * PI_W);
        tft.drawFastVLine(px0 + spx, py0, PI_H, COL_ST_LOCK);
        tft.fillCircle(px0 + spx, py0 + (int16_t)(BASE * PI_H / PH), 3, COL_ST_LOCK);
      }
      break;

    case LOCK_LOCKED:
      draw_zero_line(tft, px0, py0);
      draw_noise_trace(tft, px0, py0, 3.0f, sc, seed);
      break;

    case LOCK_HOLD:
      draw_zero_line(tft, px0, py0);
      draw_noise_trace(tft, px0, py0, 10.0f, sc, seed);
      break;

    case LOCK_RELOCK:
      draw_zero_line(tft, px0, py0);
      draw_rail_trace(tft, px0, py0, seed);
      break;
  }

  // --- CH# label (top-left overlay) ---
  tft.setTextColor(accent);
  tft.setTextSize(2);
  tft.setCursor(px0 + 4, py0 + 3);
  tft.print(ch_idx == 0 ? "CH1" : "CH2");

  // --- State badge (top-right) ---
  const char* lbl = state_label(state);
  int16_t bw = (int16_t)(strlen(lbl) * 6 + 16);
  draw_state_badge(tft, state, px0 + PI_W - bw - 3, py0 + 3);

  // --- Axis label (bottom-left) ---
  tft.setTextColor(COL_INK_MUTE);
  tft.setTextSize(1);
  tft.setCursor(px0 + 3, py0 + PI_H - 10);
  tft.print(state_axis(state));

  // --- Live value (bottom-right) ---
  {
    char vbuf[16] = "";
    if (state == LOCK_SEARCH) {
      // Show scan position as a percentage
      snprintf(vbuf, sizeof(vbuf), "%.0f%%", d.scan_phase * 100.0f);
    } else if (state == LOCK_LOCKED || state == LOCK_HOLD) {
      snprintf(vbuf, sizeof(vbuf), "+/-%.4f", d.err_rms);
    }
    if (vbuf[0]) {
      tft.setTextColor(accent);
      tft.setTextSize(1);
      // right-align: each char=6px
      int16_t vx = px0 + PI_W - (int16_t)(strlen(vbuf) * 6) - 3;
      tft.setCursor(vx, py0 + PI_H - 10);
      tft.print(vbuf);
    }
  }

  // ---- Stats area ----
  char s1[12], s2[12], s3[12], s4[12];

  // TEC TEMP
  if (isnan(d.tec_temp_c))
    strncpy(s1, "---", sizeof(s1));
  else
    snprintf(s1, sizeof(s1), "%.2f", d.tec_temp_c);

  // LASER I
  snprintf(s2, sizeof(s2), "%.1f", d.laser_i_ma);

  // SETPOINT
  snprintf(s3, sizeof(s3), "%.1f", d.setpoint_pct);

  // ERR RMS
  snprintf(s4, sizeof(s4), "%.4f", d.err_rms);

  draw_stat_cell(tft, STATS_X,               cy + ROW0_OFF, "TEC TEMP", s1, "C",  false, accent);
  draw_stat_cell(tft, STATS_X + CELL_W + CELL_GAP, cy + ROW0_OFF, "LASER I",  s2, "mA", false, accent);
  draw_stat_cell(tft, STATS_X,               cy + ROW1_OFF, "SETPOINT", s3, "%",  true,  accent);
  draw_stat_cell(tft, STATS_X + CELL_W + CELL_GAP, cy + ROW1_OFF, "ERR RMS",  s4, "",   false, accent);

  draw_health_bar(tft, STATS_X, cy + HLTH_OFF, d.lock_qual, sc);
}

// ============================================================
// Public API
// ============================================================

void display_init(Adafruit_ILI9341 &tft) {
  tft.begin();
  tft.setRotation(1);  // landscape
  tft.fillScreen(COL_BG);

  // Splash
  tft.setTextColor(COL_ST_LOCK);
  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.print("DUAL-PDH v1.0");
  tft.setTextColor(COL_INK_DIM);
  tft.setTextSize(1);
  tft.setCursor(100, 125);
  tft.print("Initialising...");
}

void display_update(Adafruit_ILI9341 &tft, const DispModel &m) {
  disp_frame++;

  draw_topbar(tft, m.pll_lock, m.refclk_mhz);
  draw_channel(tft, CH1_Y, 0, m.state[0], m.ch[0]);
  draw_channel(tft, CH2_Y, 1, m.state[1], m.ch[1]);
}
