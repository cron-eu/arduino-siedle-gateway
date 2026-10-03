"""Shared helpers for the 3D model scripts: boxes, cylinders and a coloured STEP writer.

Coordinates are KiCad 3D model coordinates: mm, x right, y up (opposite to the footprint's y),
z up from the board surface. Needs OpenCascade's Python bindings (pip install cadquery-ocp).
"""
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.gp import gp_Ax2, gp_Dir, gp_Pnt
from OCP.IFSelect import IFSelect_RetDone
from OCP.Interface import Interface_Static
from OCP.Quantity import Quantity_Color, Quantity_TOC_sRGB
from OCP.STEPCAFControl import STEPCAFControl_Writer
from OCP.STEPControl import STEPControl_AsIs
from OCP.TCollection import TCollection_ExtendedString
from OCP.TDocStd import TDocStd_Document
from OCP.XCAFDoc import XCAFDoc_ColorSurf, XCAFDoc_DocumentTool


def box(x0, y0, z0, x1, y1, z1):
    """An axis-aligned box between two corners."""
    return BRepPrimAPI_MakeBox(gp_Pnt(min(x0, x1), min(y0, y1), min(z0, z1)),
                               gp_Pnt(max(x0, x1), max(y0, y1), max(z0, z1))).Shape()


def cylinder(x, y, z, direction, radius, length):
    """A cylinder starting at (x, y, z) along `direction`."""
    return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z), gp_Dir(*direction)), radius, length).Shape()


def compound(shapes):
    """Several shapes as one, so they share a colour and one entry in the STEP file."""
    from OCP.BRep import BRep_Builder
    from OCP.TopoDS import TopoDS_Compound
    c = TopoDS_Compound()
    builder = BRep_Builder()
    builder.MakeCompound(c)
    for s in shapes:
        builder.Add(c, s)
    return c


def write_step(parts, path):
    """Write [(shape, (r, g, b))] with sRGB colours as one STEP file."""
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
    if writer.Write(path) != IFSelect_RetDone:
        raise SystemExit('writing %s failed' % path)
    print('wrote', path)
