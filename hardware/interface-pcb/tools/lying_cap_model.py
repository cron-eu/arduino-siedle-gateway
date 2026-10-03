"""STEP model of a radial electrolytic capacitor lying on the board, leads bent 90 degrees.

Matches the gateway:CP_Radial_*_Lying footprints: pad 1 (+) at the origin, pad 2 (-) at
(0, pitch) in footprint coordinates, the body starting at x = gap and lying along +x.

Needs OpenCascade's Python bindings (pip install cadquery-ocp), not KiCad's Python:

    python3 lying_cap_model.py --diameter 16 --length 30 --pitch 7.5 \
        ../gateway.3dshapes/CP_Radial_D16.0mm_L30.0mm_P7.50mm_Lying.step
"""
import argparse
import math

from OCP.BRepAlgoAPI import BRepAlgoAPI_Cut, BRepAlgoAPI_Common
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeEdge, BRepBuilderAPI_MakeWire
from OCP.BRepOffsetAPI import BRepOffsetAPI_MakePipe
from OCP.BRepPrimAPI import BRepPrimAPI_MakeCylinder, BRepPrimAPI_MakePrism
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeFace, BRepBuilderAPI_MakePolygon
from OCP.GC import GC_MakeArcOfCircle
from OCP.gp import gp_Ax2, gp_Circ, gp_Dir, gp_Pnt, gp_Vec
from OCP.IFSelect import IFSelect_RetDone
from OCP.Quantity import Quantity_Color, Quantity_TOC_sRGB
from OCP.STEPCAFControl import STEPCAFControl_Writer
from OCP.STEPControl import STEPControl_AsIs
from OCP.TCollection import TCollection_ExtendedString
from OCP.TDocStd import TDocStd_Document
from OCP.XCAFDoc import XCAFDoc_ColorSurf, XCAFDoc_DocumentTool
from OCP.Interface import Interface_Static


def body(d, length, gap, y_axis):
    """The can, along +x, its axis at y_axis and half the diameter above the board."""
    ax = gp_Ax2(gp_Pnt(gap, y_axis, d / 2), gp_Dir(1, 0, 0))
    return BRepPrimAPI_MakeCylinder(ax, d / 2, length).Shape()


def stripe(d, length, gap, y_axis, y_minus):
    """The polarity stripe: a 60 degree sector of the can's skin, turned towards the - lead."""
    r_out, r_in = d / 2 + 0.02, d / 2 - 0.3
    side = -1 if y_minus < y_axis else 1
    centre = math.radians(45)                       # half way between the side and the top
    pts = [gp_Pnt(gap, y_axis, d / 2)]
    for a in (centre - math.radians(30), centre + math.radians(30)):
        pts.append(gp_Pnt(gap, y_axis + side * 2 * r_out * math.cos(a), d / 2 + 2 * r_out * math.sin(a)))
    poly = BRepBuilderAPI_MakePolygon(*pts, True).Wire()
    wedge = BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(poly).Face(), gp_Vec(length, 0, 0)).Shape()
    ax = gp_Ax2(gp_Pnt(gap, y_axis, d / 2), gp_Dir(1, 0, 0))
    shell = BRepAlgoAPI_Cut(BRepPrimAPI_MakeCylinder(ax, r_out, length).Shape(),
                            BRepPrimAPI_MakeCylinder(ax, r_in, length).Shape()).Shape()
    return BRepAlgoAPI_Common(shell, wedge).Shape()


def lead(y, z, gap, wire_d, bend_r, below):
    """From the body's end face at height z: horizontal, a 90 degree bend, then down through the pad."""
    x_bend = bend_r                                  # the vertical leg sits on the pad, x = 0
    p0 = gp_Pnt(gap, y, z)
    p1 = gp_Pnt(x_bend, y, z)
    mid = gp_Pnt(x_bend - bend_r * math.sin(math.pi / 4), y, z - bend_r + bend_r * math.cos(math.pi / 4))
    p2 = gp_Pnt(0, y, z - bend_r)
    p3 = gp_Pnt(0, y, -below)
    w = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(p0, p1).Edge(),
                                BRepBuilderAPI_MakeEdge(GC_MakeArcOfCircle(p1, mid, p2).Value()).Edge(),
                                BRepBuilderAPI_MakeEdge(p2, p3).Edge()).Wire()
    profile = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(
        gp_Circ(gp_Ax2(p0, gp_Dir(-1, 0, 0)), wire_d / 2)).Edge()).Wire()
    pipe = BRepOffsetAPI_MakePipe(w, BRepBuilderAPI_MakeFace(profile).Face())
    return pipe.Shape()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--diameter', type=float, required=True)
    ap.add_argument('--length', type=float, required=True)
    ap.add_argument('--pitch', type=float, required=True)
    ap.add_argument('--gap', type=float, default=1.5, help='pad centre to body end face (mm)')
    ap.add_argument('--wire', type=float, default=0.8, help='lead diameter (mm)')
    ap.add_argument('--board', type=float, default=1.6, help='board thickness (mm)')
    ap.add_argument('output')
    a = ap.parse_args()

    # KiCad's 3D model y axis points opposite to the footprint's: pad 2 sits at y = -pitch.
    y_plus, y_minus = 0.0, -a.pitch
    y_axis = -a.pitch / 2
    z = a.diameter / 2
    bend_r = min(1.0, a.gap * 0.66)
    below = a.board + 1.5

    parts = [
        (body(a.diameter, a.length, a.gap, y_axis), (0.10, 0.25, 0.55)),
        (stripe(a.diameter, a.length, a.gap, y_axis, y_minus), (0.80, 0.82, 0.85)),
        (lead(y_plus, z, a.gap, a.wire, bend_r, below), (0.75, 0.75, 0.75)),
        (lead(y_minus, z, a.gap, a.wire, bend_r, below), (0.75, 0.75, 0.75)),
    ]

    doc = TDocStd_Document(TCollection_ExtendedString('XmlOcaf'))
    shapes = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())
    colors = XCAFDoc_DocumentTool.ColorTool_s(doc.Main())
    for shape, rgb in parts:
        label = shapes.AddShape(shape, False)
        colors.SetColor(label, Quantity_Color(*rgb, Quantity_TOC_sRGB), XCAFDoc_ColorSurf)

    Interface_Static.SetCVal_s('write.step.unit', 'MM')
    writer = STEPCAFControl_Writer()
    writer.SetColorMode(True)
    writer.Transfer(doc, STEPControl_AsIs)
    if writer.Write(a.output) != IFSelect_RetDone:
        raise SystemExit('writing %s failed' % a.output)
    print('wrote', a.output)


if __name__ == '__main__':
    main()
