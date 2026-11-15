#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct point {
    double x;
    double y;
};

static int checks;
static int failures;
static const double table[] = {0.5, 1.25, -3.75};
static float ratio = 0.75f;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("failed: %s\n", what);
    }
}

static int near(double a, double b)
{
    double difference = a - b;
    if (difference < 0)
        difference = -difference;
    return difference < 1e-9 * (fabs(b) > 1 ? fabs(b) : 1);
}

static double average(int count, ...)
{
    va_list list;
    va_start(list, count);
    double total = 0;
    for (int i = 0; i < count; i++)
        total += va_arg(list, double);
    va_end(list);
    return total / count;
}

static struct point midpoint(struct point a, struct point b)
{
    struct point m = {(a.x + b.x) / 2, (a.y + b.y) / 2};
    return m;
}

static float half(float value)
{
    return value / 2;
}

int main(void)
{
    char text[64];

    double a = 1.5, b = 2.25;
    check(a + b == 3.75 && a * b == 3.375 && b - a == 0.75 && b / a == 1.5, "double arithmetic");
    check(a < b && b > a && a <= 1.5 && a != b && !(a == b), "double comparisons");
    int whole = (int)7.9;
    double widened = 7;
    check(whole == 7 && widened == 7.0 && (long)-2.5 == -2, "conversions");
    unsigned long big = 18000000000000000000UL;
    check((double)big > 1.7e19 && (double)big < 1.9e19, "unsigned to double");
    check(table[2] == -3.75 && ratio == 0.75f && sizeof(float) == 4, "constant initializers");
    check(half(3.0f) == 1.5f, "float functions");
    float f = 0.1f;
    double promoted = f;
    check(promoted > 0.0999 && promoted < 0.1001, "float to double");
    check(average(4, 1.0, 2.0, 3.0, 4.0) == 2.5, "variadic doubles");
    struct point m = midpoint((struct point){0, 0}, (struct point){4, 6});
    check(m.x == 2 && m.y == 3, "doubles in structs");
    double counter = 0;
    for (int i = 0; i < 10; i++)
        counter += 0.5;
    counter++;
    check(counter == 6, "loops and increments");
    check(-a == -1.5 && !0.0 && 0.5, "negation and truth");

    check(near(sqrt(2), M_SQRT2), "sqrt");
    check(near(sin(M_PI / 6), 0.5) && near(cos(M_PI / 3), 0.5) && near(tan(M_PI / 4), 1), "trigonometry");
    check(near(atan2(1, 1), M_PI_4) && near(asin(1), M_PI_2) && near(acos(0), M_PI_2), "inverse trigonometry");
    check(near(exp(1), M_E) && near(log(M_E), 1) && near(log10(1000), 3) && near(pow(2, 10), 1024), "exp and log");
    check(near(pow(2, 0.5), M_SQRT2) && floor(-1.5) == -2 && ceil(1.2) == 2 && round(2.5) == 3, "pow and rounding");
    check(near(fmod(7.5, 2), 1.5) && fabs(-3.5) == 3.5 && isnan(NAN) && isinf(INFINITY), "fmod and classes");

    snprintf(text, sizeof text, "%.2f|%8.3f|%e|%g|%g", 3.14159, -2.5, 12345.678, 0.0001, 100000.0);
    check(!strcmp(text, "3.14|  -2.500|1.234568e+04|0.0001|100000"), "printf floats");
    if (strcmp(text, "3.14|  -2.500|1.234568e+04|0.0001|100000"))
        printf("printf gave %s\n", text);
    check(near(strtod("-12.5e2", 0), -1250) && near(atof("3.25"), 3.25), "strtod");

    printf("pi is about %.10f\n", 4 * atan(1));
    printf("%d of %d checks passed\n", checks - failures, checks);
    return failures;
}
