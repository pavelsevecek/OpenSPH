#include "objects/geometry/SymmetricTensor.h"

NAMESPACE_SPH_BEGIN


SymmetricTensor SymmetricTensor::pseudoInverse(const Float eps) const {
    Svd svd = singularValueDecomposition(*this);
    for (Size i = 0; i < 3; ++i) {
        if (svd.S[i] < eps) {
            svd.S[i] = 0._f;
        } else {
            svd.S[i] = 1._f / svd.S[i];
        }
    }
    AffineMatrix S = AffineMatrix::scale(svd.S);
    AffineMatrix result = svd.V * S * svd.U.transpose();
    // result is ALMOST symmetric, but symmetric tensor assumes ABSOLUTELY symmetric input
    /*ASSERT(almostEqual(result(0, 1), result(1, 0), 1.e-4f), result);
    ASSERT(almostEqual(result(0, 2), result(2, 0), 1.e-4f), result);
    ASSERT(almostEqual(result(1, 2), result(2, 1), 1.e-4f), result);*/
    return SymmetricTensor(
        Vector(result(0, 0), result(1, 1), result(2, 2)), Vector(result(0, 1), result(0, 2), result(1, 2)));
}

constexpr int n = 3;

INLINE static double hypot2(double x, double y) {
    ASSERT(isReal(x) && isReal(y), x, y);
    return sqrt(x * x + y * y);
}

// Symmetric Householder reduction to tridiagonal form.

static void tred2(double V[n][n], double d[n], double e[n]) {
    //  This is derived from the Algol procedures tred2 by
    //  Bowdler, Martin, Reinsch, and Wilkinson, Handbook for
    //  Auto. Comp., Vol.ii-Linear Algebra, and the corresponding
    //  Fortran subroutine in EISPACK.
    for (int j = 0; j < n; j++) {
        d[j] = V[n - 1][j];
    }
    // Householder reduction to tridiagonal form.
    for (int i = n - 1; i > 0; i--) {
        // Scale to avoid under/overflow.
        double scale = 0.0;
        double h = 0.0;
        for (int k = 0; k < i; k++) {
            scale = scale + fabs(d[k]);
        }
        if (scale == 0.0) {
            e[i] = d[i - 1];
            for (int j = 0; j < i; j++) {
                d[j] = V[i - 1][j];
                V[i][j] = 0.0;
                V[j][i] = 0.0;
            }
        } else {
            // Generate Householder vector.
            for (int k = 0; k < i; k++) {
                d[k] /= scale;
                h += d[k] * d[k];
            }
            double f = d[i - 1];
            ASSERT(h >= 0._f, h);
            double g = sqrt(h);
            if (f > 0) {
                g = -g;
            }
            e[i] = scale * g;
            h = h - f * g;
            d[i - 1] = f - g;
            for (int j = 0; j < i; j++) {
                e[j] = 0.0;
            }

            // Apply similarity transformation to remaining columns.
            for (int j = 0; j < i; j++) {
                f = d[j];
                V[j][i] = f;
                g = e[j] + V[j][j] * f;
                for (int k = j + 1; k <= i - 1; k++) {
                    g += V[k][j] * d[k];
                    e[k] += V[k][j] * f;
                }
                e[j] = g;
            }
            f = 0.0;
            ASSERT(h != 0._f);
            for (int j = 0; j < i; j++) {
                e[j] /= h;
                f += e[j] * d[j];
            }
            double hh = f / (h + h);
            for (int j = 0; j < i; j++) {
                e[j] -= hh * d[j];
            }
            for (int j = 0; j < i; j++) {
                f = d[j];
                g = e[j];
                for (int k = j; k <= i - 1; k++) {
                    V[k][j] -= (f * e[k] + g * d[k]);
                }
                d[j] = V[i - 1][j];
                V[i][j] = 0.0;
            }
        }
        d[i] = h;
    }

    // Accumulate transformations.
    for (int i = 0; i < n - 1; i++) {
        V[n - 1][i] = V[i][i];
        V[i][i] = 1.0;
        double h = d[i + 1];
        if (h != 0.0) {
            for (int k = 0; k <= i; k++) {
                d[k] = V[k][i + 1] / h;
            }
            for (int j = 0; j <= i; j++) {
                double g = 0.0;
                for (int k = 0; k <= i; k++) {
                    g += V[k][i + 1] * V[k][j];
                }
                for (int k = 0; k <= i; k++) {
                    V[k][j] -= g * d[k];
                }
            }
        }
        for (int k = 0; k <= i; k++) {
            V[k][i + 1] = 0.0;
        }
    }
    for (int j = 0; j < n; j++) {
        d[j] = V[n - 1][j];
        V[n - 1][j] = 0.0;
    }
    V[n - 1][n - 1] = 1.0;
    e[0] = 0.0;
}

