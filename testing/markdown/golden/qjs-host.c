/* QuickJS leg of ascii-golden.sh: runs the bundle and the shared runner on one sample.
 * Linked against the repo's quickjs library (libqjs.a), which has no CLI and no libc
 * modules, so the two host functions the runner needs are defined here.
 * usage: qjs-host <mermaid-ascii.js> <run-bundle.js> <sample.mmd> */
#include <stdio.h>
#include <stdlib.h>
#include "quickjs.h"

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf = NULL;

    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) >= 0 && fseek(f, 0, SEEK_SET) == 0) {
        buf = malloc((size_t)n + 1);
        if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
            free(buf);
            buf = NULL;
        }
        if (buf) {
            buf[n] = 0;
            *len = (size_t)n;
        }
    }
    fclose(f);
    return buf;
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *s;

    (void)this_val;
    s = JS_ToCString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (!s)
        return JS_EXCEPTION;
    fputs(s, stdout);
    fputc('\n', stdout);
    JS_FreeCString(ctx, s);
    return JS_UNDEFINED;
}

static JSValue js_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *path;
    char *buf;
    size_t len = 0;
    JSValue v;

    (void)this_val;
    path = JS_ToCString(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    if (!path)
        return JS_EXCEPTION;
    buf = read_file(path, &len);
    if (buf) {
        v = JS_NewStringLen(ctx, buf, len);
        free(buf);
    } else {
        v = JS_ThrowInternalError(ctx, "cannot read %s", path);
    }
    JS_FreeCString(ctx, path);
    return v;
}

static int eval_file(JSContext *ctx, const char *path)
{
    size_t len = 0;
    char *buf = read_file(path, &len);
    JSValue v;
    int rc = 0;

    if (!buf) {
        fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    v = JS_Eval(ctx, buf, len, path, JS_EVAL_TYPE_GLOBAL);
    free(buf);
    if (JS_IsException(v)) {
        JSValue e = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, e);

        fprintf(stderr, "js: %s\n", msg ? msg : "?");
        if (msg)
            JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, e);
        rc = 1;
    }
    JS_FreeValue(ctx, v);
    return rc;
}

int main(int argc, char **argv)
{
    JSRuntime *rt;
    JSContext *ctx;
    JSValue global;
    int rc;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <mermaid-ascii.js> <run-bundle.js> <sample.mmd>\n", argv[0]);
        return 2;
    }
    rt = JS_NewRuntime();
    ctx = rt ? JS_NewContext(rt) : NULL;
    if (!ctx) {
        fprintf(stderr, "quickjs: cannot create a context\n");
        return 1;
    }
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__print", JS_NewCFunction(ctx, js_print, "__print", 1));
    JS_SetPropertyStr(ctx, global, "__read", JS_NewCFunction(ctx, js_read, "__read", 1));
    JS_SetPropertyStr(ctx, global, "__sample", JS_NewString(ctx, argv[3]));
    JS_FreeValue(ctx, global);

    rc = eval_file(ctx, argv[1]) || eval_file(ctx, argv[2]);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return rc;
}
