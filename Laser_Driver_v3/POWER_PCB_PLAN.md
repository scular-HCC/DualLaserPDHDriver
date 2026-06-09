# Power PCB — Plan, Power Budget, Trace Plan & Component Verification

Board: `laser_driver_v3_Power.kicad_pcb` (project `laser_driver_v3_Power`)
Date: 2026-06-09 · supersedes prior plan · based on the as-drawn schematic
Assumptions are stated inline; **all currents/widths are calculated, verify against
measured loads before fabrication.**

---

## 1. Power architecture (as drawn)

Two toroidal transformers → two rectifier paths → linear pre-regs → LT304x LDOs:

| Transformer | Rating | Secondary used | Feeds |
|---|---|---|---|
| **T1 VPM30-1670** | 50 VA | **±15 VAC CT @ 1.67 A** | D1 (GBU4M) → **±22 V raw** → all linear rails |
| **T2 VPT18-8800** | 160 VA | **±9 VAC CT @ 8.8 A** | D7 (GBU1506) → cap-mult → **±6V_TEC** |

Mains entry: 120 VAC → **TE Connectivity PEM 7-6609940-4** (power-entry module, **2 A slow-blow** fuse integrated; ~0.8 A continuous) → **TH1** (inrush) → primaries. (Discrete F1 holder removed — fusing is now in the PEM.)

### Rail tree and load budget
| Rail | Chain | Load (assumed) |
|---|---|---|
| +15 V | +22V → U8 TPS7A39(+) → R14 → U9 LT3042 | **300 mA** |
| −15 V | −22V → U8 TPS7A39(−) → R15 → U10 LT3094 | **150 mA** |
| +5V_LASER | +22V → U13 7812 → U14 7808 → R36 → U15‖U16 LT3045 | **600 mA** |
| +5 V (analog) | +22V → U47 7808 → U49 LT3045 | ~150 mA |
| −5 V | −22V → U48 7905 → U50 LT3094 | ~100 mA |
| 5Vref | +22V → U1 7808 → U2 LT3042 → U3 LTC6655-5 | ~15 mA |
| 3V3 | +22V → U4 7812 → U5 7805 → R12 → U6 LT3045 | ~200 mA |
| 1V8 | 3V3 → U7 LP5907 | ~50 mA |
| **±6V_TEC** | ±9VAC → D7 → cap-mult Q3/Q4 → pass Q7/Q8(PNP)/Q9/Q10(NPN) | **±2.5 A** (per H-bridge; up to ~5 A if both TEC channels cool together — **confirm**) |

**Derived raw-rail currents:** +22 V ≈ **1.3 A**, −22 V ≈ **0.25 A**, ±6V_TEC raw ≈ **2.5–5 A**.

---

## 2. Component power verification

ΔT/heatsink figures assume still air. **Bold = action required.**

| Part | Rating | Calculated stress | Verdict |
|---|---|---|---|
| T1 VPM30-1670 | 50 VA / 1.67 A·CT | ~33 VA, +side ~1.3 A | OK (~66 %) |
| T2 VPT18-8800 | 160 VA / 8.8 A·CT | ~30–60 VA, ~2.5–5 A | OK (large margin) |
| Mains fuse (TE PEM 7-6609940-4) | **2 A slow-blow** | primary ≈ 0.8 A + inrush | OK (~2.5× margin) ✓ |
| D1 GBU4M | 4 A | 1.5 A avg (≈2.7 A RMS) | OK (~37 %) |
| D7 GBU1506 | 15 A | ≤5 A avg | OK |
| **U13 MC7812** | TO-220 (~2 W no-HS) | (22−12)×0.6 = **6.0 W** | **HEATSINK MANDATORY** |
| U14 MC7808 | TO-220 | (12−8)×0.6 = 2.4 W | heatsink |
| U47/U1 MC7808 | TO-220 | (22−8)×0.15 = ~2.1 W | heatsink |
| U4 MC7812 | TO-220 | (22−12)×0.25 = 2.5 W | heatsink |
| U5 MC7805 | TO-220 | (12−5)×0.25 = 1.75 W | heatsink |
| U48 LM7905 | TO-220 | (22−5)×0.1 = 1.7 W | small heatsink |
| **Q7–Q10 MJL21193/4** | TO-264 (~200 W) | (12.7−6)×2.5 ≈ **16.7 W/rail** (8–17 W/device) | **BIG HEATSINKS MANDATORY** |
| U15/U16 LT3045 | MSOP-12-EP (~40 °C/W w/vias) | (8−5)×0.3 = 0.9 W ea | OK (~36 °C rise) |
| U9 LT3042 / U10 LT3094 | MSOP-EP | (16.5−15)×0.3 = 0.45 W | OK |
| U49/U50 LT304x | MSOP-12-EP | ≤0.5 W | OK |
| R20/R21, R2/R3 (15 Ω) | **must be wire-wound** | ~1.7 W cont + inrush surge | **WW 10 W (in progress)** |
| Electrolytics on ±22 V | **≥35 V required** | 22 V working | **VERIFY each cap's V rating** |
| Electrolytics on ±6V_TEC raw | ≥25 V | ~12.7 V | verify |

