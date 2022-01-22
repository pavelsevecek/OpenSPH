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

Lut2D<EosTabValue> parseAneosFile(const Path& path) {
    if (path.empty()) {
        throw InvalidSetup("No ANEOS file specified");
    }
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
    }

    Array<Float> rhos = readValuesFromLine(ifs);
    if (rhos.size() != n_rho) {
        throw InvalidSetup("Inconsistent number of densities in the ANEOS file '" + path.string() + "'");
    }

    Array<Float> Ts = readValuesFromLine(ifs);
    if (Ts.size() != n_T) {
        throw InvalidSetup("Inconsistent number of temperatures in the ANEOS file '" + path.string() + "'");
    }

    Lut2D<EosTabValue> lut(n_T, n_rho, std::move(Ts), std::move(rhos));

    // read the rho-T table
    std::stringstream ss;
    for (Size i_T = 0; i_T < n_T; ++i_T) {
        for (Size i_rho = 0; i_rho < n_rho; ++i_rho) {
            if (!std::getline(ifs, line)) {
                throw InvalidSetup("Unexpected end of the ANEOS file '" + path.string() + "'");
            }

            ss.str(line);
            Float u, P, cs;
            ss >> u >> P >> cs;
            SPH_ASSERT(ss);

            lut.at(i_T, i_rho) = EosTabValue{ u, P, 1.e-3_f * cs }; // km/s -> m/s
        }
    }
    return lut;
}

Float getInitialDensity(const Lut2D<EosTabValue>& lut) {
    const Float T_ref = 200._f; // K
    const Float P_min = 1._f;   // Pa
    for (Size i = 0; i < lut.getValuesY().size() - 1; ++i) {
        const Float rho1 = lut.getValuesY()[i + 1];
        const Float P1 = lut.interpolate(T_ref, rho1).P;
        if (P1 > P_min) {
            return lut.getValuesY()[i];
        }
    }
    SPH_ASSERT(false); // no density in the range creates positive pressure?
    return lut.getValuesY()[lut.getValuesY().size() / 2];
}

static Lut2D<Aneos::TabValue> transposeLut(IScheduler& scheduler, const Lut2D<EosTabValue>& ilut) {
    const Array<Float>& Ts = ilut.getValuesX();
    const Array<Float>& rhos = ilut.getValuesY();
    const Size n_T = Ts.size();
    const Size n_rho = rhos.size();

    //  generate u values to tabulate
    Array<Float> us(n_T);
    for (Size i_T = 0; i_T < n_T; ++i_T) {
        us[i_T] = ilut.at(i_T, n_rho / 2).u;
    }

    // make sure the range contains all values
    Interval u_range;
    for (const EosTabValue& value : ilut.data()) {
        u_range.extend(value.u);
    }
    us.front() = u_range.lower();
    us.back() = u_range.upper();

    // transpose from (T, rho)->(u,P,cs) to (rho,u)->(T,P,cs)
    Lut2D<Aneos::TabValue> lut(n_rho, n_T, rhos.clone(), us.clone());
    parallelFor(scheduler, 0, n_rho, 1, [&](const Size i_rho) {
        for (Size i_u = 0; i_u < us.size(); ++i_u) {
            // find the corresponding energy in the LUT
            const Float u = us[i_u];
            ArrayView<const EosTabValue> row = ilut.row(i_rho);
            const auto iter = std::upper_bound(
                row.begin(), row.end(), u, [](const Float u, const EosTabValue& iv) { return u < iv.u; });
            const Size i_T2 = Size(iter - row.begin());
            if (i_T2 > 0 && i_T2 < us.size()) {
                const Size i_T1 = i_T2 - 1;
                const EosTabValue iv1 = row[i_T1];
                const EosTabValue iv2 = row[i_T2];
                const Float u1 = iv1.u;
                const Float u2 = iv2.u;
                SPH_ASSERT(u1 <= u && u <= u2, u1, u, u2);
                const Float f = u2 != u1 ? 1._f - (u - u1) / (u2 - u1) : 0._f;

                const Float P = lerp(iv1.P, iv2.P, f);
                const Float cs = lerp(iv1.cs, iv2.cs, f);
                const Float T = lerp(Ts[i_T1], Ts[i_T2], f);
                lut.at(i_rho, i_u) = Aneos::TabValue{ P, cs, T };
            } else if (i_T2 == 0) {
                const EosTabValue iv = row.front();
                lut.at(i_rho, i_u) = Aneos::TabValue{ iv.P, iv.cs, Ts.front() };
            } else {
                SPH_ASSERT(i_T2 == us.size());
                const EosTabValue iv = row.back();
                lut.at(i_rho, i_u) = Aneos::TabValue{ iv.P, iv.cs, Ts.back() };
            }
        }
    });
    return lut;
}


//-----------------------------------------------------------------------------------------------------------
// Aneos implementation
//-----------------------------------------------------------------------------------------------------------


Aneos::Aneos(const Path& path) {
    Lut2D<EosTabValue> ilut = parseAneosFile(path);
    SharedPtr<IScheduler> scheduler = Factory::getScheduler();
    lut = transposeLut(*scheduler, ilut);
}

Pair<Float> Aneos::evaluate(const Float rho, const Float u) const {
    const TabValue value = lut.interpolate(rho, u);
    return { value.P, value.cs };
}

Float Aneos::getTemperature(const Float rho, const Float u) const {
    return lut.interpolate(rho, u).T;
}

NAMESPACE_SPH_END
