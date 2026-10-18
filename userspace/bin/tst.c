/* userspace/bin/libctest.c
 *
 * Комплексный тест: syscalls, libc, FPU/SSE.
 * Требует: сборки picolibc + реализации _write/_exit/stdio в syscalls.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#include "syscall.h" /* ваши sys_* обёртки */
#include "input.h"   /* KEY_* */

/* ---------- счётчики ---------- */

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *name, int ok)
{
    printf("  [%s] %s\n", ok ? "OK" : "FAIL", name);
    if (ok)
        g_pass++;
    else
        g_fail++;
}

/* ============================================================
 * 1. Syscalls: базовые
 * ============================================================ */

static void test_syscalls_basic(void)
{
    printf("\n=== syscalls: basic ===\n");

    int pid = sys_getpid();
    printf("pid %d\n", pid);

    /* sleep: замер через повторный sleep + логирование */
    printf("  sys_sleep(50) ...\n");
    int r = sys_sleep(50);
    check("sys_sleep returns 0", r == 0);

    /* poll: должен вернуть 0 или 1 без блокировки */
    int p = sys_poll(0);
    check("sys_poll returns 0 or 1", p == 0 || p == 1);

    /* write напрямую */
    const char msg[] = "  [OK] sys_write direct\n";
    int w = sys_write(1, msg, sizeof(msg) - 1);
    check("sys_write returns length", w == (int)(sizeof(msg) - 1));
}

/* ============================================================
 * 2. libc: строки и память
 * ============================================================ */

static void test_libc_strings(void)
{
    printf("\n=== libc: strings ===\n");

    char buf[64];
    strcpy(buf, "hello");
    strcat(buf, ", world");
    check("strcpy/strcat", strcmp(buf, "hello, world") == 0);
    check("strlen", strlen(buf) == 12);

    char *dup = strdup("dup");
    check("strdup", dup && strcmp(dup, "dup") == 0);
    free(dup);

    char copy[16];
    memset(copy, 'A', 15);
    copy[15] = '\0';
    check("memset", strlen(copy) == 15 && copy[0] == 'A' && copy[14] == 'A');

    const char *found = strstr("abcdef", "cd");
    check("strstr", found && strcmp(found, "cdef") == 0);

    check("atoi", atoi("-123") == -123);
    check("strtol hex", strtol("0xFF", NULL, 16) == 255);
}

/* ============================================================
 * 3. libc: malloc
 * ============================================================ */

static void test_libc_malloc(void)
{
    printf("\n=== libc: malloc ===\n");

    /* маленький блок */
    char *a = malloc(32);
    check("malloc(32) != NULL", a != NULL);
    if (a)
    {
        memset(a, 0xAA, 32);
        int ok = 1;
        for (int i = 0; i < 32; i++)
            if ((unsigned char)a[i] != 0xAA)
            {
                ok = 0;
                break;
            }
        check("malloc(32) write/read", ok);
        free(a);
    }

    /* большой блок — гарантированно уйдёт за page boundary */
    size_t big = 256 * 1024;
    char *b = malloc(big);
    check("malloc(256K) != NULL", b != NULL);
    if (b)
    {
        memset(b, 0x55, big);
        check("malloc(256K) last byte", b[big - 1] == 0x55);
        free(b);
    }

    /* realloc */
    char *c = malloc(16);
    if (c)
    {
        for (int i = 0; i < 16; i++)
            c[i] = i;
        c = realloc(c, 128);
        check("realloc preserves data", c && c[0] == 0 && c[15] == 15);
        if (c)
            free(c);
    }

    /* calloc */
    int *z = calloc(100, sizeof(int));
    int zero_ok = 1;
    if (z)
    {
        for (int i = 0; i < 100; i++)
            if (z[i] != 0)
            {
                zero_ok = 0;
                break;
            }
    }
    check("calloc zeroes memory", z != NULL && zero_ok);
    free(z);

    /* много мелких блоков — проверка, что куча не падает */
    void *ptrs[100];
    int alloced = 0;
    for (int i = 0; i < 100; i++)
    {
        ptrs[i] = malloc(64);
        if (ptrs[i])
            alloced++;
    }
    check("100× malloc(64)", alloced == 100);
    for (int i = 0; i < 100; i++)
        free(ptrs[i]);
}

/* ============================================================
 * 4. libc: printf форматы
 * ============================================================ */