**Total board dissipation ≈ 45–60 W**, dominated by the **±6V_TEC linear pass (~33 W)** and the **+5V_LASER 78xx chain (~10 W)**. The design is **fully linear by choice** (noise priority — confirmed), so this heat is accepted and **thermal management is mandatory, not optional**. This is the #1 layout constraint.

---

## 3. Trace plan (IPC-2221, 1 oz outer copper, ΔT = 10 °C)

Width targets (use 2 oz or copper pours to halve where noted):

| Net / path | Current | Min width @1 oz | Strategy |
|---|---|---|---|
| **Mains 120 VAC (F1/TH1/primaries)** | ~0.8 A | 0.25 mm electrically | **creepage/clearance dominates** (≥2.5 mm spacing, slots) |
| T1 sec ±15 VAC → D1 | ~2.7 A RMS | ~0.8 mm | short loop to D1 + reservoir |
| **±22 V raw bus** (+side) | ~1.5 A | ~0.5 mm | star from reservoir to each pre-reg |
| −22 V raw bus | ~0.3 A | 0.2 mm | |
| **T2 sec ±9 VAC → D7** | **~9 A RMS** | **≥2.5 mm / pour (or 2 oz)** | tightest loop; highest current on board |
| **±6V_TEC raw (cap-mult/reservoir)** | ~2.5–5 A | **1.5–2.5 mm / pour** | wide, tight reservoir loop |
| **±6V_TEC → inter-board connector** | 2.5 A (≤5 A) | **0.8–1.5 mm / pour** | heavy MSTB pins to main board |
| +5V_LASER out | 0.6 A | 0.2 mm | |
| +15 V / −15 V / +5 V / 3V3 out | 0.15–0.3 A | 0.15–0.2 mm | |
| Pre-reg → LDO drop resistors (R12/14/15/36) | per-rail | match rail width | keep short, away from precision |
| GND | return-current sum | **solid pour, split AGND/PGND** | TEC return separate from reference |

**Reservoir-cap loops** (both bridges): keep bridge→cap→cap-mult loop area minimal and wide — these carry the high RMS ripple current, not just the DC average.

---

## 4. Placement / layout plan

- **4-layer**, **2 oz outer** recommended (TEC + bridge currents + heat spreading). 2-layer only if pours are generous and TO-220/TO-264 get external sinks.
- **Mains zone** (F1, TH1, T1, T2, D1, D7) isolated at one edge with proper creepage; line/neutral/earth spacing per IEC; fuse first.
- **Heat row:** all TO-220 pre-regs + TO-264 TEC pass transistors along a board edge/heatsink rail, away from the LT304x LDOs and the 5Vref/LTC6655.
- **±6V_TEC stage** (cap-mult + pass BJTs + reservoir) as a tight high-current island next to the inter-board connector.
- **Quiet corner:** 5Vref (U3 LTC6655) + its LT3042 (U2), away from switching/heat.
- Inter-board connector along the edge facing the main board; ±6V_TEC on the heavy block.

