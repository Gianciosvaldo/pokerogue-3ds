/* Precompiles bench.mjs to QuickJS bytecode (bench.qbc). Must be built for the same pointer size as the target. */
#include <stdio.h>
#include <stdlib.h>
#include "quickjs.h"
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1); fread(buf, 1, n, f); buf[n] = 0; fclose(f);
    JSRuntime *rt = JS_NewRuntime(); JS_SetMaxStackSize(rt, 0);
    JS_SetStripInfo(rt, argc > 3 ? JS_STRIP_DEBUG : JS_STRIP_SOURCE);
    JSContext *ctx = JS_NewContext(rt);
    JSValue fn = JS_Eval(ctx, buf, n, argv[1], JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(fn)) { JSValue e = JS_GetException(ctx); printf("%s\n", JS_ToCString(ctx, e)); return 1; }
    size_t len; int flags = JS_WRITE_OBJ_BYTECODE;
    uint8_t *out = JS_WriteObject(ctx, &len, fn, flags);
    FILE *o = fopen(argv[2], "wb"); fwrite(out, 1, len, o); fclose(o);
    printf("wrote %s: %zu bytes\n", argv[2], len);
    return 0;
}
