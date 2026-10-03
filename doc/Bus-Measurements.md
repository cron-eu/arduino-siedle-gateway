Bus Measurements (Rigol DS1000Z)
====

The phase 2 bus driver and the audio front end (phases 3–4) need real numbers from our installation. This is the
checklist for one measuring session.

| We need to know                                   | It decides                                              |
|---------------------------------------------------|---------------------------------------------------------|
| Idle, low and "acknowledge" voltages, edge times  | comparator threshold and hysteresis, RMT glitch filter  |
| Exact bit time and its spread                     | RMT resolution and decoding tolerance                   |
| Audio level, DC level and bandwidth during a call | codec input gain, coupling capacitor, transformer       |
| Whether audio is present *before* off-hook         | if "listen before answering" is possible at all         |
| How the bus behaves while the audio path is open  | how much current our device may draw and modulate       |
| Voltage drop under load, longest low time         | current budget and buffer of the power stage            |

Safety first
----

- The bus is shared by the whole building. Don't load or short it, and preferably measure outside office hours.
- The scope's ground clip is connected to mains earth. Before clipping it on, measure with a multimeter from each bus
  wire to protective earth. If the bus floats (no fixed voltage to earth), the ground clip may go on the bus minus
  (Tb). **Never** put the ground clip on the bus plus. If in doubt, use the ground clip nowhere and measure both wires
  with two probes (CH1 − CH2 with the Math function).
- Use the probes in **10x**, and set the channel probe ratio to 10x as well.

Setup
----

- CH1: probe tip on Ta (bus +), ground clip on Tb (bus −). Measure next to the gateway's future mounting place.
- *Storage* → set up a USB stick for PNG screenshots and CSV waveforms.
- Note the time of every capture, so it can be matched with the old gateway's MQTT log (`siedle/received`).

Captures
----

### 1. Idle bus

CH1 DC-coupled, 5 V/div, 1 ms/div, *Measure* → Vavg and Vpp.

- [ ] Idle DC voltage, and the ripple/noise in Vpp (expected: 26–29 V, up to 32 V while ringing)
- [ ] Highest DC voltage while the bell rings (ring at the door during the capture): at most 32 V allows 35 V
  capacitors for C2 and C3 ([Bus-Power.md](Bus-Power.md#smaller-c2-and-c3-decide-after-the-measurements))
- [ ] Screenshot

### 2. Data frame

CH1 DC-coupled, 5 V/div, **10 ms/div** (a frame is 32 × 2 ms = 64 ms), trigger on the **falling edge at about half
the idle voltage**, mode *Single*. Memory depth *Auto* or 12 Mpts.

Trigger, one after the other: a door ring, the door opener button on an indoor station, the light button.

- [ ] Screenshot plus CSV of each frame
- [ ] Low level voltage during a 0 bit, high level during a 1 bit inside the frame (expected: about 2 V and 7–8 V)
- [ ] Bit time: cursors across 10 bits, divided by 10
- [ ] Rise and fall time: zoom into one edge (*Measure* → Rise/Fall time)
- [ ] Longest continuous low time inside a frame (cursors): the [power stage](Bus-Power.md) has to bridge it
- [ ] **Acknowledge:** the old firmware expects the bus master to raise the voltage above ~12 V within 3 bit times
  after a frame. Capture 20 ms after the frame end (trigger as above, set the trigger position to the left edge).

### 3. Audio during a call

1. Ring at the door, pick up the handset of an indoor station, talk at the door station, then hang up.
2. During the call, CH1 **DC-coupled**, 5 V/div: does the bus voltage change once the audio path is open?
   - [ ] DC level during the call compared to idle
3. CH1 **AC-coupled**, 20–50 mV/div, 1 ms/div, *Auto* trigger:
   - [ ] Vpp while someone talks at the door (speech, a whistle, clapping; others report 50–200 mV)
   - [ ] Vpp while it is silent (noise floor)
   - [ ] Vpp while someone talks into the indoor handset (the other direction, as seen from our connection point)
   - [ ] CSV of about 1 s of speech: 100 ms/div with memory depth 1.2 Mpts gives 1 MSa/s, plenty for audio and
     still a manageable CSV file
4. *Math* → *FFT*, source CH1, window Hanning, span 0–10 kHz, while someone whistles or talks:
   - [ ] Screenshot: bandwidth of the speech signal, plus any carrier or pilot tones
5. **Before pick-up:** ring, and while it rings (handset still on hook) talk at the door station with CH1
   AC-coupled:
   - [ ] Is there any audio on the bus before pick-up? (yes/no, Vpp)

### 4. Power budget

The gateway will draw about 30 mA from the bus ([Bus-Power.md](Bus-Power.md)).

- [ ] Model of the bus power supply (label on the DIN rail unit, e.g. BNG 650 or BVNG 650) and the number of indoor
  stations in the building
- [ ] Current of the old Arduino gateway, which is already powered from the bus (multimeter in series with its bus
  connection)
- [ ] Bus voltage with and without a load: outside a call, put a 1 kΩ / 2 W resistor across the bus for a few
  seconds (about 28 mA) and measure the DC voltage with CH1 or a multimeter

### 5. Call sequence

Optional, the old gateway logs the frames anyway: note which frames show up in `siedle/received` for a complete
call (ring → off-hook → door open → on-hook) and how much time passes between them.

What to bring back
----

The USB stick contents (PNG and CSV) and the filled in checklist. If the scope is on the LAN, we can also pull
full-resolution waveforms over SCPI (port 5555) with a small script instead of the USB stick.