---

## 5. Flags / issues (priority order)

1. ~~F1 undersized~~ — **RESOLVED:** mains fusing moved to the TE Connectivity PEM 7-6609940-4 (**2 A slow-blow**, ~2.5× margin over the 0.8 A continuous draw). Touch-safe integrated inlet.
2. **Thermal (linear design):** ~55–65 W total worst case, hotspots **U13 (6 W)** and **TEC pass (≈25 W)**. **Cooling method = forced convection (small fan)** — see §5b: SK 100 internal heatsinks + a 60 mm ≥20 CFM fan, Tj ≈ 74–75 °C. **Add fan-fail/over-temp protection** (the new top open task).
3. **Confirm simultaneous TEC loading** — if both H-bridges can cool at once, ±6V_TEC raw is ~5 A and the pass dissipation doubles (~33 W/rail). Drives transformer headroom, trace width, and heatsink size.
4. **Fully-linear architecture confirmed** (noise priority). No switching pre-reg. Consequence: budget the enclosure/heatsinking for ~45–60 W (worst case more if both TECs cool together) — see #2/#3.
5. **Verify all ±22 V-rail electrolytic voltage ratings ≥ 35 V** (22 V working + margin). Check each cap in the BOM.
6. **U10 / U50 (LT3094) symbol vs part** — earlier flag; confirm the LT3094 symbol pinout now matches the real negative-LDO part (U10's −15 V input correctly ties to U8 OUTN — verified — but re-confirm SET/ILIM/PGFB per the LT3094 datasheet, not the LT3042 layout).
7. **R20/R21 and R2/R3** must be the wire-wound parts (in progress), not 0805 — ~1.7 W continuous + inrush surge.
8. **Add reverse-protection diodes** across the linear regs (output→input for 78xx, input→output for 79xx/LT3094) given the downstream capacitance — esp. U48→U50 path.

---

## 5a. Heatsink calculation (base θsa numbers; cooling = forced convection — see §5b)

**Model:** Tj = Ta + P·(θjc + θcs + θsa). **Design point:** Ta = 45 °C (vented enclosure),
**Tj ≤ 110 °C** (60–65 °C below the silicon limit), insulated mount **θcs ≈ 1 °C/W**.
Required sink: **θsa ≤ (110 − 45)/P − θjc − θcs**.

### Individual TO-220 regulators (θjc ≈ 5 °C/W, no-sink θja ≈ 62 °C/W)
| Reg | P | Tj if no sink | θsa required | Heatsink |
|---|---|---|---|---|
| **U13 MC7812** | 6.0 W | 412 °C ✗ | **≤ 4.0 °C/W** | moderate (e.g. Fischer SK104-25 ≈ 6 °C/W is too small → ~3–4 °C/W extrusion, or shared rail) |
| U4 MC7812 | 2.5 W | 200 °C ✗ | ≤ 18 °C/W | small clip-on |
| U14 MC7808 | 2.4 W | 194 °C ✗ | ≤ 19 °C/W | small clip-on |
| U47 MC7808 | 2.1 W | 175 °C ✗ | ≤ 22 °C/W | small clip-on |
| U5 MC7805 | 1.75 W | 158 °C ✗ | ≤ 28 °C/W | small clip-on |
| U48 LM7905 | 1.7 W | 155 °C ✗ | ≤ 29 °C/W | small clip-on (**tab = Vin, isolate**) |
| U1 MC7808 | 0.28 W | 67 °C ✓ | none | none needed |

### TEC pass stage — TO-264 (θjc ≈ 0.7 °C/W) + cap-mult FETs
Per-rail dissipation is fixed at **(V_raw−6)·I ≈ 5 V × 2.5 A = 12.5 W** (single channel),
**25 W** if both TEC channels cool at once. Mount each rail's devices (Q3+Q9/Q10 on +6V,
Q4+Q7/Q8 on −6V) on a **shared sink**; only one rail is active at a time, so size for 25 W.
- Shared-sink requirement (hottest device 12.5 W on it): 110 = 45 + 25·θsa + 12.5·1.0
  → **θsa ≤ 2.1 °C/W** per rail sink (or one common sink for all four, sized 25 W).

