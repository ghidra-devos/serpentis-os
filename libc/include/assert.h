#ifndef ASSERT_H
#define ASSERT_H

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
__attribute__((noreturn)) void serp_assert_failed(const char *condition, const char *file, int line);
#define assert(condition) ((condition) ? (void)0 : serp_assert_failed(#condition, __FILE__, __LINE__))
#endif

#endif
