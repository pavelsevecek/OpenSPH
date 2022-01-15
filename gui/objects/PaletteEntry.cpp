#include "gui/objects/PaletteEntry.h"

NAMESPACE_SPH_BEGIN

String PaletteEntry::toString() const {
    std::stringstream ss;
    ss << palette.getInterval() << ";" << int(palette.getScale()) << ";";
    ArrayView<const Palette::Point> points = palette.getPoints();
    for (const Palette::Point& p : points) {
        ss << p.value << " " << p.color.r() << " " << p.color.g() << " " << p.color.b() << ";";
    }
    return String::fromAscii(ss.str().c_str());
}

void PaletteEntry::fromString(const String& s) {
    Array<String> parts = split(s, L';');
    if (parts.size() < 3) {
        // malformed string
        SPH_ASSERT(false, s);
        return;
    }

    std::stringstream ss;
    ss.str(parts[0].toAscii().cstr());
    Float from, to;
    ss >> from >> to;
    ss.clear();

    ss.str(parts[1].toAscii().cstr());
    int scale;
    ss >> scale;
    ss.clear();

    Array<Palette::Point> points;
    for (Size i = 2; i < parts.size() - 1; ++i) {
        ss.str(parts[i].toAscii().cstr());
        float value, r, g, b;
        ss >> value >> r >> g >> b;
        points.push(Palette::Point{ value, Rgba(r, g, b) });
        ss.clear();
    }

    palette = Palette(std::move(points), Interval(from, to), PaletteScale(scale));
    /*    split()
        expanded.replaceAll(";", "\n");
        StringTextInputStream ss(expanded);
        palette.loadFromStream(ss);*/
}


NAMESPACE_SPH_END
