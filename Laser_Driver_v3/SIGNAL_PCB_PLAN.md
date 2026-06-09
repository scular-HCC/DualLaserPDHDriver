# Main Laser Driver (Signal) PCB — Layout Analysis & Plan

Board: `laser_driver_v3.kicad_pcb` (project `laser_driver_v3`)
Date: 2026-06-06 (rev: TEC H-bridge relocated onto this board)
Status: footprints imported, **not yet laid out** (stale outline from the pre-split
monolithic board). Plan for review before any PCB edits. No `.kicad_pcb` changes made.

This is the performance-critical board: it carries the new ~1–3 MHz analog derivative
lock front end (wideband TIA → mixer → loop filter) plus the precision ACC laser drive.

> **CHANGE:** The **TEC H-bridge (`TEC_Control`) now lives on this board** (moved from the
> power board). Consequences: (a) the 8× TO-220 H-bridge FETs + gate drivers + TEC current
> sense are now here; (b) the bipolar TEC drive (−1.5…+2.5 A) to the butterfly TEC stays
> entirely on-board (no longer crosses the connector — good); (c) **±6V_TEC now arrives
> from the power board via the connector at ±2.5 A**; (d) high-current switching now shares
> the board with the precision/RF front end — new thermal + EMC constraints (see §2/§3).
> The `.kicad_pcb` must be re-synced (Update PCB from Schematic) before layout.

---

## 1. Current state

| Item | State |
|---|---|
| Footprints | Nearly complete — only **D_PD1/D_PD2 (FGA01FC)** unassigned. PCB is **stale** (predates the H-bridge move) — needs re-sync. |
| Nets | RF + precision signals + (now) the TEC H-bridge drive/sense/control |
| Placement | Scattered / from old monolithic layout |
| Board outline | **STALE** — old 300×200 mm, 2-layer, AGND/PGND-split outline + zone labels from the monolithic board |
| Routing / vias / zones | none |

Sheets on this board: DFBLaser, Readback, Clock, Laser_Control, PhotoDiodes, PhaseRef,
Teensy, **TEC_Control (H-bridge — newly moved here)**.

Key parts by block:
- **DFBLaser:** 2× butterfly sockets `J_BTF1/2` (PinSocket 2×7), OPA192 ACC error amps
  (U22/U23) + PI (U24/U25), **ADG419 hold-switch (U26/U27)**, IRLML6344 pass FETs (Q19/Q20),
  L1/L2 bias-tee chokes.
- **TEC_Control (newly here):** bipolar **H-bridge, 8× TO-220 FETs** (IRLB8721 ×4 / IRF9540N
  ×4) + gate drivers, R7 = 0.1 Ω TEC sense — drives the butterfly TEC at −1.5…+2.5 A,
  powered from the incoming **±6V_TEC**.
- **Readback:** OPA859 wideband TIA ×2 (U30/U31), OPA380 MPD TIA ×4 (U_TIA1–4),
  INA826 ×4 (laser + TEC current sense).
- **Laser_Control:** AD9833 ×4 (mod + phase-ref), 74AHCT125 (U36), AD5064 quad DAC (U37).
- **Clock:** 10 MHz ref SMA `J2`, LMV7219 comparator, SN74LVC1G3157 mux.
- **PhotoDiodes:** LT3042 LDO (U43), ring PDs **D_PD1/D_PD2 (FGA01FC — footprints TBD)**.
- **PhaseRef:** AD831-class mixers (U45/U46) + RC LPFs — **footprints now assigned**.
- **Teensy:** Teensy 4.1 module `A1`, CR2013-MI2120 TFT `U44`.

---

## 2. Blockers to resolve before layout

1. **TEC H-bridge now shares the precision/RF board (new top concern).** Up to ±2.5 A of
   switched/linear TEC drive plus 8 TO-220 FETs' heat now sit on the same board as the
   OPA859 TIA, AD831 mixer, 5Vref, and ACC loop. Requires: a contained high-current TEC
   island with tight drive loops, hard isolation (distance + ground strategy) from the RF
   front end and reference, and a heatsinking plan for the TO-220s. **This is the single
   biggest layout risk introduced by the move.**

2. **D_PD1/D_PD2 (FGA01FC) footprints unassigned** — the only parts left without
   footprints on this board. Needed before the PD front end can place/route.

3. **Stale board outline + zone text.** The Edge.Cuts outline and `gr_text` zone labels
   describe the old monolithic design (300×200, 2-layer). Remove/redraw for this board —
   now sized to include the TEC H-bridge.

4. **Grounding/interface details:**
   - `+5V_LASER` now matches on both boards (naming reconciled); `-5V` now crosses too.
   - `AGND` is board-internal (not crossing) — still needs a defined single-point AGND↔GND
     tie, with `GND` brought in on the connector.
   - `±6V_TEC` now arrives via the connector at ±2.5 A — size those pins/traces accordingly.

---

## 3. Layout plan (once blockers settled)

### Stackup / fab
- **4-layer strongly recommended** (Sig/RF — GND — PWR — Sig): the old 2-layer note
  predates the RF lock. Solid GND reference under the OPA859 TIA → AD831 mixer path is
  needed for the 1–3 MHz (and 2Ω ≈ 4–5 MHz) signals and for the precision ACC. **Flag:
  this is a change from the documented 2-layer stackup — owner to confirm.**
