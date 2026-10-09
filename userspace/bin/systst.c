#include "unistd.h"
#include "input.h"

// =============================================================
//  helpers
// =============================================================

static int slen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}
static int wr(const char *s) { return _sysenter6(SYS_WRITE, 1, (int)s, slen(s), 0, 0, 0); }
static int wrn(const char *s, int n) { return _sysenter6(SYS_WRITE, 1, (int)s, n, 0, 0, 0); }

static void pint(int v)
{
    char b[12];
    int i = 0;
    if (v < 0)
    {
        wrn("-", 1);
        v = -v;
    }
    if (v == 0)
        b[i++] = '0';
    while (v > 0)
    {
        b[i++] = '0' + (v % 10);
        v /= 10;
    }
    char o[12];
    for (int j = 0; j < i; ++j)
        o[j] = b[i - 1 - j];
    wrn(o, i);
}

static void phex(unsigned v)
{
    static const char hx[] = "0123456789abcdef";
    char b[10] = {'0', 'x', 0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 8; ++i)
        b[2 + i] = hx[(v >> ((7 - i) * 4)) & 0xF];
    wrn(b, 10);
}

static int G_PASS = 0, G_FAIL = 0;

static void T(const char *name)
{
    wr("\n--- ");
    wr(name);
    wr(" ---\n");
}

static int check(const char *what, int got, int expect_eq)
{
    wr("  ");
    wr(what);
    wr(" -> returned ");
    pint(got);
    wr(" (expected ");
    pint(expect_eq);
    wr(")");

    if (got == expect_eq)
    {
        wr("   [OK]\n");
        G_PASS++;
        return 1;
    }
    wr("   [FAIL]\n");
    G_FAIL++;
    return 0;
}

static int check_pos(const char *what, int got)
{
    wr("  ");
    wr(what);
    wr(" -> returned ");
    pint(got);
    if (got > 0)
    {
        wr("   [OK]\n");
        G_PASS++;
        return 1;
    }
    wr("   [FAIL: expected > 0]\n");
    G_FAIL++;
    return 0;
}

static void VISUAL(const char *const *lines, int count, int pause_ms)
{
    wr("\n  +-- CHECK WITH YOUR EYES -------------------\n");
    for (int i = 0; i < count; ++i)
    {
        wr("  | ");
        wr(lines[i]);
        wr("\n");
    }
    wr("  +-------------------------------------------\n");
    wr("  Pausing ");
    pint(pause_ms / 1000);
    wr(" sec...\n");
    _sysenter6(SYS_SLEEP, pause_ms, 0, 0, 0, 0, 0);
}

// =============================================================
//  stdio
// =============================================================

static void test_stdio(void)
{
    T("stdio: sys_write");

    {
        const char msg[] = "HELLO_FROM_STDOUT\n";
        wr("  called: write(1, \"HELLO_FROM_STDOUT\\n\", 18)\n");
        wr("  expected: line \"HELLO_FROM_STDOUT\" below\n");
        wr("  actual output: ");
        int r = _sysenter6(SYS_WRITE, 1, (int)msg, sizeof(msg) - 1, 0, 0, 0);
        check("write(1,...)", r, (int)(sizeof(msg) - 1));
    }

    {
        const char msg[] = "HELLO_FROM_STDERR\n";
        wr("  called: write(2, \"HELLO_FROM_STDERR\\n\", 18)\n");
        wr("  expected: line \"HELLO_FROM_STDERR\" below\n");
        wr("  actual output: ");
        int r = _sysenter6(SYS_WRITE, 2, (int)msg, sizeof(msg) - 1, 0, 0, 0);
        check("write(2,...)", r, (int)(sizeof(msg) - 1));
    }

    {
        wr("  called: write(1, \"abc\", 3) -- no NUL terminator\n");
        wr("  expected: exactly 3 chars \"abc\" (no more!)\n");
        wr("  actual output: [");
        int r = _sysenter6(SYS_WRITE, 1, (int)"abc", 3, 0, 0, 0);
        wr("]\n");
        check("write 3 bytes", r, 3);
    }

    check("write(bad_fd=7)",
          _sysenter6(SYS_WRITE, 7, (int)"x", 1, 0, 0, 0), -1);

    check("write(NULL)",
          _sysenter6(SYS_WRITE, 1, 0, 10, 0, 0, 0), -1);

    check("write(kernel_ptr)",
          _sysenter6(SYS_WRITE, 1, 0xC0000000, 10, 0, 0, 0), -1);

    wr("\n  Also verify NO extra text appeared from previous call.\n");
}

