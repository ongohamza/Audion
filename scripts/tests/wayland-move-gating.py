#!/usr/bin/env python3
"""Exercise the real move helper with fake Wayland requests, not a compositor test."""
import pathlib, subprocess, sys, tempfile
source=pathlib.Path(sys.argv[1]).read_text()
start=source.index('static BOOL wayland_flstudio_move(')
end=source.index('\n/***********************************************************************',start)
helper=source[start:end]
pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
typedef uint16_t WCHAR; typedef int BOOL; typedef unsigned UINT; typedef void *HWND;
typedef struct { unsigned short Length,MaximumLength; WCHAR *Buffer; } UNICODE_STRING;
#define FALSE 0
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define SWP_NOMOVE 2
#define SWP_HIDEWINDOW 128
#define SWP_SHOWWINDOW 64
struct rect { int left,top,right,bottom; };
struct window_rects { struct rect window; };
struct wayland_surface { void *xdg_toplevel; };
struct wayland_win_data { HWND hwnd; struct wayland_surface *wayland_surface; struct window_rects rects; };
struct wayland_pointer { pthread_mutex_t mutex; HWND focused_hwnd,custom_move_hwnd; uint32_t left_button_serial,custom_move_serial; };
static struct { struct wayland_pointer pointer; struct { pthread_mutex_t mutex; void *wl_seat; } seat; void *wl_display; } process_wayland;
static BOOL flstudio_compat;
static int moves,flushes,other_class;
static BOOL wayland_surface_is_toplevel(struct wayland_surface *s) { return !!s->xdg_toplevel; }
static int NtUserGetClassName(HWND h, BOOL real, UNICODE_STRING *name)
{
 /* Wine's actual function returns length; it does NOT assign name->Length. */
 const char *s=other_class?"OtherForm":"TPluginForm"; int i;
 for(i=0;s[i];i++) name->Buffer[i]=s[i]; name->Buffer[i]=0; return i;
}
static void xdg_toplevel_move(void *surface,void *seat,uint32_t serial) { assert(surface&&seat&&serial); moves++; }
static void wl_display_flush(void *display) { flushes++; }
'''
post=r'''
int main(void)
{
 struct wayland_surface surface={(void *)1};
 struct wayland_win_data data={(void *)2,&surface,{{100,100,500,400}}};
 struct window_rects next={{110,110,510,410}}, resize={{110,110,520,410}};
 pthread_mutex_init(&process_wayland.pointer.mutex,0); pthread_mutex_init(&process_wayland.seat.mutex,0);
 process_wayland.pointer.focused_hwnd=data.hwnd; process_wayland.seat.wl_seat=(void *)3;
 process_wayland.pointer.left_button_serial=10;
 wayland_flstudio_move(&data,0x14,&next); assert(moves==0); /* opt-in off */
 flstudio_compat=1; other_class=1;
 wayland_flstudio_move(&data,0x14,&next); assert(moves==0); /* unrelated app window */
 other_class=0;
 wayland_flstudio_move(&data,0x14,&resize); assert(moves==0); /* size changed */
 wayland_flstudio_move(&data,SWP_SHOWWINDOW,&next); assert(moves==0);
 process_wayland.pointer.left_button_serial=0;
 wayland_flstudio_move(&data,0x14,&next); assert(moves==0); /* no live left press */
 process_wayland.pointer.left_button_serial=10;
 wayland_flstudio_move(&data,0x14,&next); assert(moves==1&&flushes==1);
 wayland_flstudio_move(&data,0x14,&next); assert(moves==1); /* once per press */
 process_wayland.pointer.left_button_serial=11;
 wayland_flstudio_move(&data,0x14,&next); assert(moves==2); /* second drag */
 puts("PASS: opt-in, class, sizing, visibility, button state, and one request per gesture");
}
'''
with tempfile.TemporaryDirectory() as d:
 c=pathlib.Path(d)/'test.c'; c.write_text(pre+helper+post)
 exe=pathlib.Path(d)/'test'
 subprocess.run(['cc','-std=c11','-pthread','-o',str(exe),str(c)],check=True)
 subprocess.run([str(exe)],check=True)
