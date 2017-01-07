#include "math/Morton.h"
#include "geometry/Box.h"

NAMESPACE_SPH_BEGIN

Size expandBits(Size v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

Size morton(const Vector& v) {
    const Vector u = v * 1024._f;
    const Size x = u[X];
    const Size y = u[Y];
    const Size z = u[Z];
    ASSERT(x >= 0 && x <= 1023);
    ASSERT(y >= 0 && y <= 1023);
    ASSERT(z >= 0 && z <= 1023);
    const Size xx = expandBits(x);
    const Size yy = expandBits(y);
    const Size zz = expandBits(z);
    return xx * 4 + yy * 2 + zz;
}

NAMESPACE_SPH_END