// =============================================================
//  process
// =============================================================

static void test_process(void)
{
    T("process: getpid / getppid");

    int pid = _sysenter6(SYS_GETPID, 0, 0, 0, 0, 0, 0);
    check_pos("getpid", pid);

    int ppid = _sysenter6(SYS_GETPPID, 0, 0, 0, 0, 0, 0);
    wr("  getppid -> ");
    pint(ppid);
    wr("  ");
    if (ppid >= 0)
    {
        wr("[OK]\n");
        G_PASS++;
    }
    else
    {
        wr("[FAIL]\n");
        G_FAIL++;
    }
}

// =============================================================
//  time
// =============================================================

struct k_ts
{
    int sec;
    int nsec;
};
struct k_tv
{
    int sec;
    int usec;
};

static void test_time(void)
{
    T("time: clock_gettime / gettimeofday / nanosleep");

    struct k_ts mono = {0, 0}, real = {0, 0}, after = {0, 0};
    struct k_tv tv = {0, 0};

    int r = _sysenter6(SYS_CLOCK_GETTIME, 1, (int)&mono, 0, 0, 0, 0);
    check("clock_gettime(MONOTONIC)", r, 0);
    wr("    sec=");
    pint(mono.sec);
    wr(" nsec=");
    pint(mono.nsec);
    wr("\n");

    r = _sysenter6(SYS_CLOCK_GETTIME, 0, (int)&real, 0, 0, 0, 0);
    check("clock_gettime(REALTIME)", r, 0);
    wr("    sec=");
    pint(real.sec);
    wr(" nsec=");
    pint(real.nsec);
    wr("\n");
    wr("  expected: REALTIME > MONOTONIC (realtime from epoch, mono from boot)\n");

    if (real.sec > mono.sec)
    {
        wr("  realtime > monotonic  [OK]\n");
        G_PASS++;
    }
    else
    {
        wr("  realtime > monotonic  [FAIL]\n");
        G_FAIL++;
    }

    r = _sysenter6(SYS_GETTIMEOFDAY, (int)&tv, 0, 0, 0, 0, 0);
    check("gettimeofday", r, 0);
    wr("    sec=");
    pint(tv.sec);
    wr(" usec=");
    pint(tv.usec);
    wr("\n");

    check("clock_gettime(bad clockid)",
          _sysenter6(SYS_CLOCK_GETTIME, 99, (int)&real, 0, 0, 0, 0), -1);

    wr("\n  nanosleep(300 ms):\n");
    struct k_ts req = {0, 300000000};
    _sysenter6(SYS_CLOCK_GETTIME, 1, (int)&mono, 0, 0, 0, 0);
    r = _sysenter6(SYS_NANOSLEEP, (int)&req, 0, 0, 0, 0, 0);
    _sysenter6(SYS_CLOCK_GETTIME, 1, (int)&after, 0, 0, 0, 0);
    check("nanosleep", r, 0);

    unsigned d1 = (unsigned)mono.sec * 1000u + (unsigned)mono.nsec / 1000000u;
    unsigned d2 = (unsigned)after.sec * 1000u + (unsigned)after.nsec / 1000000u;
    unsigned delta = d2 - d1;
    wr("    actual pause: ");
    pint((int)delta);
    wr(" ms (expected ~300)\n");
    if (delta >= 280 && delta <= 400)
    {
        wr("  [OK]\n");
        G_PASS++;
    }
    else
    {
        wr("  [FAIL: too small/too big]\n");
        G_FAIL++;
    }

    struct k_ts bad = {0, 2000000000};
    check("nanosleep(bad nsec)",
          _sysenter6(SYS_NANOSLEEP, (int)&bad, 0, 0, 0, 0, 0), -1);
    check("nanosleep(NULL)",
          _sysenter6(SYS_NANOSLEEP, 0, 0, 0, 0, 0, 0), -1);
}

// =============================================================
//  sleep + poll
// =============================================================

