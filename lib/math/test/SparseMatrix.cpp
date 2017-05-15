#include "math/SparseMatrix.h"
#include "catch.hpp"
#include "utils/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

static void testAnyMatrix(const SparseMatrix::Solver solver) {
    SparseMatrix matrix(5);
    REQUIRE_ASSERT(matrix.solve(Array<Float>{ 1.f }, solver));
    Array<Float> b{ 1.f, 2.f, 3.f, 4.f, 5.f };
    REQUIRE_FALSE(matrix.solve(b, solver));

    float values[][5] = {
        { 1, 2, -1, 0, 5 }, { 4, 5, 0, 2, -1 }, { 1, 2, 4, -5, 1 }, { 0, -2, -3, -1, 2 }, { 4, 2, 1, 3, -2 }
    };
    for (Size i = 0; i < 5; ++i) {
        for (Size j = 0; j < 5; ++j) {
            matrix.insert(i, j, values[i][j]);
        }
    }

    if (solver == SparseMatrix::Solver::CG) {
        // CG only works for symmetric matrices
        REQUIRE_ASSERT(matrix.solve(b, solver));
        return;
    }
    Expected<Array<Float>> a = matrix.solve(b, solver);
    REQUIRE(a);

    Array<Float> expected{ 901._f / 356, -235._f / 178, -35._f / 356, -235._f / 356, 18._f / 89 };
    for (Size i = 0; i < 5; ++i) {
        REQUIRE(a.value()[i] == approx(expected[i]));
    }
}

static void testSymmetricMatrix(const SparseMatrix::Solver solver) {
    SparseMatrix matrix(5);
    float values[][5] = {
        { 1, 2, -1, 3, 5 }, { 2, 5, 0, 2, -1 }, { -1, 0, 4, -5, 1 }, { 3, 2, -5, -1, 2 }, { 5, -1, 1, 2, -2 }
    };
    for (Size i = 0; i < 5; ++i) {
        for (Size j = 0; j < 5; ++j) {
            matrix.insert(i, j, values[i][j]);
        }
    }
    Array<Float> b{ -5, 2, -4, 2, 3 };
    Expected<Array<Float>> a = matrix.solve(b, solver);
    REQUIRE(a);

    Array<Float> expected{ 1693._f / 5961, 123._f / 1987, -4027._f / 5961, -72._f / 1987, -7123._f / 5961 };

    for (Size i = 0; i < 5; ++i) {
        REQUIRE(a.value()[i] == approx(expected[i]));
    }
}

TEST_CASE("Invert matrix LU", "[sparsematrix]") {
    testAnyMatrix(SparseMatrix::Solver::LU);
    testSymmetricMatrix(SparseMatrix::Solver::LU);
}

TEST_CASE("Invert matrix CG", "[sparsematrix]") {
    testAnyMatrix(SparseMatrix::Solver::CG);
    testSymmetricMatrix(SparseMatrix::Solver::CG);
}

TEST_CASE("Invert matrix BICG", "[sparsematrix]") {
    testAnyMatrix(SparseMatrix::Solver::BI_CG);
    testSymmetricMatrix(SparseMatrix::Solver::BI_CG);
}
