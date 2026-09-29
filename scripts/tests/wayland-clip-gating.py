#!/usr/bin/env python3
"""Run the real WAYLAND_ClipCursor entry point with a recording Wayland backend."""
import pathlib, subprocess, sys, tempfile
source = pathlib.Path(sys.argv[1]).read_text()
start = source.index('BOOL WAYLAND_ClipCursor(')
end = source.index('\n}', start) + 2
function = source[start:end]
pre = r'''
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
typedef int BOOL;
typedef void *HWND;
typedef struct { int left,top,right,bottom; } RECT;
typedef struct { int x,y; } POINT;
#define FALSE 0
#define TRUE 1
#define MDT_RAW_DPI 2
#define TRACE(...) ((void)0)
struct wl_surface { int unused; };
struct wayland_surface { struct wl_surface *wl_surface; struct { RECT rect; } window; };
struct wayland_win_data { struct wayland_surface *wayland_surface; };
struct wayland_pointer {
 pthread_mutex_t mutex; BOOL pending_warp; HWND constraint_hwnd;
 void *zwp_locked_pointer_v1,*wl_pointer,*wp_cursor_shape_device_v1;
 struct { void *wl_surface; } cursor; unsigned enter_serial;
};
static struct { struct wayland_pointer pointer; void *wp_pointer_warp_v1,*wl_display; } process_wayland;
static BOOL flstudio_compat;
static RECT desktop, last_rect;
static struct wl_surface wl;
static struct wayland_surface surface={&wl,{{100,100,500,400}}};
static struct wayland_win_data data={&surface};
static int confined,forced,failures;
static RECT NtUserGetVirtualScreenRect(int type) { if(type!=MDT_RAW_DPI) failures++; return desktop; }
static BOOL IsRectEmpty(const RECT *r) { return r->left>=r->right || r->top>=r->bottom; }
static void NtUserGetCursorPos(POINT *p) { p->x=200;p->y=200; }
static HWND NtUserGetForegroundWindow(void) { return (HWND)1; }
static struct wayland_win_data *wayland_win_data_get(HWND hwnd) { return &data; }
static void wayland_win_data_release(struct wayland_win_data *d) {}
static void wayland_surface_calc_confine(struct wayland_surface *s,const RECT *r,RECT *out) { *out=*r; }
static POINT map_point_to_surface(struct wayland_surface *s, POINT p) { return p; }
static int wl_fixed_from_int(int n) { return n*256; }
static void wp_pointer_warp_v1_warp_pointer(void *a,struct wl_surface *b,void *c,int d,int e,unsigned f) {}
static void zwp_locked_pointer_v1_set_cursor_position_hint(void *a,int b,int c) {}
static void wl_surface_commit(struct wl_surface *s) {}
static void wl_display_flush(void *p) {}
static void wayland_pointer_update_constraint(struct wl_surface *s,RECT *r,BOOL force) {
 confined=!!r; if(r)last_rect=*r; forced+=!!force;
}
static void check(const char *name,const RECT *clip,int expected) {
 confined=-1;
 WAYLAND_ClipCursor_PLACEHOLDER
 if(confined!=expected){printf("FAIL %s: confined=%d expected=%d\n",name,confined,expected);failures++;}
 else printf("PASS %s\n",name);
}
'''
# Forward declaration lets checks call the real implementation below.
pre = pre.replace('static void check(', 'BOOL WAYLAND_ClipCursor(const RECT *,BOOL);\nstatic void check(').replace('WAYLAND_ClipCursor_PLACEHOLDER', 'WAYLAND_ClipCursor(clip,FALSE);')
post = r'''
int main(void) {
 RECT full={0,0,1920,1080},narrow={150,150,300,300},large={-100,-100,2200,1200};
 RECT multi={-1920,-200,2560,1440},monitor={0,0,2560,1440},empty={0,0,0,0};
 pthread_mutex_init(&process_wayland.pointer.mutex,0);
 process_wayland.pointer.wl_pointer=(void *)1;
 process_wayland.pointer.cursor.wl_surface=(void *)1;
 desktop=full;flstudio_compat=1;
 check("desktop-wide clip releases visible FL pointer",&full,0);
 check("clip covering desktop also releases pointer",&large,0);
 check("narrow application clip preserved",&narrow,1);
 check("explicit release",NULL,0);
 flstudio_compat=0;check("other applications retain desktop clip",&full,1);
 flstudio_compat=1;process_wayland.pointer.cursor.wl_surface=NULL;
 check("hidden cursor retains desktop clip for relative input",&full,1);
 process_wayland.pointer.wp_cursor_shape_device_v1=(void *)1;
 check("shape-protocol visible cursor is released",&full,0);
 desktop=multi;check("negative-origin multi-monitor desktop",&multi,0);
 check("single-monitor explicit clip remains narrower than desktop",&monitor,1);
 desktop=empty;check("unavailable desktop bounds preserve clip",&full,1);
 desktop=full;process_wayland.pointer.pending_warp=1;
 check("visible pointer warp ends unconfined",&full,0);
 if(!forced){puts("FAIL warp fallback was not requested");failures++;}
 printf("%d failures\n",failures);return !!failures;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=pathlib.Path(directory)
    (path/'test.c').write_text(pre+function+post)
    subprocess.run(['cc','-std=c11','-pthread','-o',str(path/'test'),str(path/'test.c')],check=True)
    subprocess.run([str(path/'test')],check=True)
