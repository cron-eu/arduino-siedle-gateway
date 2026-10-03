Interface PCB
====

The board that connects the ESP32 to the Siedle bus: power, data in and out, and audio. Background:
[Bus-Power.md](Bus-Power.md), [Audio.md](Audio.md#hardware), [Bus-Measurements.md](Bus-Measurements.md).

**Status:** schematic draft in [hardware/interface-pcb](../hardware/interface-pcb) (KiCad 10). The topology is
complete, many component values are not: they follow from the bus measurements and the breadboard bring-up. The
[layout](#layout) is a fully routed draft with clean connectivity and DRC checks. Don't order before the values,
footprints and analog behavior are confirmed on the bench.

Revisions
----

1. **Revision 1, carrier board.** The D1 Mini plugs into female headers, so it can be swapped or pulled for
   flashing. The same footprint takes the D1 Mini soldered in directly, with plain pin headers, once the board is
   settled: no separate revision needed for that.
2. **Revision 2, module on the board.** An Espressif module instead of the D1 Mini and a 3.3 V U1, see
   [Audio.md](Audio.md#plan). Not planned in detail here.

What is settled
----

| Block | Settled | Waits for |
|---|---|---|
| Bus input: terminal, PTC fuse, TVS | parts | — |
| Power stage (gyrator, filter, U1, VOUT monitor) | topology and values from the simulation | breadboard bring-up on the bench supply and on the bus |
| Data in: two comparators (≈ 4.5 V data, ≈ 12 V acknowledge) | topology | thresholds and hysteresis from the scope (low/high levels, idle voltage) |
| Data out: carrier and bit pull-down | topology from the Arduino stage | transistor rating (current the power supply delivers into a pulled-down bus) |
| Audio receive: coupling capacitor, series resistor, clamp diodes, ES8311 | topology | speech level |
| Audio send: op-amp and transistor current sink | topology | bus impedance (sets the emitter resistor and gain) |
| ESP32 pins | [draft pin plan](Audio.md#draft-pin-plan), all pins exist on the D1 Mini | — |

So revision 1 should be built to absorb changed values:

- Standard footprints (0805) for every value that depends on a measurement, so a resistor can be swapped by hand.
- Solder jumpers to cut blocks off: the power stage (run from an external 5 V supply instead), the audio receive
  path and the audio send stage.
- Test points on bus +, VOUT, 5 V, 3.3 V, the comparator input and outputs, the audio nodes and the I2S lines, plus
  a ground point for the scope.

Schematic
----

[hardware/interface-pcb/interface-pcb.kicad_sch](../hardware/interface-pcb/interface-pcb.kicad_sch) contains four
A3 landscape sheets. The controller and overview share the root sheet; power, data and audio each have a circuit
sheet. Wires show the signal paths, dividers, filters and feedback loops. Global labels connect the sheets;
local labels join separate functional blocks on the same sheet. GND is Tb throughout.

1. **Controller and measurement map.** The D1 Mini (project symbol and footprint), the setup button on GPIO 33,
   an LED on GPIO 2, the 3.3 V test point and mounting holes. The three sheet boxes provide navigation and a
   compact probe map. I2S signal names are from the ESP32's perspective: DOUT goes to the codec, DIN comes back.
2. **Bus input and power.** Screw terminal (Ta +, Tb −, board ground is Tb), F1 PTC, TVS1, then the gyrator, the
   filter and U1 as in [Bus-Power.md](Bus-Power.md#parts-list), with SMD parts where they exist (Q1 IRFR120N in
   DPAK, C1 a 100 V X7R ceramic). Additions: D2, so a USB cable on the D1 Mini cannot feed back into U1; JP1 to
   cut the bus supply off; J2 for an external 5 V supply (not fitted); the VOUT monitor for GPIO 36.
3. **Bus data in and out.** The bus divided by 34 into an LM393 on 3.3 V. Thresholds with hysteresis: about 5.5 V
   rising and 4.5 V falling (data), 13.1 V and 12.1 V (acknowledge). The outputs are inverted: low while the bus is
   above the threshold. Out:
   200 Ω (carrier) and 10 Ω (0 bits) across the bus through BCP56 transistors on GPIO 16 and 17 (the
   [sending recipe](ReverseEngineering.md#findings-from-other-projects)).
4. **Audio.** ES8311 on the D1 Mini's 3.3 V with the decoupling of the datasheet's application circuit, at I2C
   address 0x18. In the default clock configuration, R30 holds the MCLK pin low and the codec must be configured
   to derive its internal clock from SCLK. For external MCLK on GPIO 0, remove R30 and fit R31 instead. Receive:
   coupling capacitor, series resistor, BAT54S clamps, mid-rail bias, into MIC1P.
   Send: MCP6002 and a BC846 current sink; GPIO 4 ramps its DC bias, the DAC adds the speech. JP2 and JP3 cut
   either path off the bus.

### Using the sheets during measurements

The test points identify nodes in the **planned** board; there is no PCB yet. Start with the installation captures
in [Bus-Measurements.md](Bus-Measurements.md). The other probe references are for later breadboard bring-up and
the first board.

| Sheet | Probe references | What they help establish |
|---|---|---|
| Controller | TP14: 3.3 V | D1 Mini regulator voltage under the additional load |
| Power | TP1: BUS; TP2: VOUT; TP3: 5 V; TP4: GND/Tb | input voltage, startup, hold-up and ripple |
| Data | TP7: BUS_SENSE; TP5: DATA_IN; TP6: ACK_IN | divider scaling, switching thresholds, hysteresis and timing |
| Audio | TP8: RX_IN; TP9: TX_FB; TP10–13: I2S | speech amplitude, current-sink bias and clipping, digital clocks/data |

Two details deserve explicit bring-up checks:

- **Transmit disable:** TX_EN lowers the DC bias; it does not disconnect the DAC from TX_SUM through C42.
  Verify the residual current at TP9 with TX_EN low and the DAC active. Firmware must mute the DAC while disabled;
  decide from the bench results whether a hardware mute is also needed. JP3 disconnects the stage for testing.
- **Comparator input range:** the LM393's inputs work up to VCC − 2 V over its full temperature range, 1.3 V on
  3.3 V ([TI LM393 datasheet](https://www.ti.com/lit/ds/symlink/lm393.pdf); the ordered C7955 is onsemi's
  LM393DR2G, check its datasheet too). BUS/34 stays below 0.95 V at 32 V, and below 1.3 V up to about 44 V. If
  the measured levels move the thresholds, keep that margin when choosing new references.

The clock-source selection is also a firmware requirement, as shown by Espressif's
[ES8311 driver](https://github.com/espressif/esp-adf/blob/release/v2.x/components/esp_codec_dev/device/es8311/es8311.c).

Parts carry their LCSC number in an `LCSC` field where it is checked (JLCPCB library, 2 October 2026). The rest,
mostly common resistor values, get theirs at layout time.

Mechanics
----

The gateway hangs on the wall next to a Siedle indoor station and takes Ta/Tb from the station's terminals. That
asks for a flat, compact, 3D-printed case. Its constraints can be fixed before the schematic is final:

- **Bus wires:** either from behind (through the wall box of the indoor station) or along the wall into the side of
  the case. A screw terminal at the board edge serves both, with a cable entry in the back and one in the side of
  the case.
- **Flat:** lay the two 2200 µF capacitors down (bent leads, or a cut-out in the board) rather than standing them up,
  and keep the MOSFET in a DPAK on board copper instead of a TO-220 with a heat sink. Then the D1 Mini on its headers
  and the lying capacitors (16 mm) are the tallest parts.
- **D1 Mini:** 39 × 31 mm, four rows of 10 pins (two 2 × 10 blocks). Female headers lift it by about 8.5 mm, so its
  top ends up roughly 15–20 mm above the board. Measure that and the row spacing on the real board with calipers
  before trusting any footprint: the clones differ.
- **Antenna:** the D1 Mini's antenna end goes to the board edge, with no copper and no tall parts under or next to
  it, ideally overhanging the edge. No metal enclosure.
- **Other large parts:** the two 2200 µF / 50 V capacitors (about 16 mm diameter, 25–31 mm long), U1 (SIP-3, about
  11 × 8 mm footprint, 10 mm high).
- **Outside access:** the bus terminal at one edge, the button and an LED reachable or visible through the case,
  the D1 Mini's USB port reachable for flashing.
- **Board size:** 90 × 75 mm, with four M3 mounting holes. This is about 16% less area than the first 100 × 80 mm
  placement. The case can be designed around the dimensions below; check the real module and connectors first.

Layout
----

[hardware/interface-pcb/interface-pcb.kicad_pcb](../hardware/interface-pcb/interface-pcb.kicad_pcb): 2 layers,
90 × 75 mm, routed. All 101 components retain their values, footprint identifiers, pad nets, population flags,
UUIDs and schematic links. *Update PCB from Schematic* therefore keeps working. The final KiCad 10.0.6 check
reports **0 violations, 0 unconnected items and 0 schematic-parity issues**, with zones refilled.

Floorplan, seen from the top with the bus terminal on the left:

| Area | Parts |
|---|---|
| Left edge | J1 (bus terminal, wires enter from the left), F1, TVS1 |
| Top left | gyrator (Q1, Q2, R1, R2, C1, DZ1, D1), below it the data out stage (Q3, Q4, R20–R25) and filter resistors |
| Left side, lying | C2, along the left edge |
| Bottom, lying | C3 along the bottom edge, with its leads facing right toward the regulator |
| Upper middle | data in (U2 with dividers and references), I2C pull-ups |
| Lower left and middle | ES8311, its input/reference capacitors and receive path; U4/Q5 audio send stage to their right |
| Top right | D1 Mini on its sockets, antenna at the top edge |
| Right edge and lower right | setup button SW1 and LED D50; U1, JP1, D2 and the optional J2 supply connector |

- **Heights, for the case:** the lying 2200 µF capacitors are the tallest parts at about 16 mm. The D1 Mini sits on
  8.5 mm sockets, so its top ends up around 15 mm above the board (to be measured, the footprint has the sockets'
  3D models but none for the module). U1 stands about 10 mm. The rest is SMD.
- **USB:** the D1 Mini's USB port points down the board. Keep the strip below it (x 59–73 mm) free of tall parts,
  so a cable can reach it through the case.
- **Antenna:** a rule area keeps copper (tracks, vias, pads, the ground pour) out from under the antenna end on
  both layers. The codec is about 50 mm from the center of this antenna area. Audio circuits occupy the lower
  part of the board, separated from the radio and the regulator; this reduces coupling risk but needs a noise
  measurement with Wi-Fi active.
- **Ground:** GND pours on both layers, joined by 63 stitching vias, including local returns around the audio
  section. SMD ground pads connect solidly. U3's exposed pad connects to ground pins 5, 10 and 20 through short
  front-layer necks, with ground vias outside its solder pad. The D1 Mini's GND pins also connect solidly:
  the socket rows leave no room for thermal spokes, so solder them with a bit more heat.
- **Power:** wider bus/supply routing, a front-layer copper area on Q1's drain to spread heat, and C3's positive
  lead facing the regulator. Check Q1 temperature during startup and the waveform at U1's input pins during
  bring-up; routing and copper area do not replace the power-stage measurements.
- **Net classes** are matched by name in the project. Nominal widths/clearances: *Power* 0.5/0.3 mm,
  *PullDown* 0.8/0.3 mm, *Default* 0.2/0.18 mm, *Codec* 0.15/0.18 mm, *Ground* 0.25/0.18 mm. Codec includes
  the fine-pitch signals and 3.3 V distribution; some analog routes also use 0.15 mm tracks. The board minimum
  is 0.15 mm width and clearance, with 0.6/0.3 mm vias normally used. A local rule in
  [interface-pcb.kicad_dru](../hardware/interface-pcb/interface-pcb.kicad_dru) allows 0.15 mm ground-neck clearance
  at the ES8311's corner pads; it does not relax the rest of the board.
- **C2 and C3** use a project footprint for a capacitor lying on the board (`CP_Radial_D16.0mm_P7.50mm_Lying`):
  the leads are bent by 90° at the body, and the courtyard covers the body.

### Dimensions for the case

Coordinates below are in millimeters from the **top-left board corner**, viewed from the component side, with
x increasing right and y increasing down. The KiCad drawing origin for that corner is (50, 50).

| Item | Dimensions / position |
|---|---|
| Board | 90 × 75 mm, nominal thickness 1.6 mm |
| Mounting holes | Ø 3.2 mm at (4, 4), (86, 4), (4, 71), (86, 71) |
| Mounting-hole spacing | 82 × 67 mm |
| Reserved D1 Mini footprint area | x 50–82, y 0.1–40.1; USB faces the bottom edge |
| Antenna copper keepout | x 50–82, y 0–8.5, both layers |
| Bus terminal J1 | left edge; pin 1 at (5.75, 22), pin 2 at (5.75, 27.08) |
| Setup button SW1 | footprint center (83.5, 46) |
| LED D50 | footprint center (78, 51) |

The socket 3D models have been aligned to both pad rows. The preview has no D1 Mini module model, and the lying
capacitors use approximate rotated body models rather than accurately bent leads. Use measured parts for the
final case height, USB opening, button actuator and connector access.

Next: bus measurements, bench bring-up, check the D1 Mini and R-78CK footprints against the real parts, and a
review of the analog behavior and ratings before any order. The current layout is a prototype draft.

Assembly
----

JLCPCB places the SMD parts, including the ES8311. We solder the few through-hole parts ourselves. JLCPCB prices
from 2 October 2026, for 5 bare boards of which 2 are assembled (Economic PCBA, top side only):

| Item | Cost |
|---|---|
| 5 PCBs, 2 layers, up to 100 × 100 mm, lead-free HASL | ~$4 |
| Assembly setup $8.18, stencil $1.53 | $9.71 |
| Loading fee for each Extended library part, about 8 of them (ES8311, IRFR120N, PTC fuse, 2.2 µF / 100 V, 18 Ω, the 2512 resistors) at $3.07; Basic and Preferred parts cost nothing | ~$25 |
| SMD parts for 2 boards, with JLCPCB's minimum quantities | ~$10 |
| Solder joints, about 320 at $0.0016 | ~$0.50 |
| DHL Express to the company address | ~$25–30 |
| **JLCPCB order** | **~$75–80 (about €65–70)** |
| DHL customs clearance fee | ≥ €15 |
| Import VAT, 19 % (deductible, see below) | ~€13 |
| Through-hole parts from Reichelt: 2 × R-78CK5.0-0.5, 4 × 2200 µF / 50 V, female headers (the 5.08 mm terminals are in stock) | ~€15–20 |
| **Two working boards** | **~€95–105 net** |

- **Through-hole parts by hand:** per board two capacitors, U1, the bus terminal and the D1 Mini headers, about 50
  joints, 15–20 minutes.
- **Through-hole parts by JLCPCB as well:** a $3.58 hand soldering fee, $0.0164 per joint and three more loading
  fees (terminal, capacitor, headers), about $14 more. U1 is not in JLCPCB's stock (one piece left): global
  sourcing adds 9–20 working days and possibly inspection fees. Not worth it for two boards, worth another look
  for a series.
- **Everything by hand:** no. The ES8311 is a QFN with 0.4 mm pitch, and each board has about 50 small SMD parts:
  2–3 hours per board plus a separate LCSC order and its shipping and import costs, to save about $35 of fees.
- **More boards:** the fees are per order. Each additional assembled board adds about $3–4 for parts and joints, so
  assembling all 5 costs about $12 more. Revision 1 will probably change, so 2 (or 3) are enough.
- **Revision 2** is a separate order of the same size again.

Ordering as the GmbH (an assessment, to confirm with the tax adviser):

- Put the company name in the delivery address, keep the VAT ID in the JLCPCB account, and ship with a courier
  (DHL Express, UPS, FedEx). JLCPCB treats its cheap "Global Standard Direct Line" as a private shipment even to a
  company: VAT charged at checkout under the EU import scheme (IOSS), which is not deductible, plus the EU flat duty
  of €3 per tariff line on parcels under €150 since 1 July 2026.
- With a courier, the parcel is cleared through customs in the company's name. The import VAT (Einfuhrumsatzsteuer)
  is deductible as input tax (§ 15 (1) no. 2 UStG), with the DHL import document as proof. Duty is 0 % on bare PCBs
  (HS 8534) and 0–2.2 % on assembled boards, depending on the classification.
- Sources: [JLCPCB assembly prices](https://jlcpcb.com/help/article/pcb-assembly-price),
  [customs, duties and taxes](https://jlcpcb.com/help/article/customs,-duties-and-taxes),
  [EU customs reform FAQ](https://jlcpcb.com/help/article/eu-customs-reform-faq),
  [EU €3 duty](https://commission.europa.eu/news-and-media/news/ensuring-fairness-and-safety-eur3-customs-duty-low-value-parcels-2026-06-29_en).
  Prices and the Basic/Extended status of parts change often: check again at order time.

KiCad workflow
----

- KiCad 10 (`brew install --cask kicad`), with `kicad-cli` for ERC, DRC and the fabrication outputs:
  `kicad-cli sch erc hardware/interface-pcb/interface-pcb.kicad_sch` reports no violations for the draft.
- For the routed board, run
  `kicad-cli pcb drc --schematic-parity --refill-zones --exit-code-violations -o /tmp/interface-pcb-drc.rpt hardware/interface-pcb/interface-pcb.kicad_pcb`.
  Routing was completed with local Freerouting passes plus checked manual/geometry-assisted corrections in
  KiCad. Its source geometry, rather than an autorouter completion message, is the final check.
- `AGENTS.md` records the macOS scripting pitfalls. Use
  `hardware/interface-pcb/tools/kicad-python script.py` to keep script diagnostics in a log.
- Export the four sheets for review or printing with
  `kicad-cli sch export pdf --no-background-color -o interface-pcb.pdf hardware/interface-pcb/interface-pcb.kicad_sch`.
  A3 at 100% preserves the intended text size; the PDF remains sharp when zoomed. ERC checks connectivity rules,
  not analog behavior, thermal ratings or the bus loading.
- The project library `gateway` holds the parts KiCad lacks: the ES8311, the D1 Mini, the R-78CK, and the IRFR120N
  and BCP56 adapted to their footprints.
- The D1 Mini footprint (`gateway:D1_Mini_ESP32_Socket`) places the four rows of 10 pads 22.86 mm apart (inner
  rows), with the antenna end marked. Its pad numbers follow
  [besi/kicad-esp32-wemos-d1-mini](https://github.com/besi/kicad-esp32-wemos-d1-mini). Check it against the real
  board before the layout.
- Fabrication outputs for JLCPCB (Gerbers, drill files, BOM and placement file) with the "Fabrication Toolkit"
  plugin, which reads the `LCSC` field. Footprints for new LCSC parts: the "easyeda2kicad" converter.

Open questions
----

- The items in [What is settled](#what-is-settled).
- Whether the D1 Mini's 3.3 V regulator copes with the ES8311 and the comparators on top of the ESP32 (it should:
  together about 10 mA).
- The R-78CK footprint: the draft uses KiCad's R-78E footprint (same SIP-3 pinout), to be checked against the R-78CK
  drawing.