### Cap-mult FETs on ±22 V (TO-220)
| FET | P | θsa required | Heatsink |
|---|---|---|---|
| Q1 IRF540 (+22 V) | ~4.5 W (1.5 A × ~3 V) | ≤ 11 °C/W | small/moderate (isolate — tab = drain) |
| Q2 IRF9540 (−22 V) | ~0.5 W | none | Tj ≈ 81 °C no-sink ✓ |

### Recommended grouped heatsinking
1. **Regulator rail** — U13, U4, U14, U47, U5, Q1 on a **common extrusion ≈ 1.2 °C/W**
   (e.g. ~150 mm of Fischer SK85/SK88-class, or chassis-mounted Al bar). Shared-sink check
   (Σ≈19 W, hottest U13 6 W): 45 + 19·1.2 + 6·6 = 104 °C ✓. **Insulate all** (78xx tab = GND,
   Q1 tab = drain — different potentials), or keep GND-tab 78xx uninsulated on a grounded sink
   and isolate Q1.
2. **TEC pass heatsink** — Q3/Q4 + Q7–Q10 on a **≈ 1.5–2 °C/W** extrusion / chassis wall
   (handles 25 W worst case). This is the largest sink.
3. **U48 (LM7905) + Q2** — small isolated clip-on each (tab potentials differ from #1).

### Passive feasibility
**Total heat ≈ 40 W typical, ≈ 55 W worst (both TECs cooling).** Passive (no fan) **is
achievable** but only if the heat reaches ambient:
- Mount sinks #1 and #2 to the **enclosure rear panel/chassis with fins external**, OR use a
  **vented enclosure** (bottom + top slots, **fins vertical** for chimney convection).
- A **sealed compact box will not** shed ~50 W passively — internal Ta climbs, Tj follows.
- Keep the LT304x LDOs, 5Vref/LTC6655 away from the hot sinks (thermal gradient → reference drift).

**Verdict:** passive is feasible with (a) the two large sinks above (~1.2 and ~1.5–2 °C/W),
(b) chassis/external coupling or good venting, (c) vertical fin orientation. If the enclosure
must be small/sealed, a low-speed fan would be required — but the sizing above avoids it.

> θjc values assumed: MC78xx ≈ 5 °C/W, LM7905 ≈ 5 °C/W, MJL21193/4 ≈ 0.7 °C/W, IRF540 ≈ 1.5 °C/W.
> Verify against datasheets; re-run if Ta or the both-TEC-channel assumption changes.

## 5b. Commercial heatsink BOM (passive, natural convection, Ta = 45 °C)

θsa values are typical natural-convection figures — confirm on the vendor datasheet. All
power tabs insulated (Sil-Pad/mica + shoulder washer); θcs ≈ 1 °C/W included.

### A. Small/medium TO-220 regulators — PCB clip heatsinks (Fischer SK 104 family)
SK 104 footprint = **34.9 mm (W) × 12.7 mm (D)**, height = the suffix. (θsa natural convection.)
| Device(s) | P | Heatsink | Size (W×D×H) | θsa | Tj |
|---|---|---|---|---|---|
| U5, U14, U47, U48 | 1.7–2.4 W | **SK 104 25,4 STS** | 34.9 × 12.7 × **25.4 mm** | ~19 K/W | 88–105 °C ✓ |
| U4 | 2.5 W | **SK 104 38,1 STS** | 34.9 × 12.7 × **38.1 mm** | ~13 K/W | ~95 °C ✓ |
| (margin option) | — | **SK 104 50,8 STS** | 34.9 × 12.7 × **50.8 mm** | **9 K/W** | cooler |
| U1 | 0.28 W | none | — | — | ~67 °C ✓ |
| Q2 | 0.5 W | none | — | — | ~81 °C ✓ |

Board keep-out per clip: ~**35 × 13 mm**, height 25–51 mm. (U48 LM7905 tab = −22 V — **insulate**.)

### B. High-power devices — extruded heatsinks (Fischer SK 100 profile = 66 × 40 mm cross-section)
**Cooling method: FORCED CONVECTION (small fan).** With ~2 m/s over the fins the SK 100 θsa
drops to ~0.45× its natural value, so the sinks shrink and mount **internally** in the airflow.
| Group | Devices | Heat | Heatsink | Size (W×H×L) | θsa (forced ~2 m/s) | Tj |
|---|---|---|---|---|---|---|
| **TEC pass** | Q3/Q4 + Q7–Q10 (6 dev) | **25 W (both TECs)** | **Fischer SK 100/75 SA** | **66 × 40 × 75 mm** | ~0.85 K/W | ~74 °C ✓ |
| **Reg rail** | U13·U4·U14·U47·U5·U48·Q1·**Q2** (8× TO-220) | ~21.7 W | **Fischer SK 100/125 SA** | **66 × 40 × 125 mm** | ~1.0 K/W | ~98 °C ✓ (U13 hottest) |

(Ta = 40 °C downstream air. Shared-rail check, U13 6 W: 40 + 21.7×1.0 + 6×6 = **98 °C ✓**.
All eight TO-220s now share **one** SK 100/125 rail — this supersedes the individual SK 104
clips of §A. **Insulate every tab** — 78xx = GND, U48 = −5 in, Q1/Q2 = ±22 drain. **U13 must be
on the rail, not a clip.** Q2 added for thermal symmetry/margin vs Q1.)

### Fan specification
Remove ~**60–65 W** from the box at an air-temp rise ΔT ≈ 12 °C:
`CFM_eff = 1.76·P/ΔT = 1.76·65/12 ≈ 9.5 CFM` through the box. With ~50 % system derate (box +
fin impedance) → **fan rated ≥ ~20 CFM free-air**.
- **60 mm DC fan, ≥20 CFM, 12 V, low-noise** (sleeve/FDB), e.g. Sunon MF60252V-, Noctua NF-A6x25,
  or Delta AFB0612. Run at moderate/low speed for quiet operation (margin is large at full speed).
- **Airflow path:** intake at the cool/quiet end (5Vref, LDOs) → across the **TEC-pass and
  reg-bank heatsinks** → exhaust. Place the fan as **exhaust** pulling air over the hot sinks.
- **Power:** 12 V tap (e.g. off a +12 V pre-reg node) or a dedicated small fan supply.

### ⚠ Fan-failure protection (now a single point of failure)
With forced convection the fan is a reliability item — on fan loss the box heats and the TEC-pass
junctions climb fast. Mitigate:
- T1 (VPM30-1670) already has a **140 °C self-resetting primary thermal switch** — partial backup.
- Add an **over-temp shutdown** on the TEC-pass heatsink (thermistor/thermostat → disable ±6V_TEC
  or the TEC drive) and/or a **fan-tach/alarm**. Recommended given the ~25 W there.

### Mounting hardware (per insulated device)
- TO-220: Bergquist **SP400-0.009-00-1010** Sil-Pad + nylon shoulder washer + M3.
- TO-264: Bergquist **Sil-Pad TSP 1500** (or mica + grease) + M3/M4, ≥ the device area.

**Net result:** every junction ≤ ~110 °C at Ta = 45 °C — within the safe zone — using only
passive heatsinks, provided the two extruded/chassis sinks couple to ambient (external fins
or vented enclosure, fins vertical).

## 6. Next steps
1. **Resolved:** fuse (2 A SB in TE PEM); fully-linear architecture; 3V3/+5V load estimates accepted.
2. **Owner — still open:** TEC simultaneous-load question (#3); heatsinking scheme for U13 + the TO-264 TEC pass transistors (#2); ±22 V cap voltage ratings (#5).
3. Re-sync PCB; apply trace widths / net classes from §3; pour GND with AGND/PGND split.
4. Re-verify after any rail/load change (cascades into transformer & heatsink sizing).
