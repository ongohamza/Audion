/* Wine 11.18 private driver-input ABI regression test; not a Windows test. */
#include <windows.h>
#include <stdio.h>
/* NtUserCallHwndParam_SendHardwareInput, include/ntuser.h Wine 11.18. */
#define SEND_HARDWARE_INPUT 26
#define RAWINPUT 0x02
#define NO_VSCREEN_CLIP 0x04
struct hardware_params { UINT flags; const INPUT *input; LPARAM lparam; };
typedef ULONG_PTR (WINAPI *call_hwnd_param)(HWND, ULONG_PTR, DWORD);
static call_hwnd_param call;
static int failures;
static HWND child;
static int clicks;
static POINT click;
static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
 if(hwnd==child && msg==WM_LBUTTONDOWN) { clicks++; click.x=(short)LOWORD(lp); click.y=(short)HIWORD(lp); }
 return DefWindowProcW(hwnd,msg,wp,lp);
}
static void pump(void) { MSG m; DWORD until=GetTickCount()+100; do { while(PeekMessageW(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);} Sleep(1); } while((INT)(GetTickCount()-until)<0); }
/* The WoW64 private driver syscall forwards a native (64-bit) INPUT. */
static void send_native(HWND hwnd, struct hardware_params *params)
{
#ifndef _WIN64
 struct { DWORD type,pad; LONG dx,dy; DWORD data,flags,time,pad2; ULONGLONG extra; } native={0};
 struct hardware_params converted=*params;
 native.type=params->input->type; native.dx=params->input->mi.dx; native.dy=params->input->mi.dy;
 native.data=params->input->mi.mouseData; native.flags=params->input->mi.dwFlags;
 native.time=params->input->mi.time; native.extra=params->input->mi.dwExtraInfo;
 converted.input=(const INPUT *)&native;
 call(hwnd,(ULONG_PTR)&converted,SEND_HARDWARE_INPUT);
#else
 call(hwnd,(ULONG_PTR)params,SEND_HARDWARE_INPUT);
#endif
}
static void check(HWND hwnd, const char *name, UINT flags, LONG x, LONG y, LONG expected_x, LONG expected_y)
{
 INPUT input={0}; POINT p; struct hardware_params params={flags,&input,0};
 input.type=INPUT_MOUSE; input.mi.dx=x; input.mi.dy=y;
 input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE_NOCOALESCE;
 send_native(hwnd,&params); pump(); GetCursorPos(&p);
 printf("%s: got %ld,%ld expected %ld,%ld %s\n",name,p.x,p.y,expected_x,expected_y,p.x==expected_x&&p.y==expected_y?"PASS":"FAIL");
 if(p.x!=expected_x||p.y!=expected_y) failures++;
}
int main(void)
{
 HWND hwnd; POINT saved; RECT clip; WNDCLASSW wc={0}; int right,bottom;
 call=(call_hwnd_param)GetProcAddress(LoadLibraryW(L"win32u.dll"),"NtUserCallHwndParam");
 if(!call) return 2;
 wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(0); wc.lpszClassName=L"CoordinateTest";
 RegisterClassW(&wc); GetCursorPos(&saved); GetClipCursor(&clip);
 right=GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN);
 bottom=GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN);
 hwnd=CreateWindowW(wc.lpszClassName,L"Wine Wayland coordinate regression",WS_POPUP|WS_VISIBLE,right-100,bottom-100,400,400,0,0,wc.hInstance,0);
 child=CreateWindowW(wc.lpszClassName,L"Plugin editor",WS_CHILD|WS_VISIBLE,20,20,300,300,hwnd,0,wc.hInstance,0);
 pump(); ClipCursor(NULL);
 check(hwnd,"ordinary hardware stays clipped",RAWINPUT,right+80,bottom+60,right-1,bottom-1);
 check(hwnd,"Wayland preserves surface coordinates",RAWINPUT|NO_VSCREEN_CLIP,right+80,bottom+60,right+80,bottom+60);
 { INPUT input={0}; struct hardware_params params={RAWINPUT|NO_VSCREEN_CLIP,&input,0};
   input.type=INPUT_MOUSE; input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
   send_native(hwnd,&params); pump();
   input.mi.dwFlags=MOUSEEVENTF_LEFTUP; send_native(hwnd,&params); pump();
   printf("child click targeting: clicks=%d local=%ld,%ld %s\n",clicks,click.x,click.y,clicks==1&&click.x==160&&click.y==140?"PASS":"FAIL");
   if(clicks!=1||click.x!=160||click.y!=140) failures++;
 }
 { POINT point={right+80,bottom+60}; HWND hit=WindowFromPoint(point);
   printf("JUCE-style WindowFromPoint: hit=%p expected=%p %s\n",hit,child,hit==child?"PASS":"FAIL");
   if(hit!=child) failures++;
   point.x=right+10000; point.y=bottom+10000;
   hit=WindowFromPoint(point); printf("outside any window: %s\n",!hit?"PASS":"FAIL"); if(hit) failures++;
 }
 /* Resetting default desktop clipping must not warp a valid Wayland position. */
 ClipCursor(NULL); { POINT p; GetCursorPos(&p); printf("default clip reset: %s\n",p.x==right+80&&p.y==bottom+60?"PASS":"FAIL"); if(p.x!=right+80||p.y!=bottom+60) failures++; }
 { RECT desktop={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),right,bottom}; ClipCursor(&desktop); }
 check(hwnd,"explicit full-desktop clip remains enforced",RAWINPUT|NO_VSCREEN_CLIP,right+80,bottom+60,right-1,bottom-1);
 ClipCursor(NULL);
 check(hwnd,"release explicit desktop clip",RAWINPUT|NO_VSCREEN_CLIP,right+80,bottom+60,right+80,bottom+60);
 { RECT limited={100,100,300,300}; ClipCursor(&limited); }
 check(hwnd,"explicit ClipCursor remains enforced",RAWINPUT|NO_VSCREEN_CLIP,right+80,bottom+60,299,299);
 ClipCursor(NULL);
 check(hwnd,"ordinary hardware restores desktop clipping",RAWINPUT,right+80,bottom+60,right-1,bottom-1);
 { POINT point={right+80,bottom+60}; HWND hit=WindowFromPoint(point);
   printf("ordinary desktop hit-test remains bounded: %s\n",!hit?"PASS":"FAIL"); if(hit) failures++;
 }
 ClipCursor(&clip); SetCursorPos(saved.x,saved.y); DestroyWindow(hwnd);
 printf("RESULT: %d failures\n",failures); return failures?1:0;
}
