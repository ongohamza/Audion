#!/usr/bin/env python3
"""Exercise the real native Wayland pointer refresh with a stubbed host.
Pass winewayland.drv/wayland_pointer.c. Focus, scaling, and immediate delivery
are verified; existing native input tests verify the server-side ABI.
"""
import pathlib, subprocess, sys, tempfile
source = pathlib.Path(sys.argv[1]).read_text()
start = source.index('static void pointer_send_absolute_motion(')
end = source.index('\nstatic void pointer_handle_motion(void', start)
functions = source[start:end]
pre = r'''
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
typedef int BOOL;
typedef void *HWND;
typedef int32_t wl_fixed_t;
typedef struct { int x,y; } POINT;
typedef struct { int left,top,right,bottom; } RECT;
typedef struct { int type; struct { int dx,dy; unsigned dwFlags; } mi; } INPUT;
#define TRUE 1
#define FALSE 0
#define INPUT_MOUSE 0
#define MOUSEEVENTF_MOVE 1
#define MOUSEEVENTF_ABSOLUTE 0x8000
#define MOUSEEVENTF_MOVE_NOCOALESCE 0x2000
#define TRACE(...) ((void)0)
struct wayland_pointer {
 pthread_mutex_t mutex;
 HWND focused_hwnd,refresh_hwnd;
 wl_fixed_t surface_x,surface_y;
 BOOL relative_mode;
};
struct wayland_surface { struct { RECT rect; } window; double scale; };
struct wayland_win_data { struct wayland_surface *wayland_surface; };
static struct { struct wayland_pointer pointer; } process_wayland;
static struct wayland_surface surface;
static struct wayland_win_data data;
static const unsigned wayland_pointer_flags=6;
static INPUT sent;
static int sends, failures;
static struct wayland_win_data *wayland_win_data_get(HWND hwnd) { return hwnd==(HWND)1 ? &data : 0; }
static void wayland_win_data_release(struct wayland_win_data *d) { (void)d; }
static double wl_fixed_to_double(wl_fixed_t f) { return (double)f/256; }
static POINT map_point_from_surface(struct wayland_surface *s, POINT p) { return (POINT){round(p.x*s->scale),round(p.y*s->scale)}; }
static void NtUserSendHardwareInput(HWND hwnd, unsigned flags, INPUT *input, int param) {
 (void)hwnd;(void)param;
 if(flags!=wayland_pointer_flags) failures++;
 sent=*input;sends++;
}
static void expect(const char *name, int ok) { printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok; }
'''
post = r'''
int main(void) {
 struct wayland_pointer *p=&process_wayland.pointer;
 pthread_mutex_init(&p->mutex,0);
 p->focused_hwnd=(HWND)1; data.wayland_surface=&surface;
 surface.window.rect=(RECT){-1200,1064,-400,1664};surface.scale=1.35;
 pointer_handle_motion_internal(64*256,24*256);
 expect("native motion retains coalescing",sends==1 && !(sent.mi.dwFlags&MOUSEEVENTF_MOVE_NOCOALESCE));
 expect("fractional scale applied",sent.mi.dx==-1114 && sent.mi.dy==1096);
 surface.window.rect=(RECT){0,480,800,1080};
 wayland_pointer_refresh_position((HWND)1);
 expect("stationary position follows corrected origin",sends==2 && sent.mi.dx==86 && sent.mi.dy==512);
 expect("UI-thread refresh is delivered immediately",!!(sent.mi.dwFlags&MOUSEEVENTF_MOVE_NOCOALESCE));
 /* Model a queued old-origin frame overwriting the immediate UI update. */
 sent.mi.dx=-1114;sent.mi.dy=1096;
 pointer_refresh_pending();
 expect("old buffered frame cannot retain the stale origin",sends==3 && sent.mi.dx==86 && sent.mi.dy==512);
 pointer_refresh_pending();
 expect("event refresh consumed once",sends==3);
 sends=2;
 p->focused_hwnd=(HWND)2;wayland_pointer_refresh_position((HWND)1);
 expect("unfocused window cannot change pointer",sends==2);
 p->focused_hwnd=(HWND)1;p->relative_mode=TRUE;wayland_pointer_refresh_position((HWND)1);
 expect("relative knob movement preserved",sends==2);
 p->relative_mode=FALSE;data.wayland_surface=0;wayland_pointer_refresh_position((HWND)1);
 expect("destroyed surface ignored",sends==2);
 data.wayland_surface=&surface;pointer_handle_motion_internal(900*256,700*256);
 expect("surface-edge rounding remains bounded",sent.mi.dx==799 && sent.mi.dy==1079);
 wayland_pointer_refresh_position((HWND)3);
 expect("destroyed window ignored",sends==3);
 pthread_mutex_destroy(&p->mutex);
 return !!failures;
}
'''
with tempfile.TemporaryDirectory(prefix='audion-pointer-refresh-') as tmp:
    c = pathlib.Path(tmp) / 'test.c'; binary = pathlib.Path(tmp) / 'test'
    c.write_text(pre + functions + post)
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g', str(c), '-o', str(binary), '-lm', '-pthread'], check=True)
    subprocess.run([str(binary)], check=True)