static void test_libc_printf(void)
{
    printf("\n=== libc: printf ===\n");

    char buf[128];

    int n = snprintf(buf, sizeof(buf), "%d %x %s", -42, 0xDEAD, "hi");
    check("snprintf %d %x %s", n == 11 && strcmp(buf, "-42 dead hi") == 0);

    n = snprintf(buf, sizeof(buf), "%5d|%-5d|", 7, 7);
    check("snprintf width/justify", strcmp(buf, "    7|7    |") == 0);

    n = snprintf(buf, sizeof(buf), "%08x", 0xAB);
    check("snprintf zero-pad", strcmp(buf, "000000ab") == 0);

    n = snprintf(buf, sizeof(buf), "%p", (void *)0x12345678);
    check("snprintf %p", n > 0 && buf[0] == '0' && buf[1] == 'x');

    /* char */
    n = snprintf(buf, sizeof(buf), "%c%c", 'A', 'B');
    check("snprintf %c", n == 2 && buf[0] == 'A' && buf[1] == 'B');
}

/* ============================================================
 * 5. FPU/SSE: скалярная арифметика float
 * ============================================================ */

static void test_fpu_float_basic(void)
{
    printf("\n=== FPU: float basic ===\n");

    /* базовые операции — компилируются в SSE (addss, mulss, ...) */
    volatile float a = 1.5f;
    volatile float b = 2.25f;

    float sum = a + b;
    float diff = b - a;
    float prod = a * b;
    float quot = b / a;

    check("float add 1.5+2.25=3.75", sum == 3.75f);
    check("float sub 2.25-1.5=0.75", diff == 0.75f);
    check("float mul 1.5*2.25=3.375", prod == 3.375f);
    check("float div 2.25/1.5=1.5", quot == 1.5f);

    /* сравнение */
    volatile float c = 0.1f + 0.2f;
    volatile float d = 0.3f;
    /* 0.1+0.2 != 0.3 в float — это нормально, но в пределах epsilon */
    float eps = 1e-6f;
    float delta = c - d;
    if (delta < 0)
        delta = -delta;
    check("float epsilon 0.1+0.2≈0.3", delta < eps);

    /* отрицательные и ноль */
    volatile float z = 0.0f;
    volatile float mz = -1.0f;
    check("float 1/-1 = -1", (1.0f / mz) == -1.0f);
    check("float -0.0 == 0.0", z == -z);
}

/* ============================================================
 * 6. FPU/SSE: double
 * ============================================================ */

static void test_fpu_double_basic(void)
{
    printf("\n=== FPU: double basic ===\n");

    volatile double a = 3.14159265358979;
    volatile double b = 2.0;

    double r = a / b;
    volatile double expected = 1.570796326794895;
    double diff = r - expected;
    if (diff < 0)
        diff = -diff;
    check("double pi/2", diff < 1e-12);

    /* большие числа */
    volatile double big = 1e15;
    volatile double big2 = big * 2.0;
    check("double 1e15 * 2", big2 == 2e15);

    /* очень маленькие (denormal territory) */
    volatile double small = 1e-300;
    volatile double tiny = small * 1e-10;
    check("double denormal (result != 0 or 0)", tiny == 0.0 || tiny > 0.0);
}

/* ============================================================
 * 7. FPU: math.h (sqrt, fabs, floor/ceil, fmod)
 * ============================================================ */

static void test_fpu_math_basic(void)
{
    printf("\n=== FPU: math.h ===\n");

    check("sqrt(4.0) == 2.0", sqrt(4.0) == 2.0);
    check("sqrtf(9.0f) == 3.0f", sqrtf(9.0f) == 3.0f);
    check("fabs(-3.5) == 3.5", fabs(-3.5) == 3.5);

    check("floor(2.7) == 2", floor(2.7) == 2.0);
    check("ceil(2.1) == 3", ceil(2.1) == 3.0);
    check("floor(-2.1) == -3", floor(-2.1) == -3.0);
    check("ceil(-2.7) == -2", ceil(-2.7) == -2.0);

    check("fmod(7.5, 2.0) == 1.5", fmod(7.5, 2.0) == 1.5);

    /* тригонометрия — обычно использует x87/libm полиномы */
    double s = sin(0.0);
    double cs = cos(0.0);
    check("sin(0) == 0", s == 0.0);
    check("cos(0) == 1", cs == 1.0);

    double half_pi = 3.14159265358979323846 / 2.0;
    double sh = sin(half_pi);
    double delta = sh - 1.0;
    if (delta < 0)
        delta = -delta;
    check("sin(pi/2) ≈ 1", delta < 1e-9);

    /* hypot, atan2 — тоже полезно */
    double h = hypot(3.0, 4.0);
    check("hypot(3,4) == 5", h == 5.0);

    double at = atan2(1.0, 1.0);
    double delta2 = at - half_pi / 2.0;
    if (delta2 < 0)
        delta2 = -delta2;
    check("atan2(1,1) ≈ pi/4", delta2 < 1e-9);
}

/* ============================================================
 * 8. FPU: спецзначения (inf, nan, -0)
 * ============================================================ */

