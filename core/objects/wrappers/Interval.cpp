#include "objects/wrappers/Interval.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

// wrapper over float printing "infinity/-infinity" instead of value itself
struct Printer {
    Float value;

    friend std::ostream& operator<<(std::ostream& stream, const Printer w) {
        stream << std::setw(20);
        if (w.value == INFTY) {
            stream << "infinity";
        } else if (w.value == -INFTY) {
            stream << "-infinity";
        } else {
            stream << w.value;
        }
        return stream;
    }
};

std::ostream& operator<<(std::ostream& stream, const Interval& range) {
    stream << Printer{ range.lower() } << Printer{ range.upper() };
    return stream;
}

NAMESPACE_SPH_END