// Symmetric tridiagonal QL algorithm.
static void tql2(double V[n][n], double d[n], double e[n]) {
    //  This is derived from the Algol procedures tql2, by
    //  Bowdler, Martin, Reinsch, and Wilkinson, Handbook for
    //  Auto. Comp., Vol.ii-Linear Algebra, and the corresponding
    //  Fortran subroutine in EISPACK.
    for (int i = 1; i < n; i++) {
        e[i - 1] = e[i];
    }
    e[n - 1] = 0.0;

    double f = 0.0;
    double tst1 = 0.0;
    double eps = pow(2.0, -52.0);
    for (int l = 0; l < n; l++) {
        // Find small subdiagonal element
        tst1 = max(tst1, fabs(d[l]) + fabs(e[l]));
        int m = l;
        while (m < n) {
            if (fabs(e[m]) <= eps * tst1) {
                break;
            }
            m++;
        }

        // If m == l, d[l] is an eigenvalue,
        // otherwise, iterate.
        if (m > l) {
            int iter = 0;
            do {
                ++iter;
                // Compute implicit shift
                double g = d[l];
                double p = (d[l + 1] - g) / (2.0 * e[l]);
                ASSERT(isReal(p), p, e[l], t);
                double r = hypot2(p, 1.0);
                if (p < 0) {
                    r = -r;
                }
                d[l] = e[l] / (p + r);
                d[l + 1] = e[l] * (p + r);
                double dl1 = d[l + 1];
                double h = g - d[l];
                for (int i = l + 2; i < n; i++) {
                    d[i] -= h;
                }
                f = f + h;

                // Implicit QL transformation.
                p = d[m];
                ASSERT(isReal(p), p, t);
                double c = 1.0;
                double c2 = c;
                double c3 = c;
                double el1 = e[l + 1];
                double s = 0.0;
                double s2 = 0.0;
                for (int i = m - 1; i >= l; i--) {
                    c3 = c2;
                    c2 = c;
                    s2 = s;
                    g = c * e[i];
                    h = c * p;
                    r = hypot2(p, e[i]);
                    e[i + 1] = s * r;
                    s = e[i] / r;
                    c = p / r;
                    p = c * d[i] - s * g;
                    ASSERT(isReal(p), p);
                    d[i + 1] = h + s * (c * g + s * d[i]);

                    // Accumulate transformation.
                    for (int k = 0; k < n; k++) {
                        h = V[k][i + 1];
                        V[k][i + 1] = s * V[k][i] + c * h;
                        V[k][i] = c * V[k][i] - s * h;
                    }
                }
                p = -s * s2 * c3 * el1 * e[l] / dl1;
                e[l] = s * p;
                d[l] = c * p;
            } while (fabs(e[l]) > eps * tst1);
        }
        d[l] = d[l] + f;
        e[l] = 0.0;
    }
    // Sort eigenvalues and corresponding vectors.
    for (int i = 0; i < n - 1; i++) {
        int k = i;
        double p = d[i];
        for (int j = i + 1; j < n; j++) {
            if (d[j] < p) {
                k = j;
                p = d[j];
            }
        }
        if (k != i) {
            d[k] = d[i];
            d[i] = p;
            for (int j = 0; j < n; j++) {
                p = V[j][i];
                V[j][i] = V[j][k];
                V[j][k] = p;
            }
        }
    }
}

Eigen eigenDecomposition(const SymmetricTensor& t) {
    ASSERT(isReal(t), t);
    const Float scale = maxElement(abs(t));
    if (scale < 1.e-20_f) {
        // algorithm is unstable for very small values, just return the diagonal elements + identity matrix
        return Eigen{ AffineMatrix::identity(), t.diagonal() };
    }
    ASSERT(isReal(scale));

    double e[n];
    double d[n];
    double V[n][n];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            V[i][j] = t(i, j);
        }
    }
    tred2(V, d, e);
    tql2(V, d, e);
    return { AffineMatrix(Vector(V[0][0], V[1][0], V[2][0]),
                 Vector(V[0][1], V[1][1], V[2][1]),
                 Vector(V[0][2], V[1][2], V[2][2])),
        Vector(d[0], d[1], d[2]) };
}

INLINE static double SIGN(double a, double b) {
    return b >= 0.0 ? fabs(a) : -fabs(a);
}

