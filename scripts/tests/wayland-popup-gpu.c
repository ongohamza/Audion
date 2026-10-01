/* Native Wayland popup above a child-window OpenGL editor. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
static HWND root,editor;
static HDC dc;
static HGLRC gl;
static int phase;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {
 if(m==WM_PAINT){PAINTSTRUCT p;BeginPaint(h,&p);EndPaint(h,&p);return 0;}
 return DefWindowProcA(h,m,w,l);
}
static VOID CALLBACK tick(HWND h,UINT msg,UINT_PTR id,DWORD time) {
 (void)h;(void)msg;(void)id;(void)time;
 static const UINT keys[]={VK_DOWN,VK_RIGHT,VK_DOWN,VK_RETURN};
 if(phase<4){PostMessageA(editor,WM_KEYDOWN,keys[phase],0);PostMessageA(editor,WM_KEYUP,keys[phase],0);phase++;}
}
int main(void) {
 WNDCLASSA c={.lpfnWndProc=proc,.hInstance=GetModuleHandleA(0),.lpszClassName="AudionPopupGpuParent"};
 RegisterClassA(&c);
 root=CreateWindowA(c.lpszClassName,"Audion GPU child popup regression",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,800,600,0,0,c.hInstance,0);
 editor=CreateWindowA("STATIC","GPU editor",WS_CHILD|WS_VISIBLE,20,30,650,450,root,0,c.hInstance,0);
 dc=GetDC(editor);
 PIXELFORMATDESCRIPTOR p={.nSize=sizeof(p),.nVersion=1,.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,.iPixelType=PFD_TYPE_RGBA,.cColorBits=32};
 int fmt=ChoosePixelFormat(dc,&p);if(!fmt||!SetPixelFormat(dc,fmt,&p)||(gl=wglCreateContext(dc))==0||!wglMakeCurrent(dc,gl)){puts("FAIL OpenGL child setup");return 1;}
 glClearColor(0.2,0.3,0.8,1);glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(dc);
 MSG msg;for(int i=0;i<30;i++){while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);}Sleep(10);}
 HMENU top=CreatePopupMenu(),sub=CreatePopupMenu();AppendMenuA(sub,MF_STRING,123,"Select test preset");AppendMenuA(top,MF_POPUP,(UINT_PTR)sub,"Factory");
 RECT r;GetWindowRect(editor,&r);UINT_PTR timer=SetTimer(0,0,200,tick);
 UINT command=TrackPopupMenu(top,TPM_RETURNCMD|TPM_NONOTIFY,r.left+200,r.top+60,0,editor,0);
 KillTimer(0,timer);printf("%s nested menu selection command=%u\n",command==123?"PASS":"FAIL",command);DestroyMenu(top);
 wglMakeCurrent(0,0);wglDeleteContext(gl);ReleaseDC(editor,dc);DestroyWindow(root);
 return command==123?0:1;
}
