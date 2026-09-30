#!/usr/bin/env python3
"""Exercise real configure delivery/minimize updates using a stubbed Wayland host.

Pass winewayland.drv/window.c. The adjacent wayland_surface.c is also read.
Lifecycle cases catch coalescing and acknowledgment-dependent activation loss.
"""
import pathlib, re, subprocess, sys, tempfile
window=pathlib.Path(sys.argv[1]); w=window.read_text(); s=window.with_name('wayland_surface.c').read_text()
def function(source,name):
 start=source.index('static void '+name+'(')
 return source[start:source.index('\n}',start)+2]
configure=function(s,'xdg_surface_handle_configure')
get_config=function(w,'wayland_win_data_get_config')
decision=re.findall(r'restoring_from_minimize = (.*?);',w,re.S)[-1]
pre=r'''
#include <stdio.h>
#include <stdint.h>
#include <string.h>
typedef int BOOL;
typedef unsigned DWORD;
typedef void *HWND;
typedef struct { int left,top,right,bottom; } RECT;
#define TRUE 1
#define FALSE 0
#define TRACE(...) ((void)0)
#define WS_MINIMIZE 1
#define WS_MAXIMIZE 2
#define WS_CAPTION 4
#define WS_VISIBLE 8
#define GWL_STYLE 0
#define WM_WAYLAND_CONFIGURE 1
enum wayland_surface_config_state { WAYLAND_SURFACE_CONFIG_STATE_MAXIMIZED=1, WAYLAND_SURFACE_CONFIG_STATE_FULLSCREEN=2 };
struct xdg_surface { int unused; };
struct wayland_surface_config { RECT rect; unsigned state,serial; BOOL processed,activated; };
struct wayland_window_config { RECT rect; unsigned state; BOOL minimized,resizeable,visible,managed; };
struct wayland_surface {
 struct xdg_surface *xdg_surface;
 struct wayland_surface_config pending,requested,processing,current;
 struct wayland_window_config window;
 BOOL received_activated,restore_pending;
};
struct wayland_win_data {
 HWND hwnd; struct wayland_surface *wayland_surface;
 struct { RECT visible; } rects;
 BOOL is_fullscreen,resizeable,managed;
};
static struct xdg_surface xdg;
static struct wayland_surface obj;
static struct wayland_win_data data;
static DWORD style;
static unsigned serial;
static int failures;
static struct wayland_win_data *wayland_win_data_get(HWND hwnd) { (void)hwnd;return &data; }
static void wayland_win_data_release(struct wayland_win_data *d) { (void)d; }
static BOOL wayland_surface_is_toplevel(struct wayland_surface *s) { (void)s;return TRUE; }
static void NtUserPostMessage(HWND hwnd,int msg,int a,int b) { (void)hwnd;(void)msg;(void)a;(void)b; }
static void NtUserExposeWindowSurface(HWND hwnd,int a,void *b) { (void)hwnd;(void)a;(void)b; }
static DWORD NtUserGetWindowLongW(HWND hwnd,int index) { (void)hwnd;(void)index;return style; }
'''
post=r'''
static void receive(BOOL activated) {
 obj.pending.activated=activated;
 xdg_surface_handle_configure(data.hwnd,&xdg,++serial);
}
static void minimize(BOOL minimized) {
 style=WS_VISIBLE | (minimized ? WS_MINIMIZE : 0);
 wayland_win_data_get_config(&data,&obj.window);
}
static void reset(void) {
 memset(&obj,0,sizeof(obj));memset(&data,0,sizeof(data));
 data.wayland_surface=&obj;data.hwnd=(HWND)1;
 data.rects.visible=(RECT){-128,-16,32,15};obj.xdg_surface=&xdg;
 minimize(FALSE);receive(TRUE);obj.current=obj.requested;
 memset(&obj.requested,0,sizeof(obj.requested));
}
static void expect(const char *name,int expected) {
 struct wayland_surface *surface=&obj;
 obj.processing=obj.requested;
 int result=DECISION;
 printf("%s %s\n",result==expected?"PASS":"FAIL",name);
 failures+=result!=expected;
}
int main(void) {
 reset();minimize(TRUE);receive(FALSE);receive(TRUE);
 expect("coalesced inactive then active restores iconic FL window",1);
 receive(TRUE);expect("repeated active configure preserves pending restore",1);
 reset();minimize(TRUE);receive(FALSE);
 obj.processing=obj.requested;memset(&obj.requested,0,sizeof(obj.requested));
 /* Incompatible geometry prevents acknowledging the inactive config. */
 receive(TRUE);expect("unacknowledged inactive geometry does not lose restore",1);
 reset();minimize(TRUE);receive(TRUE);
 expect("stale active configure does not undo minimize",0);
 reset();minimize(TRUE);receive(FALSE);expect("inactive configure stays minimized",0);
 reset();receive(FALSE);receive(TRUE);expect("ordinary activation is not restore",0);
 reset();minimize(TRUE);receive(FALSE);receive(TRUE);receive(FALSE);
 expect("deactivation cancels queued restore",0);
 reset();minimize(TRUE);receive(FALSE);receive(TRUE);minimize(FALSE);
 expect("explicit Win32 restore cancels queued restore",0);
 minimize(TRUE);receive(TRUE);expect("old restore does not survive new minimize epoch",0);
 receive(FALSE);receive(TRUE);expect("new minimize epoch can reactivate normally",1);
 reset();data.rects.visible=(RECT){-32000,-32000,-31840,-31969};
 minimize(TRUE);receive(FALSE);receive(TRUE);expect("sentinel minimized rect restores",1);
 reset();data.rects.visible=(RECT){10,10,170,41};
 minimize(TRUE);receive(FALSE);receive(TRUE);expect("positive iconic rect restores",1);
 printf("%d failures\n",failures);return !!failures;
}
'''.replace('DECISION',decision)
with tempfile.TemporaryDirectory(prefix='audion-restore-') as directory:
 p=pathlib.Path(directory);(p/'test.c').write_text(pre+configure+get_config+post)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