static void test_sleep_poll(void)
{
    T("sleep / poll");

    struct k_ts t1 = {0, 0}, t2 = {0, 0};
    _sysenter6(SYS_CLOCK_GETTIME, 1, (int)&t1, 0, 0, 0, 0);

    wr("  sys_sleep(500 ms):\n");
    int r = _sysenter6(SYS_SLEEP, 500, 0, 0, 0, 0, 0);
    check("sys_sleep", r, 0);

    _sysenter6(SYS_CLOCK_GETTIME, 1, (int)&t2, 0, 0, 0, 0);
    unsigned d1 = (unsigned)t1.sec * 1000u + (unsigned)t1.nsec / 1000000u;
    unsigned d2 = (unsigned)t2.sec * 1000u + (unsigned)t2.nsec / 1000000u;
    unsigned delta = d2 - d1;
    wr("    actual pause: ");
    pint((int)delta);
    wr(" ms (expected ~500)\n");
    if (delta >= 480 && delta <= 700)
    {
        wr("  [OK]\n");
        G_PASS++;
    }
    else
    {
        wr("  [FAIL]\n");
        G_FAIL++;
    }

    int pr = _sysenter6(SYS_POLL, 0, 0, 0, 0, 0, 0);
    wr("  poll(keyboard) -> ");
    pint(pr);
    if (pr == 0 || pr == 1)
    {
        wr("  [OK]\n");
        G_PASS++;
    }
    else
    {
        wr("  [FAIL: must be 0 or 1]\n");
        G_FAIL++;
    }
    wr("  (0 = no keys queued, 1 = key is waiting)\n");
}

// =============================================================
//  memory
// =============================================================

static void test_mem(void)
{
    T("memory: brk / mmap2 / munmap");

    int b0 = _sysenter6(SYS_BRK, 0, 0, 0, 0, 0, 0);
    wr("  brk(0) -> ");
    phex((unsigned)b0);
    wr("  ");
    if (b0 > 0)
    {
        wr("[OK]\n");
        G_PASS++;
    }
    else
    {
        wr("[FAIL]\n");
        G_FAIL++;
    }

    int b1 = _sysenter6(SYS_BRK, b0 + 8192, 0, 0, 0, 0, 0);
    wr("  brk(b0+8192) -> ");
    phex((unsigned)b1);
    wr("  ");
    if (b1 >= b0 + 8192)
    {
        wr("[OK]\n");
        G_PASS++;
    }
    else
    {
        wr("[FAIL]\n");
        G_FAIL++;
    }

    wr("  writing 1024 words at ");
    phex((unsigned)b0);
    wr("...\n");
    unsigned *bp = (unsigned *)b0;
    for (int i = 0; i < 1024; ++i)
        bp[i] = 0xA1B2C300u + i;
    int good = 1;
    for (int i = 0; i < 1024; ++i)
        if (bp[i] != 0xA1B2C300u + i)
        {
            good = 0;
            break;
        }
    if (good)
    {
        wr("  read-back check -- matched  [OK]\n");
        G_PASS++;
    }
    else
    {
        wr("  read-back check -- MISMATCH  [FAIL]\n");
        G_FAIL++;
    }

    int b2 = _sysenter6(SYS_BRK, b0 - 4096, 0, 0, 0, 0, 0);
    wr("  brk(shrink by 4K) -> ");
    phex((unsigned)b2);
    wr("  expected: ");
    phex((unsigned)b1);
    wr(" (old value, no change)  ");
    if (b2 == b1)
    {
        wr("[OK]\n");
        G_PASS++;
    }
    else
    {
        wr("[FAIL]\n");
        G_FAIL++;
    }

    int m = _sysenter6(SYS_MMAP2, 0, 16384, 3, 0x22, (int)-1, 0);
    wr("  mmap2(16K, anon) -> ");
    phex((unsigned)m);
    wr("  ");
    if (m > 0)
    {
        wr("[OK]\n");
        G_PASS++;
    }
    else
    {
        wr("[FAIL]\n");
        G_FAIL++;
        return;
    }

    unsigned *mp = (unsigned *)m;
    for (int i = 0; i < 4096; ++i)
        mp[i] = 0xDEAD0000u + i;
    good = 1;
    for (int i = 0; i < 4096; ++i)
        if (mp[i] != 0xDEAD0000u + i)
        {
            good = 0;
            break;
        }
    if (good)
    {
        wr("  mmap2 r/w -- [OK]\n");
        G_PASS++;
    }
    else
    {
        wr("  mmap2 r/w -- [FAIL]\n");
        G_FAIL++;
    }

    check("mmap2(non-anon)",
          _sysenter6(SYS_MMAP2, 0, 4096, 3, 0x02, (int)-1, 0), -1);
    check("munmap",
          _sysenter6(SYS_MUNMAP, m, 16384, 0, 0, 0, 0), 0);
    check("munmap(unaligned)",
          _sysenter6(SYS_MUNMAP, m + 1, 100, 0, 0, 0, 0), -1);
}

