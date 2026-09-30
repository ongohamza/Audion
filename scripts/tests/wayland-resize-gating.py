#!/usr/bin/env python3
"""Exercise real FL custom-resize detection against a stubbed compositor."""
import pathlib, subprocess, sys, tempfile
s=pathlib.Path(sys.argv[1]).read_text()
def extract(name):
    start=s.find('static BOOL '+name+'(')
    return s[start:s.index('\n}',start)+2] if start>=0 else ''
helper=extract('wayland_flstudio_main_window')+extract('wayland_flstudio_resize')
if not helper: helper='static BOOL wayland_flstudio_resize(struct wayland_win_data *d, UINT f, const struct window_rects *r) { return FALSE; }'
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
typedef uint16_t WCHAR; typedef int BOOL; typedef unsigned UINT,DWORD; typedef void *HWND;
typedef int wl_fixed_t;
typedef struct {int x,y;} POINT;
typedef struct {int left,top,right,bottom;} RECT;
typedef struct {unsigned short Length,MaximumLength;WCHAR *Buffer;} UNICODE_STRING;
#define TRUE 1
#define FALSE 0
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define SWP_NOSIZE 1
#define SWP_SHOWWINDOW 64
#define SWP_HIDEWINDOW 128
#define GWL_STYLE 0
#define WS_VISIBLE 1
#define WS_MINIMIZE 2
#define WS_MAXIMIZE 4
#define XDG_TOPLEVEL_RESIZE_EDGE_NONE 0
#define XDG_TOPLEVEL_RESIZE_EDGE_TOP 1
#define XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM 2
#define XDG_TOPLEVEL_RESIZE_EDGE_LEFT 4
#define XDG_TOPLEVEL_RESIZE_EDGE_RIGHT 8
#define TRACE(...) ((void)0)
struct wayland_surface {void *xdg_toplevel; BOOL resizing; double scale; struct {unsigned serial;BOOL processed;} processing;};
struct window_rects {RECT window;};
struct wayland_win_data {HWND hwnd;struct wayland_surface *wayland_surface;struct window_rects rects;};
struct wayland_pointer {pthread_mutex_t mutex;HWND focused_hwnd;uint32_t left_button_serial,custom_resize_serial;wl_fixed_t left_button_x,left_button_y;};
static struct {struct wayland_pointer pointer;struct {pthread_mutex_t mutex;void *wl_seat;} seat;void *wl_display;} process_wayland;
static BOOL flstudio_compat=TRUE;
static DWORD style=WS_VISIBLE;
static int other_class, requests, last_edge, failures;
static DWORD NtUserGetWindowLongW(HWND h,int index){return style;}
static int NtUserGetClassName(HWND h,BOOL real,UNICODE_STRING *name){const char *s=other_class?"OtherForm":"TFruityLoopsMainForm";int i;for(i=0;s[i];i++)name->Buffer[i]=s[i];name->Buffer[i]=0;return i;}
static BOOL wayland_surface_is_toplevel(struct wayland_surface *s){return !!s->xdg_toplevel;}
static double wl_fixed_to_double(wl_fixed_t x){return x/256.;}
static POINT map_point_from_surface(struct wayland_surface *s,POINT p){return (POINT){round(p.x*s->scale),round(p.y*s->scale)};}
static void xdg_toplevel_resize(void *s,void *seat,uint32_t serial,unsigned edge){requests++;last_edge=edge;}
static void wl_display_flush(void *d){}
static void expect(const char *name,int ok){printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok;}
'''
post=r'''
int main(void){
 struct wayland_surface surface={.xdg_toplevel=(void*)1,.scale=1};
 struct wayland_win_data data={.hwnd=(HWND)1,.wayland_surface=&surface,.rects={{100,100,900,700}}};
 struct wayland_pointer *p=&process_wayland.pointer;
 struct window_rects next={{{0}}};
 int i; const unsigned edges[]={4,8,1,2,5,9,6,10};
 pthread_mutex_init(&p->mutex,0);pthread_mutex_init(&process_wayland.seat.mutex,0);
 p->focused_hwnd=data.hwnd;process_wayland.seat.wl_seat=(void*)1;
 for(i=0;i<8;i++){
  unsigned e=edges[i];next=data.rects;
  p->left_button_x=((e&4)?2:(e&8)?798:400)*256;
  p->left_button_y=((e&1)?2:(e&2)?598:300)*256;
  if(e&4)next.window.left+=10;if(e&8)next.window.right+=10;
  if(e&1)next.window.top+=10;if(e&2)next.window.bottom+=10;
  p->left_button_serial=100+i;
  expect("edge/corner handoff",wayland_flstudio_resize(&data,0,&next)&&last_edge==e);
  expect("one request per press",!wayland_flstudio_resize(&data,0,&next));
 }
 requests=0;next=data.rects;next.window.left+=10;p->left_button_x=2*256;p->left_button_y=300*256;p->left_button_serial=200;
 flstudio_compat=FALSE;expect("compatibility disabled",!wayland_flstudio_resize(&data,0,&next));flstudio_compat=TRUE;
 other_class=1;expect("other classes excluded",!wayland_flstudio_resize(&data,0,&next));other_class=0;
 style=WS_VISIBLE|WS_MAXIMIZE;expect("maximized excluded",!wayland_flstudio_resize(&data,0,&next));
 style=WS_VISIBLE|WS_MINIMIZE;expect("minimized excluded",!wayland_flstudio_resize(&data,0,&next));
 style=0;expect("hidden excluded",!wayland_flstudio_resize(&data,0,&next));style=WS_VISIBLE;
 expect("no-size flag",!wayland_flstudio_resize(&data,SWP_NOSIZE,&next));
 expect("show transition",!wayland_flstudio_resize(&data,SWP_SHOWWINDOW,&next));
 expect("hide transition",!wayland_flstudio_resize(&data,SWP_HIDEWINDOW,&next));
 p->left_button_serial=0;expect("programmatic change excluded",!wayland_flstudio_resize(&data,0,&next));p->left_button_serial=200;
 p->focused_hwnd=(HWND)2;expect("unfocused window excluded",!wayland_flstudio_resize(&data,0,&next));p->focused_hwnd=data.hwnd;
 p->left_button_x=400*256;expect("interior click excluded",!wayland_flstudio_resize(&data,0,&next));p->left_button_x=2*256;
 surface.resizing=TRUE;expect("native resize does not restart",!wayland_flstudio_resize(&data,0,&next));surface.resizing=FALSE;
 surface.processing.serial=1;surface.processing.processed=FALSE;expect("configure callback excluded",!wayland_flstudio_resize(&data,0,&next));surface.processing.serial=0;
 next.window.right+=10;expect("pure move excluded",!wayland_flstudio_resize(&data,0,&next));next.window.right-=10;
 process_wayland.seat.wl_seat=0;expect("absent seat ignored",!wayland_flstudio_resize(&data,0,&next));process_wayland.seat.wl_seat=(void*)1;
 surface.scale=1.35;next=data.rects;next.window.right+=10;p->left_button_x=590*256;
 expect("fractional-scale right edge",wayland_flstudio_resize(&data,0,&next)&&last_edge==8);
 expect("only eligible request sent",requests==1);
 p->left_button_serial=201;p->left_button_x=2*256;p->left_button_y=300*256;
 expect("opposite-edge press excluded",!wayland_flstudio_resize(&data,0,&next));
 surface.scale=1;next=data.rects;next.window.right+=10;p->left_button_x=798*256;p->left_button_y=2*256;
 expect("corner keeps both axes when first motion is horizontal",wayland_flstudio_resize(&data,0,&next)&&last_edge==9);
 p->left_button_serial=202;next=data.rects;next.window.top+=10;
 expect("corner keeps both axes when first motion is vertical",wayland_flstudio_resize(&data,0,&next)&&last_edge==9);
 p->left_button_serial=203;p->left_button_x=12*256;p->left_button_y=300*256;next=data.rects;next.window.left+=10;
 expect("outside border tolerance excluded",!wayland_flstudio_resize(&data,0,&next));
 p->left_button_x=11*256;
 expect("inside border tolerance included",wayland_flstudio_resize(&data,0,&next)&&last_edge==4);
 p->left_button_serial=204;next.window.right+=5;
 expect("unanchored resize excluded",!wayland_flstudio_resize(&data,0,&next));

 return !!failures;
}
'''
post=post.replace('struct window_rects next={{{0}}};','struct window_rects next={0};')
with tempfile.TemporaryDirectory(prefix='audion-resize-') as tmp:
 c=pathlib.Path(tmp)/'test.c';c.write_text(pre+helper+post);exe=pathlib.Path(tmp)/'test'
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-g','-pthread',str(c),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
