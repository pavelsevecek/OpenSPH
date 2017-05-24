#!/bin/python

# Debugging helpers for custom types
# Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
# 2016-2017

from dumper import *

def qdump____m128(d, value):
    d.putEmptyValue()
    d.putNumChild(1)
    if d.isExpanded():
        d.putArrayData(value.address, 4, d.lookupType("float"))


def qdump____m128d(d, value):
    d.putEmptyValue()
    d.putNumChild(1)
    if d.isExpanded():
        d.putArrayData(value.address, 2, d.lookupType("double"))


def qdump__Sph__Array(d, value):  
    actsize = value["actSize"].integer()
    maxsize = value["maxSize"].integer()
    d.putNumChild(actsize)
    p = value["data"]
    innerType = p.type
    if d.isExpanded():
        with Children(d, actsize, childType=innerType): 
            for i in range(0, actsize):
                d.putSubItem(i, p.dereference()) # (d.addressOf(p) + i).dereference())
                p = p + 1

#    d.putValue("Array %s (%s)" % (actsize, maxsize))
#    d.putField("keeporder", "1")


#def qdump__Sph__Vector(d, value):
#    data = value["data"]
#    d.putValue(data[0])
#    d.putValue(data[1])

#    d.putItem(data.cast(FloatType))
    
#def qdump_Sph_TracelessTensor(d, value):
#    m = value["m"]
#    m12 = value["m12"]