INLINE static double PYTHAG(double a, double b) {
    double at = fabs(a), bt = fabs(b), ct, result;

    if (at > bt) {
        ct = bt / at;
        ASSERT(isReal(ct));
        result = at * sqrt(1.0 + ct * ct);
    } else if (bt > 0.0) {
        ct = at / bt;
        ASSERT(isReal(ct));
        result = bt * sqrt(1.0 + ct * ct);
    } else {
        result = 0.0;
    }
    return result;
}


int dsvd(float (&a)[3][3], float* w, float (&v)[3][3]) {
    const int m = 3, n = 3;
    int flag, i, its, j, jj, k, l, nm;
    double c, f, h, s, x, y, z;
    double anorm = 0.0, g = 0.0, scale = 0.0;
    double* rv1;

    ASSERT(m >= n, "#rows must be > #cols");

    rv1 = (double*)malloc((unsigned int)n * sizeof(double));

    /* Householder reduction to bidiagonal form */
    for (i = 0; i < n; i++) {
        /* left-hand reduction */
        l = i + 1;
        rv1[i] = scale * g;
        g = s = scale = 0.0;
        if (i < m) {
            for (k = i; k < m; k++)
                scale += fabs((double)a[k][i]);
            if (scale) {
                for (k = i; k < m; k++) {
                    a[k][i] = (float)((double)a[k][i] / scale);
                    s += ((double)a[k][i] * (double)a[k][i]);
                }
                ASSERT(s >= 0._f, s);
                f = (double)a[i][i];
                g = -SIGN(sqrt(s), f);
                h = f * g - s;
                a[i][i] = (float)(f - g);
                if (i != n - 1) {
                    for (j = l; j < n; j++) {
                        for (s = 0.0, k = i; k < m; k++)
                            s += ((double)a[k][i] * (double)a[k][j]);
                        f = s / h;
                        for (k = i; k < m; k++)
                            a[k][j] += (float)(f * (double)a[k][i]);
                    }
                }
                for (k = i; k < m; k++)
                    a[k][i] = (float)((double)a[k][i] * scale);
            }
        }
        w[i] = (float)(scale * g);

        /* right-hand reduction */
        g = s = scale = 0.0;
        if (i < m && i != n - 1) {
            for (k = l; k < n; k++)
                scale += fabs((double)a[i][k]);
            if (scale) {
                for (k = l; k < n; k++) {
                    a[i][k] = (float)((double)a[i][k] / scale);
                    s += ((double)a[i][k] * (double)a[i][k]);
                }
                ASSERT(s >= 0._f, s);
                f = (double)a[i][l];
                g = -SIGN(sqrt(s), f);
                h = f * g - s;
                a[i][l] = (float)(f - g);
                for (k = l; k < n; k++)
                    rv1[k] = (double)a[i][k] / h;
                if (i != m - 1) {
                    for (j = l; j < m; j++) {
                        for (s = 0.0, k = l; k < n; k++)
                            s += ((double)a[j][k] * (double)a[i][k]);
                        for (k = l; k < n; k++)
                            a[j][k] += (float)(s * rv1[k]);
                    }
                }
                for (k = l; k < n; k++)
                    a[i][k] = (float)((double)a[i][k] * scale);
            }
        }
        anorm = max(anorm, (fabs((double)w[i]) + fabs(rv1[i])));
    }

    /* accumulate the right-hand transformation */
    for (i = n - 1; i >= 0; i--) {
        if (i < n - 1) {
            if (g) {
                for (j = l; j < n; j++)
                    v[j][i] = (float)(((double)a[i][j] / (double)a[i][l]) / g);
                /* double division to avoid underflow */
                for (j = l; j < n; j++) {
                    for (s = 0.0, k = l; k < n; k++)
                        s += ((double)a[i][k] * (double)v[k][j]);
                    for (k = l; k < n; k++)
                        v[k][j] += (float)(s * (double)v[k][i]);
                }
            }
            for (j = l; j < n; j++)
                v[i][j] = v[j][i] = 0.0;
        }
        v[i][i] = 1.0;
        g = rv1[i];
        l = i;
    }

    /* accumulate the left-hand transformation */
    for (i = n - 1; i >= 0; i--) {
        l = i + 1;
        g = (double)w[i];
        if (i < n - 1)
            for (j = l; j < n; j++)
                a[i][j] = 0.0;
        if (g) {
            g = 1.0 / g;
            if (i != n - 1) {
                for (j = l; j < n; j++) {
                    for (s = 0.0, k = l; k < m; k++)
                        s += ((double)a[k][i] * (double)a[k][j]);
                    f = (s / (double)a[i][i]) * g;
                    for (k = i; k < m; k++)
                        a[k][j] += (float)(f * (double)a[k][i]);
                }
            }
            for (j = i; j < m; j++)
                a[j][i] = (float)((double)a[j][i] * g);
        } else {
            for (j = i; j < m; j++)
                a[j][i] = 0.0;
        }
        ++a[i][i];
    }

    /* diagonalize the bidiagonal form */
    for (k = n - 1; k >= 0; k--) {       /* loop over singular values */
        for (its = 0; its < 30; its++) { /* loop over allowed iterations */
            flag = 1;
            for (l = k; l >= 0; l--) { /* test for splitting */
                nm = l - 1;
                if (fabs(rv1[l]) + anorm == anorm) {
                    flag = 0;
                    break;
                }
                if (fabs((double)w[nm]) + anorm == anorm)
                    break;
            }
            if (flag) {
                c = 0.0;
                s = 1.0;
                for (i = l; i <= k; i++) {
                    f = s * rv1[i];
                    if (fabs(f) + anorm != anorm) {
                        g = (double)w[i];
                        h = PYTHAG(f, g);
                        w[i] = (float)h;
                        h = 1.0 / h;
                        c = g * h;
                        s = (-f * h);
                        for (j = 0; j < m; j++) {
                            y = (double)a[j][nm];
                            z = (double)a[j][i];
                            a[j][nm] = (float)(y * c + z * s);
                            a[j][i] = (float)(z * c - y * s);
                        }
                    }
                }
            }
            z = (double)w[k];
            if (l == k) {      /* convergence */
                if (z < 0.0) { /* make singular value nonnegative */
                    w[k] = (float)(-z);
                    for (j = 0; j < n; j++)
                        v[j][k] = (-v[j][k]);
                }
                break;
            }
            ASSERT(its < 30, "No convergence after 30,000! iterations ");


            /* shift from bottom 2 x 2 minor */
            x = (double)w[l];
            nm = k - 1;
            y = (double)w[nm];
            g = rv1[nm];
            h = rv1[k];
            f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
            g = PYTHAG(f, 1.0);
            f = ((x - z) * (x + z) + h * ((y / (f + SIGN(g, f))) - h)) / x;

            /* next QR transformation */
            c = s = 1.0;
            for (j = l; j <= nm; j++) {
                i = j + 1;
                g = rv1[i];
                y = (double)w[i];
                h = s * g;
                g = c * g;
                z = PYTHAG(f, h);
                rv1[j] = z;
                c = f / z;
                s = h / z;
                f = x * c + g * s;
                g = g * c - x * s;
                h = y * s;
                y = y * c;
                for (jj = 0; jj < n; jj++) {
                    x = (double)v[jj][j];
                    z = (double)v[jj][i];
                    v[jj][j] = (float)(x * c + z * s);
                    v[jj][i] = (float)(z * c - x * s);
                }
                z = PYTHAG(f, h);
                w[j] = (float)z;
                if (z) {
                    z = 1.0 / z;
                    c = f * z;
                    s = h * z;
                }
                f = (c * g) + (s * y);
                x = (c * y) - (s * g);
                for (jj = 0; jj < m; jj++) {
                    y = (double)a[jj][j];
                    z = (double)a[jj][i];
                    a[jj][j] = (float)(y * c + z * s);
                    a[jj][i] = (float)(z * c - y * s);
                }
            }
            rv1[l] = 0.0;
            rv1[k] = f;
            w[k] = (float)x;
        }
    }
    free((void*)rv1);
    return (1);
}


Svd singularValueDecomposition(const SymmetricTensor& t) {
    Vector diag = t.diagonal();
    Vector off = t.offDiagonal();
    float V[3][3];
    float S[3];

    // clang-format off
    float U[3][3] = {
        { float(diag[0]), float(off[0]),  float(off[1]) },
        { float(off[0]),  float(diag[1]), float(off[2]) },
        { float(off[1]),  float(off[2]),  float(diag[2]) },
    };

    dsvd(U, S, V);

    Svd result;
    result.S = Vector(S[0], S[1], S[2]);
    result.U = AffineMatrix(Vector(U[0][0], U[0][1], U[0][2]),
                            Vector(U[1][0], U[1][1], U[1][2]),
                            Vector(U[2][0], U[2][1], U[2][2]));
    result.V = AffineMatrix(Vector(V[0][0], V[0][1], V[0][2]),
                            Vector(V[1][0], V[1][1], V[1][2]),
                            Vector(V[2][0], V[2][1], V[2][2]));
    // clang-format on
    return result;
}

NAMESPACE_SPH_END
