"""
Capture web dashboard screenshots for the HTML documentation.
Requires: playwright  (pip install playwright && playwright install chromium)
Run from the docs/ directory:  python capture_screenshots.py
"""
import asyncio, json, time
from pathlib import Path
from playwright.async_api import async_playwright

URL   = "http://localhost:5501/dashboard_preview.html"
OUTDIR = Path(__file__).parent / "img"
OUTDIR.mkdir(exist_ok=True)

# Mock datasets for different scenarios
MOCK_BASE = {
    "t": 45231000, "pll": 1, "refclk": 25.000, "dither": 10000,
    "ip": "192.168.1.47", "demo": 1,
    "net": {"dhcp": 1, "sip": "192.168.1.200",
            "sub": "255.255.255.0", "gw": "192.168.1.1", "host": "dual-pdh"},
}

CH_LOCKED = {"n": 1, "st": "LOCKED",  "rms": 0.00312, "temp": 24.52,
             "ima": 138.2, "setp": 52.30, "scan": 0.0, "peak": 0.614,
             "qual": 0.94, "relk": 0,
             "kp": 0.08, "ki": 0.5, "kd": 0.0, "lthr": 0.02, "athr": 0.10}
CH_SEARCH = {"n": 2, "st": "SEARCH",  "rms": 0.42100, "temp": 23.87,
             "ima": 138.0, "setp": 43.00, "scan": 0.347, "peak": 0.600,
             "qual": 0.00, "relk": 0,
             "kp": 0.08, "ki": 0.5, "kd": 0.0, "lthr": 0.02, "athr": 0.10}
CH_ACQUIRE = {"n": 1, "st": "ACQUIRE", "rms": 0.08500, "temp": 24.20,
              "ima": 140.8, "setp": 51.00, "scan": 0.0, "peak": 0.614,
              "qual": 0.61, "relk": 0,
              "kp": 0.08, "ki": 0.5, "kd": 0.0, "lthr": 0.02, "athr": 0.10}
CH_RELOCK  = {"n": 2, "st": "RELOCK",  "rms": 0.54200, "temp": 24.50,
              "ima": 138.0, "setp": 50.00, "scan": 0.0, "peak": 0.600,
              "qual": 0.00, "relk": 3,
              "kp": 0.08, "ki": 0.5, "kd": 0.0, "lthr": 0.02, "athr": 0.10}
CH_LOCKED2 = {**CH_LOCKED, "n": 2, "temp": 23.91, "ima": 139.2,
              "setp": 48.60, "peak": 0.580, "rms": 0.00298, "qual": 0.96}
CH_HOLD    = {"n": 1, "st": "HOLD",    "rms": 0.03200, "temp": 24.55,
              "ima": 138.5, "setp": 52.30, "scan": 0.0, "peak": 0.614,
              "qual": 0.50, "relk": 0,
              "kp": 0.08, "ki": 0.5, "kd": 0.0, "lthr": 0.02, "athr": 0.10}

SCENARIOS = [
    ("web-01-main.png",       {**MOCK_BASE, "ch": [CH_LOCKED,  CH_SEARCH]},  None,           1400),
    ("web-02-both-locked.png",{**MOCK_BASE, "ch": [CH_LOCKED,  CH_LOCKED2]}, None,           1400),
    ("web-03-acquire.png",    {**MOCK_BASE, "ch": [CH_ACQUIRE, CH_SEARCH]},  None,           1400),
    ("web-04-relock.png",     {**MOCK_BASE, "ch": [CH_HOLD,    CH_RELOCK]},  None,           1400),
    ("web-05-demo-off.png",   {**MOCK_BASE, "demo": 0,
                               "ch": [CH_LOCKED, CH_LOCKED2]},               None,           1400),
    ("web-06-settings.png",   {**MOCK_BASE, "ch": [CH_LOCKED,  CH_LOCKED2]}, "settings",     1600),
    ("web-07-network.png",    {**MOCK_BASE, "ch": [CH_LOCKED,  CH_LOCKED2]}, "netsettings",  1600),
]

INJECT = "window.__MOCK__ = {mock};"

async def capture():
    async with async_playwright() as pw:
        browser = await pw.chromium.launch()
        for fname, mock, open_details, height in SCENARIOS:
            page = await browser.new_page(viewport={"width": 1200, "height": height})
            await page.route("**/api/**", lambda r: r.abort())
            await page.goto(URL, wait_until="domcontentloaded")
            # Inject mock fetch override
            await page.evaluate(INJECT.format(mock=json.dumps(mock)))
            # Wait for channels to render
            await page.wait_for_selector("#ch1 .ch-rail", timeout=5000)
            await asyncio.sleep(0.8)  # let animations settle
            # Open accordion if requested
            if open_details:
                el = await page.query_selector(f"#{open_details}")
                if el:
                    await el.evaluate("el => el.setAttribute('open', '')")
                    await asyncio.sleep(0.4)
            path = str(OUTDIR / fname)
            await page.screenshot(path=path, full_page=False)
            print(f"  saved {fname}")
            await page.close()
        await browser.close()
    print("Done.")

asyncio.run(capture())
