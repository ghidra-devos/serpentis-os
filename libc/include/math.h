#ifndef MATH_H
#define MATH_H

#define M_E 2.71828182845904523536
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_SQRT2 1.41421356237309504880
#define INFINITY (1e300 * 1e300)
#define HUGE_VAL INFINITY
#define NAN (INFINITY - INFINITY)

double sqrt(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);
double trunc(double x);
double round(double x);
double fmod(double x, double y);
double modf(double x, double *whole);
double frexp(double x, int *exponent);
double ldexp(double x, int exponent);
double exp(double x);
double log(double x);
double log10(double x);
double log2(double x);
double pow(double x, double y);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double hypot(double x, double y);
double fmin(double x, double y);
double fmax(double x, double y);
int isnan(double x);
int isinf(double x);
int isfinite(double x);

#endif