static void test_fpu_special(void)
{
    printf("\n=== FPU: special values ===\n");

    volatile double zero = 0.0;
    volatile double one = 1.0;

    double inf = one / zero;
    check("1.0/0.0 == +inf", isinf(inf) && inf > 0);

    double neg_inf = -one / zero;
    check("-1.0/0.0 == -inf", isinf(neg_inf) && neg_inf < 0);

    double nan = zero / zero;
    check("0.0/0.0 is nan", isnan(nan));
    check("nan != nan", nan != nan);

    /* -0.0 */
    volatile double neg_zero = -0.0;
    check("-0.0 == 0.0", neg_zero == 0.0);
    /* но 1/-0.0 = -inf */
    double r = one / neg_zero;
    check("1/-0.0 == -inf", isinf(r) && r < 0);

    /* знак через signbit */
    check("signbit(-0.0) == 1", signbit(-0.0) != 0);
    check("signbit(0.0)  == 0", signbit(0.0) == 0);
}

/* ============================================================
 * 9. FPU: MXCSR — режим округления
 * ============================================================
 * Проверяет, что fesetround работает. Это доказывает, что
 * MXCSR читается/пишется корректно и что ldmxcsr не был сломан.
 * Если в ядре MXCSR не сохраняется при переключении задач,
 * то этот тест всё равно пройдёт (один поток), но если хотя бы
 * один другой процесс параллельно меняет MXCSR — сломается.
 */

#include <fenv.h>

static void test_fpu_rounding(void)
{
    printf("\n=== FPU: rounding modes ===\n");

    volatile double x = 2.5;

    fesetround(FE_TONEAREST);
    double r1 = nearbyint(x);
    check("round-nearest(2.5) == 2", r1 == 2.0);

    fesetround(FE_UPWARD);
    double r2 = nearbyint(x);
    check("round-up(2.5) == 3", r2 == 3.0);

    fesetround(FE_DOWNWARD);
    double r3 = nearbyint(x);
    check("round-down(2.5) == 2", r3 == 2.0);

    fesetround(FE_TOWARDZERO);
    double r4 = nearbyint(-2.5);
    check("round-toward-zero(-2.5) == -2", r4 == -2.0);

    /* вернуть по умолчанию */
    fesetround(FE_TONEAREST);
}

/* ============================================================
 * 10. FPU: конверсии int <-> float
 * ============================================================ */

static void test_fpu_conversions(void)
{
    printf("\n=== FPU: conversions ===\n");

    int i = 42;
    float f = (float)i;
    double d = (double)i;
    check("int->float 42", f == 42.0f);
    check("int->double 42", d == 42.0);

    float f2 = 3.99f;
    int i2 = (int)f2;
    check("float->int (3.99) == 3", i2 == 3);

    double d2 = -7.99;
    int i3 = (int)d2;
    check("double->int (-7.99) == -7", i3 == -7);

    /* очень большое число — насыщение/UB, но не должно падать */
    volatile double huge = 1e30;
    int sat = (int)huge;
    /* значение неопределено, но программа не должна упасть */
    check("huge double->int did not crash", 1);
    (void)sat;

    /* uint64 <-> double — редкий путь, но полезно */
    volatile double d_big = 1e12;
    unsigned long long u = (unsigned long long)d_big;
    check("double->u64 1e12", u == 1000000000000ULL);
}

/* ============================================================
 * 11. libc: time
 * ============================================================ */

// static void test_libc_time(void)
// {
//     printf("\n=== libc: time ===\n");

//     /* clock() — процессорное время. Если picolibc не реализует,
//        вернёт (clock_t)-1 */
//     clock_t c = clock();
//     check("clock() != -1", c != (clock_t)-1);

//     /* sleep 10 мс и проверить, что clock() вырос */
//     clock_t c1 = clock();
//     sys_sleep(100);
//     clock_t c2 = clock();
//     /* c1 или c2 может быть -1, если picolibc не умеет;
//        тогда просто пропускаем проверку */
//     if (c1 != (clock_t)-1 && c2 != (clock_t)-1)
//         check("clock() grows after sleep", c2 > c1);
//     else
//         printf("  [SKIP] clock() not implemented\n");
// }

/* ============================================================
 * main
 * ============================================================ */

int main(void)
{
    printf("=== libctest: syscalls, libc, FPU ===\n");
    printf("If any [FAIL] appears, there's a real problem.\n");

    test_syscalls_basic();
    test_libc_strings();
    test_libc_malloc();
    test_libc_printf();
    test_fpu_float_basic();
    test_fpu_double_basic();
    sys_sleep(3000); // чтобы запустить еще один такой же процесс для проверки перeключения fpu
    test_fpu_math_basic();
    test_fpu_special();
    test_fpu_rounding();
    test_fpu_conversions();
    // test_libc_time();

    printf("\n=== RESULT: %d passed, %d failed ===\n", g_pass, g_fail);

    /* финальный визуальный маркер */
    if (g_fail == 0)
        printf("*** ALL TESTS PASSED ***\n");
    else
        printf("*** %d FAILURES ***\n", g_fail);

    return g_fail == 0 ? 0 : 1;
}