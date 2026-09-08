/* Generic nested OpenGL child/client-surface ordering probe. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

static LRESULT CALLBACK paint_window(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint;
        RECT rect;
        HBRUSH brush;
        HDC hdc = BeginPaint(hwnd, &paint);

        GetClientRect(hwnd, &rect);
        brush = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static void pump_messages(DWORD duration)
{
    DWORD end = GetTickCount() + duration;
    MSG message;

    do
    {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        Sleep(10);
    } while ((LONG)(end - GetTickCount()) > 0);
}

int main(void)
{
    const PIXELFORMATDESCRIPTOR pfd =
    {
        .nSize = sizeof(pfd),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 24,
    };
    WNDCLASSA class = {0};
    HWND top, container, child;
    HGLRC context;
    HDC hdc;
    int format, i;

    class.lpfnWndProc = paint_window;
    class.hInstance = GetModuleHandleA(NULL);
    class.lpszClassName = "wine-client-surface-ordering";
    if (!RegisterClassA(&class)) return 1;

    top = CreateWindowA(class.lpszClassName, "wine-client-surface-ordering",
                        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200, 500, 400,
                        NULL, NULL, class.hInstance, NULL);
    container = CreateWindowA(class.lpszClassName, "container",
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                              30, 30, 420, 300, top, NULL, class.hInstance, NULL);
    child = CreateWindowA("static", "OpenGL child", WS_CHILD | WS_VISIBLE,
                          50, 50, 300, 200, container, NULL, class.hInstance, NULL);
    if (!top || !container || !child) return 2;

    UpdateWindow(top);
    UpdateWindow(container);
    pump_messages(200);

    hdc = GetDC(child);
    format = ChoosePixelFormat(hdc, &pfd);
    if (!format || !SetPixelFormat(hdc, format, &pfd)) return 3;
    context = wglCreateContext(hdc);
    if (!context || !wglMakeCurrent(hdc, context)) return 4;

    glDrawBuffer(GL_BACK);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    if (!SwapBuffers(hdc)) return 5;
    fputs("ORDERING_INITIAL_PRESENT\n", stderr);
    fflush(stderr);

    for (i = 0; i < 5; ++i)
    {
        SetWindowPos(container, NULL, 30 + i * 8, 30 + i * 4, 420, 300,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        InvalidateRect(container, NULL, TRUE);
        UpdateWindow(container);
        GdiFlush();
        pump_messages(50);
    }

    fputs("ORDERING_PARENT_FLUSH_DONE\n", stderr);
    fflush(stderr);
    pump_messages(200);

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(context);
    ReleaseDC(child, hdc);
    DestroyWindow(top);
    return 0;
}
