#include "run/SpecialEntries.h"

NAMESPACE_SPH_BEGIN

String CurveEntry::toString() const {
    std::stringstream ss;
    for (Size i = 0; i < curve.getPointCnt(); ++i) {
        const CurvePoint p = curve.getPoint(i);
        ss << p.x << " " << p.y << " ";
        if (i < curve.getPointCnt() - 1) {
            ss << curve.getSegment(i) << " ";
        }
    }
    return String::fromAscii(ss.str().c_str());
}

void CurveEntry::fromString(const String& s) {
    std::stringstream ss(std::string(s.toAscii()));
    Array<CurvePoint> points;
    Array<bool> flags;
    while (ss) {
        CurvePoint p;
        ss >> p.x >> p.y;
        points.push(p);

        bool f;
        ss >> f;
        flags.push(f);
    }
    curve = Curve(std::move(points));
    for (Size i = 0; i < flags.size() - 1; ++i) {
        curve.setSegment(i, flags[i]);
    }
}

AutoPtr<IExtraEntry> CurveEntry::clone() const {
    return makeAuto<CurveEntry>(curve);
}

NAMESPACE_SPH_END
