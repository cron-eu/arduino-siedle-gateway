# Project instructions

- Use neutral or conventional language. Do not use gender forms with special characters.

## KiCad work

- Read `doc/PCB.md` and the current schematic before changing the interface board. Preserve schematic links,
  pad nets, component values, population options and project footprints unless the requested change calls for
  an electrical change. The analog values remain provisional until bus measurements and bench bring-up.
- Board dimensions may change to improve the layout. Prefer a compact, simple outline suitable for a printed
  enclosure, with room for the lying capacitors, mounting hardware and USB access. Keep the codec and sensitive
  analog circuitry away from the ESP32 antenna and the switching regulator. Preserve the antenna copper and
  component keepout on both layers.
- Work on a temporary copy while iterating. KiCad may be open on the original; check for newer saved changes
  before replacing it, and tell the user to reopen the board after external edits.
- Run schematic-parity DRC, copper/clearance checks, zone refill and a visual review before delivering a layout.
  Report remaining unconnected items and violations honestly; a clean DRC does not validate analog behavior.

### macOS KiCad 10 scripting findings

Verified with KiCad 10.0.6 on this machine:

- The bundled Python API is available at
  `/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9`.
  It can load/save boards and export/import Specctra DSN/SES without a separate plugin.
- **Avoid `board.Remove(item)` in standalone scripts.** Its wrapper calls `IsActionRunning()`, which triggered
  a second KiCad runtime initialization in this environment. Subsequent calls returned unwrapped SWIG objects
  or segfaulted. `board.RemoveNative(item)` worked instead. Keep removed objects alive until the operation ends;
  take care with SWIG ownership. Do not blindly repeat a crashing call.
- For new geometry, supply the board as parent, then call `board.Add(item)`. Use `pcbnew.DEGREES_T` with
  `EDA_ANGLE`, and `GetAreaCount()` / `GetArea(index)` when editing zones.
- `SaveBoard(path, board, True)` skips saving project settings. Use this when `.kicad_pro` rules are managed
  explicitly, so saving geometry does not replace the intended net classes and manufacturing rules.
- KiCad DRC crashed inside the macOS sandbox with `SwiftNativeNSArray ... Array index out of range`; the same
  read-only command worked with the normal approved sandbox escalation. `wx.App(False)` also needed display
  access and was unnecessary for the successful standalone editing scripts.
- Redirect Python stdout/stderr into a task-local log and report a concise success/failure result. Enable
  `-X faulthandler` for useful crash diagnostics. A shell wrapper does **not** suppress macOS crash-report dialogs;
  avoid the faulty API instead. Do not change the user's system-wide crash-report preferences.
  The reusable wrapper is `hardware/interface-pcb/tools/kicad-python script.py [arguments ...]`.
  `KICAD_PYTHON` and `KICAD_LOG_DIR` can override its executable and log directory.
- A successful `command -v java` on macOS does not establish an installed Java runtime; check `java -version`.
  For local autorouting, Freerouting 2.4.1 and a verified portable Java 25 runtime were usable from `/tmp`.
  Disable its analytics with `-da`, use `--gui.enabled=false --api_server.enabled=false`, and set an explicit
  `--user_data_path` in the task's temporary directory. Never upload board files to a routing service implicitly.
- Importing autorouter output is an intermediate step: check the result in KiCad, including ground connectivity,
  antenna keepouts, narrow codec escapes, power-track widths and sensitive analog paths.
- Freerouting may create neckdowns below the board minimum, and its plane assumptions can leave KiCad ground
  islands disconnected. Refill and check in KiCad, finish escapes, and add ground stitching as needed. Keep the
  ES8311 reference/input capacitors in pin order so their routes do not obstruct SDA/MCLK escape.
- Custom DRC expressions use `NetName` for net-name comparisons. The local ES8311 ground-neck rule in
  `interface-pcb.kicad_dru` permits 0.15 mm clearance at the QFN corners; do not remove it or loosen the whole
  board to hide a local violation. Ground necks connect the exposed pad to pins 5, 10 and 20, then to nearby
  ground vias outside the solder pad.