- **2 oz outer copper** is now warranted: the TEC H-bridge moved here carries ±2.5 A, and
  ±6V_TEC enters at ±2.5 A. (Previously 1 oz when TEC was off-board — that assumption no
  longer holds.) Board size set after re-placement; must now also fit the TEC H-bridge +
  TO-220 thermals.

### Placement zoning (two symmetric channels; energy/signal flow left→right)
- **Optical edge:** butterfly sockets `J_BTF1/2` at one edge for fiber/pigtail access;
  ACC drive (OPA192 + IRLML6344 + Kelvin sense) immediately adjacent to minimize the
  laser-current loop.
- **RF front end (per channel, kept tight & guarded):** ring-PD → OPA859 TIA (U30/U31)
  → AD831 mixer (U45/U46) → RC LPF → loop filter. Short TIA→mixer run, ground guard,
  channel-to-channel shield gap.
- **Modulation source:** AD9833 ×4 + 74AHCT125 near the injection point but isolated
  from the TIA inputs.
- **Precision DC:** AD5064 DAC (U37), 5Vref/VREF_MID, INA826 current sense — quiet
  analog zone, away from DDS clocks and the Teensy.
- **Digital corner:** Teensy 4.1 (A1) + TFT (U44) + Clock block (SMA, comparator, mux)
  grouped on DGND, single-point tie to AGND.
- **TEC H-bridge island (new):** place the 8 TO-220 FETs + drivers as a tight high-current
  block **near the butterfly TEC pins** (short, wide BTF drive loops) and as **far as
  possible from the OPA859 TIA / AD831 mixer / 5Vref**. Bring ±6V_TEC in from the connector
  directly to this island. Keep its switching return currents on a local PGND poured under
  the H-bridge, tied to the system ground at one point — not under the RF/reference zone.
- **Inter-board connector(s)** along the edge facing the power board (see §4); land
  ±6V_TEC closest to the TEC island.

### Two-channel discipline
- Mirror the two channels for matched parasitics; run them at slightly different Ω;
  maintain a guard/shield channel between RF paths to keep modulation crosstalk out of
  the differential beat.

### Grounding
- AGND / DGND split with a defined single-point star tie; keep DDS and Teensy return
  currents off the TIA/mixer reference. Stitch GND pours on the inner layer.

### Net classes / trace widths
| Class | Nets | Note |
|---|---|---|
| **TEC power (new)** | TEC1/2_BTF_*, ±6V_TEC (in) + PGND | pour or ≥2.5 mm @1oz (≈1.3 mm @2oz), 2.5 A peak; tight loops, contained island |
| RF | PD*_IN, TIA*_OUT, LO*_REF, MOD*_DRV, MCLK* | short, guarded, controlled return; off pours |
| Precision DC | 5Vref, VREF_MID, VOUT A–D, *_SENSE_*, *_IMON, NTC*_A | star to AGND, guarded; keep clear of the TEC island |
| Rails | +15V, −15V, +5V, −5V, +5V_LASER, 3V3, 1V8 | modest width (laser 600 mA on +5V_LASER) |
| Digital | SPI (MOSI/SCK/CS), FSYNC, I²C, TFT | DGND reference, away from RF |

---

## 4. Inter-board interface (re-derived from both schematic netlists, post-move)

After the H-bridge move the interface is **purely power rails + grounds — 13 conductors**:

`+15V, -15V, +5V, -5V, +5V_LASER, +6V_TEC, -6V_TEC, 3V3, 1V8, 5Vref, VREF_MID, GND, DGND`

Notes for the connector design:
- **`±6V_TEC` is the new high-current crossing (±2.5 A)** — heavy contacts / paralleled
  pins, with a solid GND return on the same block.
- **No longer cross** (all now on this board with the H-bridge): `TEC1/2_BTF_*`,
  `TEC1/2_SENSE_*`, `VOUTC`, `VOUTD`, `TEC_VZERO`, `HBridgeFault1/2`.
- `VREF_MID` is the only precision-analog net still crossing — guarded pin, or regenerate
  locally to keep it off the power connector.
- `AGND` stays board-internal; tie to incoming `GND` at one point.
- All-power now → **Phoenix MSTB 5.08 mm** fits the whole interface
  (e.g. 6-pos + 6-pos + 3-pos; put ±6V_TEC on the high-current block).

---

## 5. Next steps
1. **Owner:** assign D_PD1/D_PD2 footprints; confirm 4-layer + 2 oz stackup; decide TEC
   H-bridge isolation/heatsinking strategy; finalize MSTB connector sizing.
2. **Re-sync the PCB** from the schematic (Update PCB from Schematic) so the H-bridge parts
   import and the stale TEC drive/sense/control nets clear.
3. After go-ahead, I can script: remove stale outline/zone text, draw new Edge.Cuts,
   grouped first-pass placement by block (incl. the TEC island), net-class/DRC setup.
4. Interactive RF/precision + TEC-power routing in pcbnew.
5. Re-verify DAC-to-current transfer function + power budget after any changes (cascade).
