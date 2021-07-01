#include "math/Morton.h"
#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

// https://devblogs.nvidia.com/parallelforall/thinking-parallel-part-iii-tree-construction-gpu/
Size expandBits(Size v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

Size morton(const Vector& v) {
    const Vector u = v * 1024._f;
    const int x = int(u[X]);
    const int y = int(u[Y]);
    const int z = int(u[Z]);
    SPH_ASSERT(x >= 0 && x <= 1023);
    SPH_ASSERT(y >= 0 && y <= 1023);
    SPH_ASSERT(z >= 0 && z <= 1023);
    const int xx = expandBits(x);
    const int yy = expandBits(y);
    const int zz = expandBits(z);
    return xx * 4 + yy * 2 + zz;
}

NAMESPACE_SPH_END
