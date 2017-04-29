#pragma once

/// Date and time routines.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "common/Globals.h"
#include "math/Math.h"
#include <string>

NAMESPACE_SPH_BEGIN

/// Input format of Julian date
enum class JulianDateFormat {
    JD,  /// (ordinary) Julian date, number of days since noon January 1, 4713 BC
    RJD, /// reduced Julian date, equals to JD - 2.400.000,0
    MJD  /// modified Julian date, equals to JD - 2.400.000,5
};

/// Helper class for transforming Julian date to calendar date
class DateFormat {
private:
    Float time;
    std::string outputFormat;

public:
    /// Construct object using Julian date
    /// \param value Numeric value of Julian date.
    /// \param inputFormat Format of input value, see JulianDateFormat enum
    /// \param outputFormat Format of output string. Replaces symbols with respective time value:
    ///   %Y  - year
    ///   %m  - month
    ///   %d  - day
    ///   %H  - hour
    ///   %M  - minute
    ///   %s  - second
    /// Example:
    ///   "%H:%M - %d. %m. %Y" returns something like "15:43 - 12. 11. 2016"
    DateFormat(const Float value, const JulianDateFormat inputFormat, const std::string& outputFormat);

    /// Returns the formatted string containing date/time
    std::string get() const;
};

NAMESPACE_SPH_END
