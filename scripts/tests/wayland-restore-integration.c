/* Native Wayland integration probe. After MINIMIZED, restore this window
 * using the compositor (taskbar or isolated KWin scripting). Pass any
 * argument to maximize before minimizing. The iconic rect models FL Studio. */
#include <windows.h>
#include <stdio.h>
static unsigned restores;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l)
{
 if(m==WM_SYSCOMMAND && (w&0xfff0)==SC_RESTORE) restores++;
 return DefWindowProcW(h,m,w,l);
}
static void pump(DWORD ms) { DWORD end=GetTickCount()+ms; MSG m; do {while(PeekMessageW(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(5);}while((INT)(end-GetTickCount())>0); }
int main(int argc,char **argv)
{
 WNDCLASSW wc={0}; RECT before,after; HWND h; DWORD start;
 wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(0);wc.lpszClassName=L"AudionRestoreTest";RegisterClassW(&wc);
 h=CreateWindowW(wc.lpszClassName,L"Audion restore integration",WS_POPUP|WS_VISIBLE|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,100,100,800,600,0,0,wc.hInstance,0);
 pump(800);
 if(argc>1){ShowWindow(h,SW_MAXIMIZE);pump(500);}
 GetWindowRect(h,&before);SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)|WS_MINIMIZE);SetWindowPos(h,0,-128,-16,160,31,SWP_NOACTIVATE|SWP_NOZORDER);pump(800);GetWindowRect(h,&after);
 printf("MINIMIZED %d rect=%ld,%ld,%ld,%ld\n",IsIconic(h),after.left,after.top,after.right,after.bottom);fflush(stdout);
 start=GetTickCount();while(IsIconic(h)&&GetTickCount()-start<10000)pump(20);
 pump(200);GetWindowRect(h,&after);
 int ok=!IsIconic(h)&&restores>0&&after.right-after.left>160&&after.bottom-after.top>31;
 printf("RESTORE %s commands=%u iconic=%d rect=%ld,%ld,%ld,%ld old=%ldx%ld\n",ok?"PASS":"FAIL",restores,IsIconic(h),after.left,after.top,after.right,after.bottom,before.right-before.left,before.bottom-before.top);
 DestroyWindow(h);return !ok;
}
