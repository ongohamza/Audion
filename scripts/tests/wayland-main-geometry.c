/* Native Wine Wayland regression for FL's logical menu coordinate space.
 * Build with MinGW (64-bit or 32-bit). Use an isolated native Wayland prefix,
 * WINE_WAYLAND_FLSTUDIO=1, and a virtual desktop at least 1000x700.
 * Argument "disabled" expects WINE_WAYLAND_FLSTUDIO=0.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static int failures;
static void pump(void)
{
    DWORD end = GetTickCount() + 250;
    MSG msg;
    do
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(2);
    } while ((INT)(end - GetTickCount()) > 0);
}
static void check(HWND hwnd, const char *name, int x, int y, int width, int height)
{
    RECT rect;
    int ok;
    GetWindowRect(hwnd, &rect);
    ok = rect.left == x && rect.top == y && rect.right - rect.left == width && rect.bottom - rect.top == height;
    printf("%s %s got=%ld,%ld,%ld,%ld expected=%d,%d,%d,%d\n",
           ok ? "PASS" : "FAIL", name, rect.left, rect.top, rect.right, rect.bottom, x, y, x + width, y + height);
    failures += !ok;
}
int main(int argc, char **argv)
{
    WNDCLASSW wc = {0};
    HWND main, plugin, child;
    int left, top, right, bottom, x, y, width, height;
    int disabled = argc > 1 && !strcmp(argv[1], "disabled");
    SetProcessDPIAware();
    wc.hInstance = GetModuleHandleW(0);
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"TFruityLoopsMainForm";
    RegisterClassW(&wc);
    wc.lpszClassName = L"TPluginForm";
    RegisterClassW(&wc);
    left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    right = left + width;
    bottom = top + height;
    if (width < 1000 || height < 700) return 2;
    main = CreateWindowW(L"TFruityLoopsMainForm", L"Audion main geometry", WS_POPUP | WS_VISIBLE,
                         left + 100, top + 100, 800, 600, 0, 0, wc.hInstance, 0);
    child = CreateWindowW(L"STATIC", L"Menu anchor", WS_CHILD | WS_VISIBLE,
                          20, 20, 120, 30, main, 0, wc.hInstance, 0);
    pump();
    SetWindowPos(main, 0, left - 1200, bottom - 16, 800, 600, SWP_NOACTIVATE | SWP_NOZORDER);
    pump();
    x = disabled ? left - 1200 : left;
    y = disabled ? bottom - 16 : bottom - 600;
    check(main, "main logical position", x, y, 800, 600);
    check(child, "child menu anchor follows parent", x + 20, y + 20, 120, 30);
    SetWindowPos(main, 0, right - 16, top - 500, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    pump();
    check(main, "opposite desktop edges", disabled ? right - 16 : right - 800,
          disabled ? top - 500 : top, 800, 600);
    SetWindowPos(main, 0, left + 30, top + 40, 800, 600, SWP_NOACTIVATE | SWP_NOZORDER);
    pump();
    check(main, "valid main position unchanged", left + 30, top + 40, 800, 600);
    plugin = CreateWindowW(L"TPluginForm", L"Plugin geometry", WS_POPUP | WS_VISIBLE,
                          left - 1200, bottom - 16, 400, 300, 0, 0, wc.hInstance, 0);
    pump();
    check(plugin, "plugin logical rect unchanged", left - 1200, bottom - 16, 400, 300);
    DestroyWindow(plugin);
    SetWindowPos(main, 0, left - 1200, bottom - 16, width + 100, 600, SWP_NOACTIVATE | SWP_NOZORDER);
    pump();
    check(main, "oversized main unchanged", left - 1200, bottom - 16, width + 100, 600);
    SetWindowPos(main, 0, left - 1200, bottom - 16, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
    pump();
    check(main, "normal window not turned fullscreen", left - 1200, bottom - 16, width, height);
    ShowWindow(main, SW_HIDE);
    SetWindowPos(main, 0, left - 1200, bottom - 16, 800, 600, SWP_NOACTIVATE | SWP_NOZORDER);
    pump();
    check(main, "hidden main rect unchanged", left - 1200, bottom - 16, 800, 600);
    DestroyWindow(main);
    printf("RESULT %d failures\n", failures);
    return !!failures;
}
