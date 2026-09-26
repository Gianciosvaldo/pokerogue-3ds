/* QuickJS host for the headless PokeRogue benchmark. Builds for Linux (PC) and 3DS (devkitARM + libctru). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "quickjs.h"

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#define RESULT_PATH "sdmc:/3ds/pokerogue-bench-result.txt"
u32 __ctru_linear_heap_size = 6 * 1024 * 1024; /* leave nearly all app memory to the regular heap */
u32 __stacksize__ = 1024 * 1024;
static PrintConsole top_con, bot_con;
#else
#include <time.h>
#define RESULT_PATH "bench-result.txt"
#endif

static const char *PLATFORM =
#ifdef __3DS__
    "3DS ARM11"
#elif defined(__i386__)
    "PC x86 32-bit"
#else
    "PC x86-64"
#endif
    ;

/* ---- allocation tracking (current + peak bytes handed to QuickJS) ---- */
static size_t mem_cur, mem_peak;
typedef struct { size_t size; size_t pad; } hdr_t; /* 8/16-byte header keeps alignment */
static void *t_malloc(JSMallocState *s, size_t n) {
    (void)s;
    hdr_t *h = malloc(sizeof(hdr_t) + n);
    if (!h) return NULL;
    h->size = n; mem_cur += n; if (mem_cur > mem_peak) mem_peak = mem_cur;
    return h + 1;
}
static void t_free(JSMallocState *s, void *p) {
    (void)s;
    if (!p) return;
    hdr_t *h = (hdr_t *)p - 1; mem_cur -= h->size; free(h);
}
static void *t_realloc(JSMallocState *s, void *p, size_t n) {
    if (!p) return t_malloc(s, n);
    if (n == 0) { t_free(s, p); return NULL; }
    hdr_t *h = (hdr_t *)p - 1; size_t old = h->size;
    hdr_t *nh = realloc(h, sizeof(hdr_t) + n);
    if (!nh) return NULL;
    mem_cur = mem_cur - old + n; if (mem_cur > mem_peak) mem_peak = mem_cur;
    nh->size = n; return nh + 1;
}
static size_t t_usable(const void *p) { return p ? ((const hdr_t *)p - 1)->size : 0; }
static const JSMallocFunctions mf = { t_malloc, t_free, t_realloc, t_usable };

static double now_ms(void) {
#ifdef __3DS__
    return (double)svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000.0);
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

static int done = 0;
static char header[512];
static char *result_text = NULL;

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
#ifdef __3DS__
        consoleSelect(&top_con);
#endif
        printf("%s\n", s); fflush(stdout);
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}
static JSValue js_now(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewFloat64(ctx, now_ms());
}
static JSValue js_mem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue o = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, o, "cur", JS_NewFloat64(ctx, (double)mem_cur));
    JS_SetPropertyStr(ctx, o, "peak", JS_NewFloat64(ctx, (double)mem_peak));
    return o;
}
static JSRuntime *g_rt;
static JSValue js_gc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double t = now_ms();
    JS_RunGC(g_rt);
    return JS_NewFloat64(ctx, now_ms() - t);
}
static JSValue js_done(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) { result_text = strdup(s); JS_FreeCString(ctx, s); }
    done = 1;
    return JS_UNDEFINED;
}

static void dump_exception(JSContext *ctx) {
    JSValue e = JS_GetException(ctx);
    const char *s = JS_ToCString(ctx, e);
    printf("Exception: %s\n", s ? s : "?");
    if (s) JS_FreeCString(ctx, s);
    if (JS_IsObject(e)) {
        JSValue st = JS_GetPropertyStr(ctx, e, "stack");
        const char *ss = JS_ToCString(ctx, st);
        if (ss) { printf("%s\n", ss); JS_FreeCString(ctx, ss); }
        JS_FreeValue(ctx, st);
    }
    JS_FreeValue(ctx, e);
}

static void rejection_tracker(JSContext *ctx, JSValueConst promise, JSValueConst reason, JS_BOOL is_handled, void *opaque) {
    if (is_handled) return;
    const char *s = JS_ToCString(ctx, reason);
    printf("Unhandled rejection: %.300s\n", s ? s : "?");
    if (s) JS_FreeCString(ctx, s);
    if (JS_IsObject(reason)) {
        JSValue st = JS_GetPropertyStr(ctx, reason, "stack");
        const char *ss = JS_ToCString(ctx, st);
        if (ss) { printf("%.600s\n", ss); JS_FreeCString(ctx, ss); }
        JS_FreeValue(ctx, st);
    }
}

static uint8_t *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    buf[n] = 0; fclose(f); *len = n; return buf;
}

static void finish(void) {
    char tail[512] = "";
#ifdef __3DS__
    struct mallinfo mi = mallinfo();
    snprintf(tail, sizeof tail, "host: app heap %.1f MB, malloc arena %.1f MB, in use %.1f MB (JS peak %.1f MB)",
             envGetHeapSize() / 1048576.0, mi.arena / 1048576.0, mi.uordblks / 1048576.0, mem_peak / 1048576.0);
    printf("%s\n", tail);
#endif
    FILE *f = fopen(RESULT_PATH, "w");
    if (f) {
        fprintf(f, "%s\n%s\n%s\n", header, result_text ? result_text : "(benchmark did not finish)", tail);
        fclose(f);
        printf("\nSaved to %s\n", RESULT_PATH);
    }
}