// =============================================================
//  windows
// =============================================================

static void test_windows(void)
{
    T("windows: win_create / commit / move / set_z / destroy");

    int w1 = _sysenter6(SYS_WIN_CREATE, 120, 80, 30, 30, 100, 0);
    wr("  win_create(120x80 at 30,30, z=100) -> ");
    pint(w1);
    wr("\n");
    if (w1 < 0)
    {
        wr("  [FAIL]\n");
        G_FAIL++;
        return;
    }
    G_PASS++;

    int w2 = _sysenter6(SYS_WIN_CREATE, 100, 60, 180, 50, 101, 0);
    wr("  win_create(100x60 at 180,50, z=101) -> ");
    pint(w2);
    wr("\n");
    if (w2 < 0)
    {
        wr("  [FAIL]\n");
        G_FAIL++;
        _sysenter6(SYS_WIN_DESTROY, w1, 0, 0, 0, 0, 0);
        return;
    }
    G_PASS++;

    static unsigned buf1[120 * 80];
    static unsigned buf2[100 * 60];
    for (int i = 0; i < 120 * 80; ++i)
        buf1[i] = 0xFF4060C0u; // blue
    for (int i = 0; i < 100 * 60; ++i)
        buf2[i] = 0xFFC06040u; // orange

    wr("  commit(w1, BLUE buffer 120x80)\n");
    int r = _sysenter6(SYS_WIN_COMMIT, w1, (int)buf1, 0, 0, 0, 0);
    check("win_commit(w1)", r, 0);

    wr("  commit(w2, ORANGE buffer 100x60)\n");
    r = _sysenter6(SYS_WIN_COMMIT, w2, (int)buf2, 0, 0, 0, 0);
    check("win_commit(w2)", r, 0);

    {
        static const char *lines[] = {
            "Two windows must appear on screen:",
            "  - BLUE   120x80 near top-left (~30,30)",
            "  - ORANGE 100x60 to the right (~180,50)",
            "Mouse cursor must be ON TOP of both windows.",
            "Console with this text must be UNDER the windows.",
        };
        VISUAL(lines, 5, 3000);
    }

    wr("\n  win_move(w1, 40, 60)\n");
    r = _sysenter6(SYS_WIN_MOVE, w1, 40, 60, 0, 0, 0);
    check("win_move", r, 0);

    {
        static const char *lines[] = {
            "The BLUE window has moved.",
            "Expected: BLUE now at (40, 60),",
            "ORANGE unchanged at (180, 50).",
        };
        VISUAL(lines, 3, 2500);
    }

    wr("\n  win_set_z(w1, 102) -- was 100, now 102; w2 has z=101\n");
    r = _sysenter6(SYS_WIN_SET_Z, w1, 102, 0, 0, 0, 0);
    check("win_set_z", r, 0);

    {
        static const char *lines[] = {
            "Z-order changed.",
            "Expected: BLUE window is now ON TOP of ORANGE",
            "in the area where they overlap (if they overlap).",
        };
        VISUAL(lines, 3, 3000);
    }

    check("win_commit(bad handle)",
          _sysenter6(SYS_WIN_COMMIT, 999, (int)buf1, 0, 0, 0, 0), -1);
    check("win_commit(NULL buf)",
          _sysenter6(SYS_WIN_COMMIT, w1, 0, 0, 0, 0, 0), -1);
    check("win_create(w=0)",
          _sysenter6(SYS_WIN_CREATE, 0, 80, 0, 0, 100, 0), -1);
    check("win_create(w=5000)",
          _sysenter6(SYS_WIN_CREATE, 5000, 80, 0, 0, 100, 0), -1);

    wr("\n  win_destroy(w1)\n");
    r = _sysenter6(SYS_WIN_DESTROY, w1, 0, 0, 0, 0, 0);
    check("win_destroy(w1)", r, 0);

    {
        static const char *lines[] = {
            "The BLUE window has disappeared.",
            "Expected: only ORANGE window is visible.",
        };
        VISUAL(lines, 2, 2000);
    }

    wr("\n  win_destroy(w2)\n");
    r = _sysenter6(SYS_WIN_DESTROY, w2, 0, 0, 0, 0, 0);
    check("win_destroy(w2)", r, 0);

    {
        static const char *lines[] = {
            "Both windows are gone.",
            "Expected: only console and mouse cursor on screen.",
        };
        VISUAL(lines, 2, 2000);
    }

    check("win_destroy(dead handle)",
          _sysenter6(SYS_WIN_DESTROY, w1, 0, 0, 0, 0, 0), -1);
}

