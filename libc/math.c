#include <math.h>
#include <stdint.h>
#include <string.h>

#define TWO_52 4503599627370496.0

static uint64_t bits_of(double x)
{
    uint64_t bits;
    memcpy(&bits, &x, 8);
    return bits;
}

static double from_bits(uint64_t bits)
{
    double x;
    memcpy(&x, &bits, 8);
    return x;
}

int isnan(double x)
{
    return x != x;
}

int isinf(double x)
{
    return (bits_of(x) & 0x7FFFFFFFFFFFFFFFUL) == 0x7FF0000000000000UL;
}

int isfinite(double x)
{
    return (bits_of(x) & 0x7FF0000000000000UL) != 0x7FF0000000000000UL;
}

double sqrt(double x)
{
#ifdef __scc__
    asm("    movq xmm0, qword [rbp+16]\n"
        "    sqrtsd xmm0, xmm0\n"
        "    movq rax, xmm0\n"
        "    jmp .done");
    return x;
#else
    double root;
    __asm__("sqrtsd %1, %0" : "=x"(root) : "x"(x));
    return root;
#endif
}

double fabs(double x)
{
    return from_bits(bits_of(x) & 0x7FFFFFFFFFFFFFFFUL);
}

double trunc(double x)
{
    if (!(fabs(x) < TWO_52))
        return x;
    return (double)(long)x;
}

double floor(double x)
{
    double whole = trunc(x);
    return whole > x ? whole - 1 : whole;
}

double ceil(double x)
{
    double whole = trunc(x);
    return whole < x ? whole + 1 : whole;
}

double round(double x)
{
    return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5);
}

double fmod(double x, double y)
{
    if (y == 0 || !isfinite(x))
        return NAN;
    double quotient = trunc(x / y);
    return x - quotient * y;
}

double modf(double x, double *whole)
{
    *whole = trunc(x);
    return x - *whole;
}

double frexp(double x, int *exponent)
{
    *exponent = 0;
    if (x == 0 || !isfinite(x))
        return x;
    uint64_t bits = bits_of(x);
    int raw = (int)((bits >> 52) & 0x7FF);
    if (raw == 0) {
        x *= TWO_52;
        bits = bits_of(x);
        raw = (int)((bits >> 52) & 0x7FF) - 52;
    }
    *exponent = raw - 1022;
    bits = (bits & 0x800FFFFFFFFFFFFFUL) | 0x3FE0000000000000UL;
    return from_bits(bits);
}

double ldexp(double x, int exponent)
{
    while (exponent > 60) {
        x *= 1152921504606846976.0;
        exponent -= 60;
    }
    while (exponent < -60) {
        x /= 1152921504606846976.0;
        exponent += 60;
    }
    if (exponent >= 0)
        return x * (double)(1UL << exponent);
    return x / (double)(1UL << -exponent);
}

double exp(double x)
{
    if (isnan(x))
        return x;
    if (x > 709.78)
        return INFINITY;
    if (x < -745.2)
        return 0;
    double k = round(x / M_LN2);
    double r = x - k * M_LN2;
    double term = 1, sum = 1;
    for (int i = 1; i < 20; i++) {
        term *= r / i;
        sum += term;
    }
    return ldexp(sum, (int)k);
}

double log(double x)
{
    if (isnan(x) || x < 0)
        return NAN;
    if (x == 0)
        return -INFINITY;
    if (isinf(x))
        return x;
    int exponent;
    double m = frexp(x, &exponent);
    if (m < M_SQRT2 / 2) {
        m *= 2;
        exponent--;
    }
    double s = (m - 1) / (m + 1);
    double s2 = s * s, term = s, sum = 0;
    for (int i = 1; i < 40; i += 2) {
        sum += term / i;
        term *= s2;
    }
    return 2 * sum + exponent * M_LN2;
}

double log10(double x)
{
    return log(x) / M_LN10;
}

double log2(double x)
{
    return log(x) / M_LN2;
}

double pow(double x, double y)
{
    if (y == 0)
        return 1;
    if (isnan(x) || isnan(y))
        return NAN;
    double whole = trunc(y);
    if (whole == y && fabs(y) < 1e18) {
        long n = (long)y;
        int negative = n < 0;
        if (negative)
            n = -n;
        double result = 1, base = x;
        while (n) {
            if (n & 1)
                result *= base;
            base *= base;
            n >>= 1;
        }
        return negative ? 1 / result : result;
    }
    if (x < 0)
        return NAN;
    if (x == 0)
        return y > 0 ? 0 : INFINITY;
    return exp(y * log(x));
}

static double sin_kernel(double x)
{
    double x2 = x * x, term = x, sum = x;
    for (int i = 1; i < 12; i++) {
        term *= -x2 / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sum;
}

static double cos_kernel(double x)
{
    double x2 = x * x, term = 1, sum = 1;
    for (int i = 1; i < 12; i++) {
        term *= -x2 / ((2 * i - 1) * (2 * i));
        sum += term;
    }
    return sum;
}

static double reduce(double x, int *quadrant)
{
    double k = round(x / M_PI_2);
    *quadrant = (int)((long)k & 3);
    return (x - k * 1.5707963267341256) - k * 6.077100506506192e-11;
}

double sin(double x)
{
    if (!isfinite(x))
        return NAN;
    int quadrant;
    double r = reduce(x, &quadrant);
    switch (quadrant) {
    case 0: return sin_kernel(r);
    case 1: return cos_kernel(r);
    case 2: return -sin_kernel(r);
    }
    return -cos_kernel(r);
}

double cos(double x)
{
    if (!isfinite(x))
        return NAN;
    int quadrant;
    double r = reduce(x, &quadrant);
    switch (quadrant) {
    case 0: return cos_kernel(r);
    case 1: return -sin_kernel(r);
    case 2: return -cos_kernel(r);
    }
    return sin_kernel(r);
}

double tan(double x)
{
    return sin(x) / cos(x);
}

double atan(double x)
{
    if (isnan(x))
        return x;
    if (x < 0)
        return -atan(-x);
    if (x > 1)
        return M_PI_2 - atan(1 / x);
    double reduced = x / (1 + sqrt(1 + x * x));
    reduced = reduced / (1 + sqrt(1 + reduced * reduced));
    double x2 = reduced * reduced, term = reduced, sum = 0;
    for (int i = 0; i < 25; i++) {
        sum += term / (2 * i + 1);
        term *= -x2;
    }
    return 4 * sum;
}

double atan2(double y, double x)
{
    if (x > 0)
        return atan(y / x);
    if (x < 0)
        return y < 0 ? atan(y / x) - M_PI : atan(y / x) + M_PI;
    if (y > 0)
        return M_PI_2;
    if (y < 0)
        return -M_PI_2;
    return 0;
}

double asin(double x)
{
    if (x < -1 || x > 1)
        return NAN;
    return atan2(x, sqrt(1 - x * x));
}

double acos(double x)
{
    if (x < -1 || x > 1)
        return NAN;
    return atan2(sqrt(1 - x * x), x);
}

double sinh(double x)
{
    double e = exp(x);
    return (e - 1 / e) / 2;
}

double cosh(double x)
{
    double e = exp(x);
    return (e + 1 / e) / 2;
}

double tanh(double x)
{
    if (x > 20)
        return 1;
    if (x < -20)
        return -1;
    double e = exp(2 * x);
    return (e - 1) / (e + 1);
}

double hypot(double x, double y)
{
    return sqrt(x * x + y * y);
}

double fmin(double x, double y)
{
    return x < y ? x : y;
}

double fmax(double x, double y)
{
    return x > y ? x : y;
}
