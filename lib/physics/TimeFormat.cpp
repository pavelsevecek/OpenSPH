#include "physics/TimeFormat.h"
#include <iomanip>
#include <sstream>

NAMESPACE_SPH_BEGIN

DateFormat::DateFormat(const Float value, const JulianDateFormat inputFormat, const std::string& outputFormat)
    : outputFormat(outputFormat) {
    // convert input format to JD
    switch (inputFormat) {
    case JulianDateFormat::JD:
        time = value;
        break;
    case JulianDateFormat::RJD:
        time = value + Float(2'400'000. * 60. * 60. * 24.);
        break;
    case JulianDateFormat::MJD:
        time = value + Float(2'400'000.5 * 60. * 60. * 24.);
        break;
    }
}

std::string DateFormat::get() const {
    const char* format = outputFormat.c_str();
    std::string output;

    // table from wikipedia
    const int y = 4716, j = 1401, m = 2, n = 12, r = 4, p = 1461, v = 3, u = 5, s = 153, w = 2, B = 274'277,
              C = -38;
    /*UnitSystem<T> daySystem = Units::SI<T>; // make the conversion a little bit more elegant
    daySystem.time          = (1._days).value();*/

    const Float timeInDays = time / (60. * 60. * 24.) + 0.5_f;
    const Float remainder = timeInDays - floor(timeInDays); // + T(0.5);
    const int J = int(timeInDays);

    const int f = J + j + (((4 * J + B) / 146097) * 3) / 4 + C;
    const int e = r * f + v;
    const int g = (e % p) / r;
    const int h = u * g + w;

    const int D = (h % s) / u + 1;
    const int M = (h / s + m) % n + 1;
    const int Y = (e / p) - y + (n + m - M) / n;

    for (const char* c = format; *c; ++c) {
        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0');
        if (*c == '%') {
            c++;
            ASSERT(*c != 0); // invalid format
            switch (*c) {
            case 'Y': // year
                ss << Y;
                break;
            case 'm': // year
                ss << M;
                break;
            case 'd': // year
                ss << D;
                break;
            case 'H': // hour
                ss << int(remainder * 24) % 24;
                break;
            case 'M': // minute
                ss << int(remainder * 24 * 60) % 60;
                break;
            case 's': // second
                ss << int(remainder * 24 * 60 * 60) % 60;
                break;
            }
            output += ss.str();
        } else {
            output += *c;
        }
    }
    return output;
}

NAMESPACE_SPH_END
