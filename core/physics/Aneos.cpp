#include "physics/Aneos.h"
#include "io/Path.h"
#include "objects/containers/String.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "thread/Scheduler.h"
#include <fstream>
#include <sstream>

NAMESPACE_SPH_BEGIN

Aneos::TabValue Aneos::TabValue::operator+(const TabValue& other) const {
    return { P + other.P, cs + other.cs, T + other.T };
}

Aneos::TabValue Aneos::TabValue::operator*(const Float f) const {
    return { P * f, cs * f, T * f };
}

namespace {

struct TRhoValue {
    Float u;
    Float P;
    Float cs;

    TRhoValue operator+(const TRhoValue& other) const {
        return { u + other.u, P + other.P, cs + other.cs };
    }

    TRhoValue operator*(const Float f) const {
        return { u * f, P * f, cs * f };
    }
};

Array<Float> readValuesFromLine(std::ifstream& ifs) {
    std::string line;
    std::getline(ifs, line);
    std::stringstream ss(line);
    Array<Float> values;
    while (true) {
        Float value;
        ss >> value;
        if (ss) {
            values.push(value);
        } else {
            break;
        }
    }
    return values;
}

} // namespace

//-----------------------------------------------------------------------------------------------------------
// Aneos implementation
//-----------------------------------------------------------------------------------------------------------

Lut2D<Aneos::TabValue> Aneos::parseAneosFile(IScheduler& scheduler, const Path& path) {
    std::ifstream ifs(path.native());
    std::string line;
    while (std::getline(ifs, line)) {
        String s = String::fromAscii(line.c_str()).trim();
        if (s.empty() || s[0] == '#') {
            continue;
        }
        // first non-empty line contains the file date, skip
        break;
    }

    Size n_rho, n_T;
    {
        std::getline(ifs, line);
        std::stringstream ss(line);
        ss >> n_rho >> n_T;
        if (!ifs) {
            throw InvalidSetup("Failed reading dimensions from the ANEOS file '" + path.string() + "'");
        }
        Size dummy;
        ss >> dummy;
        if (ifs) { // did not hit end of line, probably a different file format
            throw InvalidSetup("Unexpected file format of the ANEOS file '" + path.string() + "'");
        }
    }

    Array<Float> rhos = readValuesFromLine(ifs);
    if (rhos.size() != n_rho) {
        throw InvalidSetup("Inconsistent number of densities in the ANEOS file '" + path.string() + "'");
    }

    Array<Float> Ts = readValuesFromLine(ifs);
    if (Ts.size() != n_T) {
        throw InvalidSetup("Inconsistent number of temperatures in the ANEOS file '" + path.string() + "'");
    }

    Lut2D<TRhoValue> ilut(n_T, n_rho, Ts.clone(), rhos.clone());

    // read the rho-T table
    std::stringstream ss;
    Interval u_range;
    for (Size i_T = 0; i_T < n_T; ++i_T) {
        for (Size i_rho = 0; i_rho < n_rho; ++i_rho) {
            if (!std::getline(ifs, line)) {
                throw InvalidSetup("Unexpected end of the ANEOS file '" + path.string() + "'");
            }

            ss.str(line);
            Float u, P, cs;
            ss >> u >> P >> cs;
            SPH_ASSERT(ss);

            ilut.at(i_T, i_rho) = TRhoValue{ u, P, 1.e-3_f * cs }; // km/s -> m/s
            u_range.extend(u);
        }
    }

    //  generate u values to tabulate
    Array<Float> us(n_T);
    for (Size i_T = 0; i_T < n_T; ++i_T) {
        us[i_T] = ilut.at(i_T, n_rho / 2).u;
    }
    // make sure the range contains all values
    us.front() = u_range.lower();
    us.back() = u_range.upper();

    // transpose from (T, rho)->(u,P,cs) to (rho,u)->(T,P,cs)
    Lut2D<TabValue> lut(n_rho, n_T, rhos.clone(), us.clone());
    parallelFor(scheduler, 0, n_rho, 1, [&](const Size i_rho) {
        for (Size i_u = 0; i_u < us.size(); ++i_u) {
            // find the corresponding energy in the LUT
            const Float u = us[i_u];
            ArrayView<const TRhoValue> row = ilut.row(i_rho);
            const auto iter = std::upper_bound(
                row.begin(), row.end(), u, [](const Float u, const TRhoValue& iv) { return u < iv.u; });
            SPH_ASSERT(iter != row.begin());
            const Size i_T2 = iter - row.begin();
            const Size i_T1 = i_T2 - 1;
            const TRhoValue iv1 = row[i_T1];
            const TRhoValue iv2 = row[i_T2];
            const Float u1 = iv1.u;
            const Float u2 = iv2.u;
            SPH_ASSERT(u1 <= u && u < u2, u1, u, u2);
            const Float f = 1._f - (u - u1) / (u2 - u1);

            const Float P = lerp(iv1.P, iv2.P, f);
            const Float cs = lerp(iv1.cs, iv2.cs, f);
            const Float T = lerp(Ts[i_T1], Ts[i_T2], f);
            lut.at(i_rho, i_u) = TabValue{ P, cs, T };
        }
    });
    return lut;
}

Aneos::Aneos(const BodySettings& settings) {
    const Path path(settings.get<String>(BodySettingsId::ANEOS_FILE));
    if (path.empty()) {
        throw InvalidSetup("No ANEOS file specified");
    }
    SharedPtr<IScheduler> scheduler = Factory::getScheduler();
    lut = parseAneosFile(*scheduler, path);
}

Pair<Float> Aneos::evaluate(const Float rho, const Float u) const {
    const TabValue value = lut.interpolate(rho, u);
    return { value.P, value.cs };
}

Float Aneos::getTemperature(const Float rho, const Float u) const {
    return lut.interpolate(rho, u).T;
}

NAMESPACE_SPH_END
