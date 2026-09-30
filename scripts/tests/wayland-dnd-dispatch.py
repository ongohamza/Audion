#!/usr/bin/env python3
"""Exercise production GUI dispatch against a recording user32 boundary.

Catches stale generation/HWND delivery, duplicate legacy delivery, callback
failure dispatch, and clearing GUI ownership when the worker times out.
"""
import pathlib, subprocess, sys, tempfile
s=(pathlib.Path(sys.argv[1])/'dlls/winewayland.drv/wayland_data_device.c').read_text()
state=s[s.index('static pthread_mutex_t drop_mutex'):s.index('static void *read_drop_data')]
handler=s[s.index('LRESULT wayland_drop_files('):s.index('\nstatic BOOL drag_update_point(')]
code=r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
typedef void *HWND;
typedef uint32_t DWORD;
typedef uintptr_t ULONG_PTR;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef int BOOL;
typedef struct {int x, y;} POINT;
struct format_entry {unsigned format, size; char data[1];};
#define TRUE 1
#define FALSE 0
#define CF_HDROP 15
#define DROPEFFECT_NONE 0
#define DROPEFFECT_COPY 1
#define MAKELONG(x,y) ((uint32_t)(uint16_t)(x) | ((uint32_t)(uint16_t)(y)<<16))
enum {WINE_DRAG_DROP_ENTER, WINE_DRAG_DROP_DRAG, WINE_DRAG_DROP_DROP, WINE_DRAG_DROP_LEAVE};
#define NtUserDragDropCall 777
static LRESULT NtUserMessageCall(HWND, unsigned, uintptr_t, LPARAM, void *, unsigned, BOOL);
'''+state+handler+r'''
static unsigned calls[8], count;
static int fail_enter, reject_drag, fail_drop, expire_during_enter;
static LRESULT NtUserMessageCall(HWND hwnd, unsigned msg, uintptr_t wp, LPARAM lp,
                                void *info, unsigned type, BOOL ansi)
{
    (void)info; (void)ansi;
    assert(type == NtUserDragDropCall);
    assert(drop_gui_busy);
    calls[count++] = msg;
    if (msg == WINE_DRAG_DROP_ENTER)
    {
        struct format_entry *entry = (void *)lp;
        assert(!hwnd);
        assert(wp == offsetof(struct format_entry, data) + 4);
        assert(entry->format == CF_HDROP && entry->size == 4);
        assert(!memcmp(entry->data, "abc", 4));
        if (expire_during_enter)
        {
            free(pending_drop.data);
            memset(&pending_drop, 0, sizeof(pending_drop));
            assert(drop_gui_busy); /* survives worker protocol cleanup */
        }
        return fail_enter;
    }
    if (msg == WINE_DRAG_DROP_DRAG)
    {
        assert(hwnd == (HWND)123);
        assert(wp == MAKELONG(-10, 200));
        assert(lp == DROPEFFECT_COPY);
        return reject_drag ? 0 : DROPEFFECT_COPY;
    }
    if (msg == WINE_DRAG_DROP_DROP) return fail_drop ? 0 : DROPEFFECT_COPY;
    return 0;
}
static void setup(void)
{
    free(pending_drop.data);
    memset(&pending_drop, 0, sizeof(pending_drop));
    pending_drop.generation = ++drop_generation;
    pending_drop.hwnd = (HWND)123;
    pending_drop.data = malloc(4);
    memcpy(pending_drop.data, "abc", 4);
    pending_drop.size = 4;
    pending_drop.point = (POINT){-10,200};
    pending_drop.ready = 1;
    fail_enter = reject_drag = fail_drop = expire_during_enter = count = 0;
    drop_gui_busy = 0;
}
int main(void)
{
    setup();
    wayland_drop_files((HWND)123, pending_drop.generation - 1);
    assert(count == 0 && !pending_drop.handled && pending_drop.ready);
    wayland_drop_files((HWND)124, pending_drop.generation);
    assert(count == 0 && !pending_drop.handled && pending_drop.ready);
    wayland_drop_files((HWND)123, pending_drop.generation);
    assert(count == 4 && calls[0] == WINE_DRAG_DROP_ENTER && calls[1] == WINE_DRAG_DROP_DRAG &&
           calls[2] == WINE_DRAG_DROP_DRAG && calls[3] == WINE_DRAG_DROP_DROP);
    assert(pending_drop.handled && pending_drop.effect == DROPEFFECT_COPY && !drop_gui_busy);
    wayland_drop_files((HWND)123, pending_drop.generation);
    assert(count == 4 && pending_drop.effect == DROPEFFECT_COPY); /* cannot redeliver or overwrite result */
    setup(); fail_enter = 1;
    wayland_drop_files((HWND)123, pending_drop.generation);
    assert(count == 1 && pending_drop.handled && !pending_drop.effect && !drop_gui_busy);
    setup(); reject_drag = 1;
    wayland_drop_files((HWND)123, pending_drop.generation);
    assert(count == 4 && calls[3] == WINE_DRAG_DROP_LEAVE && !pending_drop.effect);
    setup(); fail_drop = 1;
    wayland_drop_files((HWND)123, pending_drop.generation);
    assert(count == 4 && !pending_drop.effect);
    setup(); expire_during_enter = 1;
    wayland_drop_files((HWND)123, pending_drop.generation);
    assert(count == 4 && !drop_gui_busy && !pending_drop.handled);
    free(pending_drop.data);
    puts("PASS: GUI drop dispatch, stale delivery, failure and timeout ownership");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=pathlib.Path(tmp)/'test.c'; exe=pathlib.Path(tmp)/'test'
    src.write_text(code)
    subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
