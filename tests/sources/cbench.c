#include <serpentis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIMIT 2000000

static char composite[LIMIT + 1];

static int sieve(void)
{
    memset(composite, 0, sizeof composite);
    int count = 0;
    for (int i = 2; i <= LIMIT; i++) {
        if (composite[i])
            continue;
        count++;
        for (long j = (long)i * i; j <= LIMIT; j += i)
            composite[j] = 1;
    }
    return count;
}

static int fibonacci(int n)
{
    return n < 2 ? n : fibonacci(n - 1) + fibonacci(n - 2);
}

static unsigned int mix(unsigned int rounds)
{
    unsigned int hash = 2166136261u;
    for (unsigned int i = 0; i < rounds; i++)
        hash = (hash ^ (i & 255)) * 16777619u;
    return hash;
}

int main(void)
{
    unsigned long start = serp_ticks();
    int primes = sieve();
    unsigned long after_sieve = serp_ticks();
    int fib = fibonacci(27);
    unsigned long after_fib = serp_ticks();
    unsigned int hash = mix(20000000);
    unsigned long end = serp_ticks();
    printf("primes %d, fib %d, hash %u\n", primes, fib, hash);
    printf("sieve %lu, fib %lu, mix %lu ticks\n", after_sieve - start, after_fib - after_sieve, end - after_fib);
    return 0;
}
