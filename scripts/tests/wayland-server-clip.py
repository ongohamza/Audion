#!/usr/bin/env python3
"""Exercise the server clipping policy across input origins and clip kinds."""
import pathlib, subprocess, sys, tempfile
source = pathlib.Path(sys.argv[1]).read_text()
start = source.index('static int should_clip_cursor_pos(')
function = source[start:source.index('\n}', start) + 2]
code = r"""
#include <stdio.h>
#define SEND_HWMSG_NO_VSCREEN_CLIP 4
#define SET_CURSOR_CLIP 8
#define SET_CURSOR_FSCLIP 32
struct desktop { unsigned cursor_flags, clip_flags; int narrower; };
static int is_cursor_clipped(struct desktop *d) { return d->narrower; }
""" + function + r"""
int main(void) {
    int failures = 0, count = 0;
    /* Ordinary / Wayland input, no clip / explicit / automatic, desktop / monitor. */
    for (int wayland = 0; wayland < 2; wayland++)
    for (int kind = 0; kind < 3; kind++)
    for (int narrower = 0; narrower < 2; narrower++) {
        struct desktop d = {wayland ? SEND_HWMSG_NO_VSCREEN_CLIP : 0,
            kind ? SET_CURSOR_CLIP | (kind == 2 ? SET_CURSOR_FSCLIP : 0) : 0, narrower};
        int expected = !wayland || kind == 1 || (kind == 2 && narrower);
        int got = should_clip_cursor_pos(&d);
        count++;
        if (got != expected) {
            printf("FAIL wayland=%d kind=%d narrower=%d got=%d expected=%d\n",
                wayland, kind, narrower, got, expected);
            failures++;
        }
    }
    printf("%d cases, %d failures\n", count, failures);
    return !!failures;
}
"""
with tempfile.TemporaryDirectory(prefix='audion-server-clip-') as tmp:
    c = pathlib.Path(tmp) / 'test.c'
    exe = pathlib.Path(tmp) / 'test'
    c.write_text(code)
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
