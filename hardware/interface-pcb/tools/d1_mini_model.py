"""Simplified STEP model of the AZ-Delivery ESP32 D1 Mini, plugged into the board's sockets.

For the 3D view and the case design, not for the module's own details: the board, the
ESP32-WROOM-32 with its antenna, the micro-USB connector, the reset button, the CP2104 and
the four rows of male headers that go down into the sockets.

Coordinates follow the gateway:D1_Mini_ESP32_Socket footprint: origin in the centre of the pin
field, the antenna end towards -y in footprint coordinates (+y in the 3D model), z = 0 on the
surface of our board. Defaults are nominal values from AZ-Delivery's product page (39 x 31.5 mm)
and photos; replace them with caliper measurements.

    python3 d1_mini_model.py ../gateway.3dshapes/ESP32_D1_Mini_AZ-Delivery.step

Needs OpenCascade's Python bindings (pip install cadquery-ocp).
"""
import argparse

from step_util import box, compound, write_step

PITCH = 2.54
ROWS_X = (-13.97, -11.43, 11.43, 13.97)          # outer and inner pin rows, as in the footprint
PIN_Y = [-11.43 + PITCH * i for i in range(10)]  # footprint y of the ten pins in each row

BLUE, BLACK, METAL, GOLD, WHITE = ((0.10, 0.30, 0.75), (0.08, 0.08, 0.08), (0.78, 0.78, 0.80),
                                   (0.85, 0.70, 0.30), (0.92, 0.92, 0.92))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--width', type=float, default=31.5, help='board width (mm)')
    ap.add_argument('--antenna-edge', type=float, default=-20.6,
                    help='footprint y of the board edge at the antenna end')
    ap.add_argument('--usb-edge', type=float, default=18.4, help='footprint y of the board edge at the USB end')
    ap.add_argument('--thickness', type=float, default=1.2, help='board thickness')
    ap.add_argument('--socket', type=float, default=8.5, help='height of our female sockets')
    ap.add_argument('--spacer', type=float, default=2.5, help="height of the male headers' plastic spacer")
    ap.add_argument('--module-height', type=float, default=3.1, help='ESP32-WROOM-32 height above the board')
    ap.add_argument('--usb-overhang', type=float, default=0.6, help='micro-USB front beyond the board edge')
    ap.add_argument('output')
    a = ap.parse_args()

    def b(fx0, fy0, z0, fx1, fy1, z1):
        """A box in footprint x/y (y down), converted to the model's y axis."""
        return box(fx0, -fy0, z0, fx1, -fy1, z1)

    z_bottom = a.socket + a.spacer                 # underside of the D1 Mini
    z_top = z_bottom + a.thickness                 # its component side
    half = a.width / 2
    parts = [(b(-half, a.antenna_edge, z_bottom, half, a.usb_edge, z_top), BLUE)]

    # ESP32-WROOM-32: 18 x 25.5 mm, antenna end flush with the board edge, metal shield over the rest
    m0, m1 = a.antenna_edge, a.antenna_edge + 25.5
    parts.append((b(-9, m0, z_top, 9, m1, z_top + 0.8), BLACK))
    parts.append((b(-8.3, m1 - 18.0, z_top + 0.8, 8.3, m1 - 0.4, z_top + a.module_height), METAL))

    # micro-USB, centred on the USB edge
    u1 = a.usb_edge + a.usb_overhang
    parts.append((b(-3.75, u1 - 5.0, z_top, 3.75, u1, z_top + 2.6), METAL))
    # CP2104 (QFN-24, 4 x 4 mm) and the 3.3 V regulator
    parts.append((b(6.0, 10.0, z_top, 10.0, 14.0, z_top + 0.9), BLACK))
    parts.append((b(-10.0, 10.5, z_top, -7.0, 13.5, z_top + 1.1), BLACK))
    # reset button at the long edge next to the USB end, pressed from the side
    parts.append((b(half - 3.2, 10.0, z_top, half - 0.2, 14.0, z_top + 1.8), WHITE))
    parts.append((b(half - 0.2, 11.4, z_top + 0.4, half + 0.6, 12.6, z_top + 1.4), BLACK))

    # male headers under the board: two 2 x 10 blocks of plastic, 40 pins
    for x0, x1 in ((ROWS_X[0] - 1.27, ROWS_X[1] + 1.27), (ROWS_X[2] - 1.27, ROWS_X[3] + 1.27)):
        parts.append((b(x0, PIN_Y[0] - 1.27, a.socket, x1, PIN_Y[-1] + 1.27, z_bottom), BLACK))
    pins = [b(x - 0.32, y - 0.32, a.socket - 6.0, x + 0.32, y + 0.32, z_top + 1.8) for x in ROWS_X for y in PIN_Y]
    parts.append((compound(pins), GOLD))

    write_step(parts, a.output)


if __name__ == '__main__':
    main()