// =============================================================
//  stubs
// =============================================================

static void test_stubs(void)
{
    T("stubs (no VFS yet) -- all must return -1");

    struct
    {
        const char *name;
        int num;
        int a1;
    } t[] = {
        {"open", SYS_OPEN, 0},
        {"close", SYS_CLOSE, 3},
        {"lseek", SYS_LSEEK, 3},
        {"stat", SYS_STAT, 0},
        {"fstat", SYS_FSTAT, 3},
        {"unlink", SYS_UNLINK, 0},
        {"chdir", SYS_CHDIR, 0},
        {"getcwd", SYS_GETCWD, 0},
        {"dup", SYS_DUP, 1},
        {"access", SYS_ACCESS, 0},
        {"ioctl", SYS_IOCTL, 0},
    };

    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); ++i)
        check(t[i].name, _sysenter6(t[i].num, t[i].a1, 0, 0, 0, 0, 0), -1);

    check("unknown syscall #510",
          _sysenter6(510, 0, 0, 0, 0, 0, 0), -1);
}

// =============================================================
//  kill + exit (last!)
// =============================================================

static void test_kill_and_exit(void)
{
    T("kill + exit");

    int me = _sysenter6(SYS_GETPID, 0, 0, 0, 0, 0, 0);
    check("kill(other pid)",
          _sysenter6(SYS_KILL, me + 100, 9, 0, 0, 0, 0), -1);

    wr("\n  +------------------------------------------+\n");
    wr("  | Process will now kill itself via kill(0) |\n");
    wr("  | You should see the terminal prompt       |\n");
    wr("  | \"...>\" with no errors after this.        |\n");
    wr("  +------------------------------------------+\n");
    wr("  (pausing 2 sec before self-termination)\n");
    _sysenter6(SYS_SLEEP, 2000, 0, 0, 0, 0, 0);

    _sysenter6(SYS_KILL, 0, 9, 0, 0, 0, 0);

    wr("BUG: kill(0) returned, process still alive\n");
    _sysenter6(SYS_EXIT, 1, 0, 0, 0, 0, 0);
    for (;;)
        asm volatile("pause");
}

// =============================================================
//  main
// =============================================================

void _start(void)
{
    wr("\n");
    wr("===============================================\n");
    wr("   direct syscall test (no wrappers)\n");
    wr("   all calls through _sysenter6(...)\n");
    wr("===============================================\n");
    wr("\n  Legend:\n");
    wr("    [OK] / [FAIL] -- automatic check\n");
    wr("    +-- CHECK WITH YOUR EYES -- visual check\n");

    test_stdio();
    test_process();
    test_time();
    test_sleep_poll();
    test_mem();
    test_windows();
    test_stubs();

    wr("\n===============================================\n");
    wr("   TOTAL: ");
    pint(G_PASS);
    wr(" OK, ");
    pint(G_FAIL);
    wr(" FAIL");
    wr("\n===============================================\n");

    int ind = _sysenter6(SYS_WIN_CREATE, 200, 80, 100, 100, 100, 0);
    if (ind >= 0)
    {
        static unsigned pix[200 * 80];
        unsigned c = (G_FAIL == 0) ? 0xFF20C020u : 0xFFC02020u;
        for (int i = 0; i < 200 * 80; ++i)
            pix[i] = c;
        _sysenter6(SYS_WIN_COMMIT, ind, (int)pix, 0, 0, 0, 0);

        static const char *lines[] = {
            "Final indicator window must be GREEN if all ok,",
            "or RED if there were failures.",
        };
        VISUAL(lines, 2, 3000);

        _sysenter6(SYS_WIN_DESTROY, ind, 0, 0, 0, 0, 0);
    }

    test_kill_and_exit();
}