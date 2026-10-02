Interface PCB
====

The board that connects the ESP32 to the Siedle bus: power, data in and out, and audio. Background:
[Bus-Power.md](Bus-Power.md), [Audio.md](Audio.md#hardware), [Bus-Measurements.md](Bus-Measurements.md).

**Status:** planning. The topology is known, most component values are not: they follow from the bus measurements
and the breadboard bring-up. The schematic can be drawn now, the board ordered afterwards.

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

Schematic outline
----

One KiCad sheet per block:

1. **Bus input.** 2-pin screw terminal (Ta +, Tb −), F1 PTC, TVS1. Board ground is Tb. The receive, send and data
   circuits connect here, before D1 (see [Bus-Power.md](Bus-Power.md#circuit)).
2. **Power stage.** As in [Bus-Power.md](Bus-Power.md#parts-list), plus:
   - a Schottky diode between U1's 5 V and the D1 Mini's 5 V pin, so a USB cable on the D1 Mini cannot feed back
     into U1;
   - a 2-pin terminal or jumper for an external 5 V supply, the fallback if the bus budget is too small.
3. **Data in.** Bus divided down to the comparators' input range, references from 3.3 V, hysteresis through
   feedback resistors, open-drain outputs to GPIO 34 and 35 with pull-ups to 3.3 V.
4. **Data out.** Two transistors from GPIO 16 and 17: 200 Ω across the bus for the whole telegram, 10 Ω during
   0 bits (the [sending recipe](ReverseEngineering.md#findings-from-other-projects)). The transistors need
   ≥ 60 V and the current the bus power supply delivers into a short.
5. **Audio.** ES8311 on the D1 Mini's 3.3 V with its decoupling and reference capacitors, I2C pull-ups, I2S to GPIO
   25–27 and 32. Receive and send stages as in [Audio.md](Audio.md#audio-front-end).
6. **ESP32.** D1 Mini footprint, Wi-Fi / admin button on GPIO 33, VOUT monitor on GPIO 36.

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
- **Board size:** aim for at most 100 × 100 mm (the cheapest PCB price class), probably about 80 × 60 mm, four M3
  mounting holes.

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

- KiCad 9 or newer (`brew install --cask kicad`), which brings `kicad-cli` for ERC, DRC and the fabrication outputs.
- Symbols and footprints for LCSC parts: the "easyeda2kicad" converter or the JLCPCB part library plugin, so every
  part carries its LCSC number in an `LCSC` field.
- Fabrication outputs for JLCPCB (Gerbers, drill files, BOM and placement file) with the "Fabrication Toolkit"
  plugin, which reads that field.
- D1 Mini footprint: start from [besi/kicad-esp32-wemos-d1-mini](https://github.com/besi/kicad-esp32-wemos-d1-mini)
  and check it against the real board.
- The project lives in `hardware/interface-pcb/`.

Open questions
----

- The items in [What is settled](#what-is-settled).
