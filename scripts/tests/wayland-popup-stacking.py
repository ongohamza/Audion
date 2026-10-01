#!/usr/bin/env python3
"""Run production Wayland popup restacking against a protocol-order model.
Arguments: winewayland.drv/wayland_surface.c. No graphical session required.
"""
import pathlib, subprocess, sys, tempfile
source = pathlib.Path(sys.argv[1]).read_text()
def function(name):
    start = source.index('static void ' + name + '(')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
functions = function('wayland_surface_reconfigure_subsurface')
pre = r'''
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef uintptr_t HWND;
typedef int BOOL;
typedef unsigned UINT;
typedef struct { int left,top,right,bottom; } RECT;
#define GW_CHILD 5
#define GW_HWNDNEXT 2
#define GA_PARENT 1
#define TRACE(...) ((void)0)
struct wl_surface { int id; };
struct wl_subsurface { struct wl_surface *surface; };
struct wayland_client_surface { HWND toplevel; struct wl_surface *wl_surface; struct wl_subsurface *wl_subsurface; };
struct wayland_surface {
 HWND hwnd,owner_hwnd;
 struct wl_surface *wl_surface;
 struct wl_subsurface *wl_subsurface;
 struct { RECT rect; double scale; } window;
 struct { unsigned serial; BOOL processed; } processing;
};
struct wayland_win_data { struct wayland_surface *wayland_surface; struct wayland_client_surface *client_surface; };
static struct wayland_win_data windows[16];
static HWND children[16],nexts[16],parents[16];
static int order[16],count,failures,locks,commits,positions,x,y,invalid;
static struct wayland_win_data *wayland_win_data_get(HWND h) { if(h<16 && windows[h].wayland_surface) { locks++;return &windows[h]; } return NULL; }
static void wayland_win_data_release(struct wayland_win_data *d) { (void)d; locks--; }
static HWND NtUserGetWindowRelative(HWND h,UINT which) { return h<16 ? (which==GW_CHILD ? children[h] : nexts[h]) : 0; }
static HWND NtUserGetAncestor(HWND h,UINT which) { (void)which;return h<16 ? parents[h] : 0; }
static RECT map_rect_to_surface(struct wayland_surface *s,RECT r) {
 r.left=round(r.left/s->window.scale);r.top=round(r.top/s->window.scale);
 r.right=round(r.right/s->window.scale);r.bottom=round(r.bottom/s->window.scale);return r;
}
static void OffsetRect(RECT *r,int x,int y) { r->left+=x;r->right+=x;r->top+=y;r->bottom+=y; }
static int index_of(int id) { for(int i=0;i<count;i++) if(order[i]==id)return i;return -1; }
static void restack(struct wl_subsurface *s,struct wl_surface *ref,int above) {
 int from=index_of(s->surface->id),to=index_of(ref->id);
 if(from<0 || to<0 || from==to) { invalid++;return; }
 int id=order[from]; memmove(order+from,order+from+1,(count-from-1)*sizeof(int));count--;
 to=index_of(ref->id)+above;memmove(order+to+1,order+to,(count-to)*sizeof(int));order[to]=id;count++;
}
static void wl_subsurface_place_above(struct wl_subsurface *s,struct wl_surface *r) { restack(s,r,1); }
static void wl_subsurface_place_below(struct wl_subsurface *s,struct wl_surface *r) { restack(s,r,0); }
static void wl_subsurface_set_position(struct wl_subsurface *s,int a,int b) { (void)s;positions++;x=a;y=b; }
static void wl_surface_commit(struct wl_surface *s) { (void)s;commits++; }
static void expect(const char *name,int ok) { printf("%s %s\n",ok?"PASS":"FAIL",name);failures+=!ok; }
'''
post = r'''
int main(void) {
 struct wl_surface wl[8];struct wl_subsurface subs[8];
 struct wayland_surface owner={.hwnd=1,.window={{100,200,1000,900},1.35}}, popup={.hwnd=4,.owner_hwnd=1,.window={{700,281,791,321},1.35}}, child={.hwnd=2},grandchild={.hwnd=3},other={.hwnd=5};
 struct wayland_client_surface gpu={.toplevel=1}, gpu2={.toplevel=1},foreign={.toplevel=9};
 for(int i=0;i<8;i++){wl[i].id=i;subs[i].surface=&wl[i];}
 owner.wl_surface=&wl[0];popup.wl_surface=&wl[3];popup.wl_subsurface=&subs[3];
 windows[1].wayland_surface=&owner;windows[2].wayland_surface=&child;windows[3].wayland_surface=&grandchild;windows[5].wayland_surface=&other;
 gpu.wl_surface=&wl[1];gpu.wl_subsurface=&subs[1];windows[3].client_surface=&gpu;
 children[1]=2;children[2]=3;parents[2]=1;parents[3]=2;
 order[0]=0;order[1]=1;order[2]=3;count=3;popup.processing.serial=1;popup.processing.processed=1;
 wayland_surface_reconfigure_subsurface(&popup);
 expect("popup is above the GPU surface owned by a grandchild",index_of(3)>index_of(1));
 expect("fractional position remains relative to its owner",x==444 && y==60);
 expect("parent is committed and processing is consumed",commits==1 && !popup.processing.serial && locks==0);
 /* Multiple editors on one native parent, with a foreign-parent client among descendants. */
 gpu2.wl_surface=&wl[2];gpu2.wl_subsurface=&subs[2];windows[2].client_surface=&gpu2;
 foreign.wl_surface=&wl[7];foreign.wl_subsurface=&subs[7];windows[5].client_surface=&foreign;
 nexts[2]=5;parents[5]=1;
 order[0]=0;order[1]=1;order[2]=2;order[3]=3;count=4;popup.processing.serial=1;popup.processing.processed=1;
 wayland_surface_reconfigure_subsurface(&popup);
 expect("popup is above every GPU child on the same native parent",index_of(3)>index_of(1) && index_of(3)>index_of(2));
 expect("foreign-parent clients never receive invalid sibling requests",invalid==0);
 expect("GPU clients retain their relative native stacking",index_of(1)<index_of(2));
 /* Keep an already open popup above this popup, matching existing ordering. */
 order[count++]=4;popup.processing.serial=1;popup.processing.processed=1;
 wayland_surface_reconfigure_subsurface(&popup);
 expect("existing higher popup remains above a reconfigured lower popup",index_of(4)>index_of(3));
 /* A client on the owner itself remains supported. */
 windows[1].client_surface=&gpu;windows[2].client_surface=0;windows[3].client_surface=0;children[1]=0;
 order[0]=0;order[1]=1;order[2]=3;count=3;popup.processing.serial=1;popup.processing.processed=1;
 wayland_surface_reconfigure_subsurface(&popup);
 expect("direct owner GPU clients retain their ordering",index_of(3)>index_of(1));
 windows[1].client_surface=0;popup.processing.serial=1;popup.processing.processed=1;
 wayland_surface_reconfigure_subsurface(&popup);
 expect("GDI-only owners retain popup positioning",index_of(3)>index_of(0));
 int before=positions;popup.processing.serial=0;wayland_surface_reconfigure_subsurface(&popup);
 expect("unchanged surfaces do not scan or restack",positions==before);
 popup.processing.serial=1;popup.processing.processed=0;wayland_surface_reconfigure_subsurface(&popup);
 expect("unprocessed state does not scan or restack",positions==before);
 popup.processing.processed=1;windows[1].wayland_surface=0;wayland_surface_reconfigure_subsurface(&popup);
 expect("destroyed owner is ignored safely",positions==before && locks==0);
 return !!failures;
}
'''
with tempfile.TemporaryDirectory(prefix='audion-popup-stacking-') as tmp:
    c=pathlib.Path(tmp)/'test.c';binary=pathlib.Path(tmp)/'test'
    c.write_text(pre+functions+post)
    subprocess.run(['cc','-Wall','-Wextra','-Wno-unused-function','-Werror','-fsanitize=address,undefined','-g',str(c),'-o',str(binary),'-lm'],check=True)
    subprocess.run([str(binary)],check=True)
