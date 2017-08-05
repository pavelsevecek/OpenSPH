#!/bin/python

# Debugging helpers for custom types
# Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
# 2016-2017

from dumper import *
import math
import sys

def qdump____m128(d, value):
    d.putEmptyValue()
    d.putNumChild(1)
    if d.isExpanded():
        d.putArrayData(value.address(), 4, d.lookupType("float"))


def qdump____m128d(d, value):
    d.putEmptyValue()
    d.putNumChild(1)
    if d.isExpanded():
        d.putArrayData(value.address(), 2, d.lookupType("double"))

def qdump__Sph__AutoPtr(d, value):
    ptr = value["ptr"]
    if ptr.integer() == 0:
        d.putValue("nullptr")
    else:
        d.putItem(ptr.dereference())

def qdump__Sph__RawPtr(d, value):
    ptr = value["ptr"]
    if ptr.integer() == 0:
        d.putValue("nullptr")
    else:
        d.putItem(ptr.dereference())

def qdump__Sph__SharedPtr(d, value):
    ptr = value["ptr"]
    if ptr.integer() == 0:
        d.putValue("nullptr")
    else:
        d.putItem(ptr.dereference())

def qdump__Sph__Optional(d, value):
    used = value["used"]
    if used.integer() == 0:
        d.putValue("NOTHING")
    else:
        templateType = d.templateArgument(value.type, 0)
        x = d.createValue(value.address(), templateType)
        d.putItem(x)

def qdump__Sph__Array(d, value):  
    actsize = value["actSize"].integer()
#    maxsize = value["maxSize"].integer()
    d.putNumChild(actsize)
    p = value["data"]
    d.putValue("(%s values)" % (actsize))
    innerType = p.type
    templateType = d.templateArgument(value.type, 0)
    if d.isExpanded():
        with Children(d, actsize, childType=innerType): 
            for i in range(0, actsize):
                d.putSubItem(i, p.dereference().cast(templateType))
                p = p + 1

def qdump__Sph__ArrayView(d, value):
    actsize = value["actSize"].integer()
    d.putNumChild(actsize)
    p = value["data"]
    d.putValue("(%s values)" % (actsize))
    innerType = p.type
    templateType = d.templateArgument(value.type, 0)
    if d.isExpanded():
        with Children(d, actsize, childType=innerType): 
            for i in range(0, actsize):
                d.putSubItem(i, p.dereference().cast(templateType))
                p = p + 1

def qdump__Sph__BasicVector(d, value):
    d.putNumChild(4)
    templateType = d.templateArgument(value.type, 0)
    siz = 8 # have to be changed to 4 for floats!
    x = d.createValue(value.address(), templateType)
    y = d.createValue(value.address() + siz, templateType)
    z = d.createValue(value.address() + 2*siz, templateType)
    h = d.createValue(value.address() + 3*siz, templateType)
    if d.isExpanded():
        with Children(d, 4, childType=templateType):
            d.putSubItem("x", x)
            d.putSubItem("y", y)
            d.putSubItem("z", z)
            d.putSubItem("h", h)

def qdump__Sph__TracelessTensor(d, value):
    d.putNumChild(5)
    templateType = d.lookupType("double")
    siz = 8 # have to be changed to 4 for floats!
    sxx = d.createValue(value.address(), templateType)
    syy = d.createValue(value.address() + siz, templateType)
    sxy = d.createValue(value.address() + 2*siz, templateType)
    sxz = d.createValue(value.address() + 3*siz, templateType)
    syz = d.createValue(value.address() + 4*siz, templateType)
    if d.isExpanded():
        with Children(d, 5, childType=templateType):
            d.putSubItem("sxx", sxx)
            d.putSubItem("syy", syy)
            d.putSubItem("sxy", sxy)
            d.putSubItem("sxz", sxz)
            d.putSubItem("syz", syz)

def qdump__Sph__SymmetricTensor(d, value):
    d.putNumChild(6)
    templateType = d.lookupType("double")
    diag = value["diag"]
    off = value["off"]
    siz = 8 # have to be changed to 4 for floats!
    sxx = d.createValue(diag.address(), templateType)
    syy = d.createValue(diag.address() + siz, templateType)
    szz = d.createValue(diag.address() + 2*siz, templateType)
    sxy = d.createValue(off.address(), templateType)
    sxz = d.createValue(off.address() + siz, templateType)
    syz = d.createValue(off.address() + 2*siz, templateType)
    if d.isExpanded():
        with Children(d, 6, childType=templateType):
            d.putSubItem("sxx", sxx)
            d.putSubItem("syy", syy)
            d.putSubItem("szz", szz)
            d.putSubItem("sxy", sxy)
            d.putSubItem("sxz", sxz)
            d.putSubItem("syz", syz)