static int run(const char *path, int turns) {
    JSRuntime *rt = JS_NewRuntime2(&mf, NULL);
    g_rt = rt;
    JS_SetMaxStackSize(rt, 900 * 1024);
    JS_SetHostPromiseRejectionTracker(rt, rejection_tracker, NULL);
    JSContext *ctx = JS_NewContext(rt);
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue h = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, h, "print", JS_NewCFunction(ctx, js_print, "print", 1));
    JS_SetPropertyStr(ctx, h, "now", JS_NewCFunction(ctx, js_now, "now", 0));
    JS_SetPropertyStr(ctx, h, "mem", JS_NewCFunction(ctx, js_mem, "mem", 0));
    JS_SetPropertyStr(ctx, h, "gc", JS_NewCFunction(ctx, js_gc, "gc", 0));
    JS_SetPropertyStr(ctx, h, "done", JS_NewCFunction(ctx, js_done, "done", 1));
    JS_SetPropertyStr(ctx, h, "platform", JS_NewString(ctx, PLATFORM));
    JS_SetPropertyStr(ctx, g, "__host", h);
    if (turns > 0) JS_SetPropertyStr(ctx, g, "__turns", JS_NewInt32(ctx, turns));

    double t0 = now_ms();
    size_t len;
    uint8_t *buf = read_file(path, &len);
    if (!buf) { printf("cannot read %s\n", path); return 1; }
    double tRead = now_ms();
    JS_SetPropertyStr(ctx, g, "__t0", JS_NewFloat64(ctx, t0));

    JSValue fn;
    size_t plen = strlen(path);
    if (plen > 4 && strcmp(path + plen - 4, ".qbc") == 0) {
        fn = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    } else {
        fn = JS_Eval(ctx, (const char *)buf, len, path, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    }
    free(buf);
    if (JS_IsException(fn)) { dump_exception(ctx); return 1; }
    double tCompile = now_ms();
    snprintf(header, sizeof header, "host: read file %.0f ms, %s %.0f ms, JS heap %.1f MB after load",
             tRead - t0, (plen > 4 && strcmp(path + plen - 4, ".qbc") == 0) ? "load bytecode" : "compile source",
             tCompile - tRead, mem_cur / 1048576.0);
    printf("%s\n", header);
    JSValue r = JS_EvalFunction(ctx, fn);
    if (JS_IsException(r)) { dump_exception(ctx); return 1; }
    JS_FreeValue(ctx, r);

    JSValue runTimers = JS_GetPropertyStr(ctx, g, "__runTimers");
    for (;;) {
        JSContext *c2;
        int err;
        while ((err = JS_ExecutePendingJob(rt, &c2)) != 0) {
            if (err < 0) dump_exception(c2);
        }
        if (done) break;
#ifdef __3DS__
        if (!aptMainLoop()) break;
#endif
        JSValue more = JS_Call(ctx, runTimers, g, 0, NULL);
        if (JS_IsException(more)) { dump_exception(ctx); break; }
        int any = JS_ToBool(ctx, more);
        JS_FreeValue(ctx, more);
        if (!any && !JS_IsJobPending(rt)) { printf("event loop drained before benchmark finished\n"); break; }
    }
    finish();
    return 0;
}

#ifdef __3DS__
int main(int argc, char **argv) {
    osSetSpeedupEnable(true); /* 804 MHz + L2 cache on a New 3DS; no effect on an original 3DS */
    gfxInitDefault();
    consoleInit(GFX_TOP, &top_con);
    consoleInit(GFX_BOTTOM, &bot_con);
    consoleSelect(&bot_con);
    bool isNew = false;
    APT_CheckNew3DS(&isNew);
    printf("PokeRogue QuickJS benchmark (%s)\n\n", isNew ? "New 3DS, 804 MHz" : "original 3DS, 268 MHz");
    printf("app heap: %.1f MB, linear: %.1f MB\n", envGetHeapSize() / 1048576.0, envGetLinearHeapSize() / 1048576.0);
    printf("system mem free: %.1f MB\n", osGetMemRegionFree(MEMREGION_APPLICATION) / 1048576.0);
    printf("Running... this can take minutes.\nDo not close.\n");
    consoleSelect(&top_con);
    Result rc = romfsInit();
    if (rc) printf("romfsInit failed %08lx\n", rc);
    run("romfs:/bench.qbc", 0);
    consoleSelect(&bot_con);
    printf("\nDone. Results saved to\nsd:/3ds/pokerogue-bench-result.txt\n\nPress START to exit.\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    romfsExit();
    gfxExit();
    return 0;
}
#else
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s bench.mjs|bench.qbc [turns]\n", argv[0]); return 2; }
    return run(argv[1], argc > 2 ? atoi(argv[2]) : 0);
}
#endif
