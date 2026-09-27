#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, label) do { if (!(c)) { printf("FAIL: %s\n", label); failures++; } } while (0)

static HGDIOBJ alias(HGDIOBJ object)
{
    return (HGDIOBJ)(uintptr_t)(0x18c00000000ULL | ((uintptr_t)object & 0xffff));
}

int main(int argc, char **argv)
{
    BOOL strict = !(argc > 1 && !strcmp(argv[1], "--legacy"));
    BITMAPINFO info = {0};
    void *bits = NULL;
    HBITMAP bitmap;
    HDC dc;
    HBRUSH brush;
    HRGN region;
    BOOL deleted;
    DWORD type;

    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = 1064;
    info.bmiHeader.biHeight = -23;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(bitmap && bits, "DIB allocation");
    if (!bitmap || !bits) return 2;
    deleted = DeleteObject(alias(bitmap));
    type = GetObjectType(bitmap);
    CHECK(deleted == !strict, "malformed bitmap deletion result");
    CHECK(type == (strict ? OBJ_BITMAP : 0), "live bitmap survives in strict mode");
    if (type) { memset(bits, 0x5a, 1064 * 23 * 4); CHECK(DeleteObject(bitmap), "full bitmap cleanup"); }

    dc = CreateCompatibleDC(NULL);
    CHECK(dc != NULL, "DC allocation");
    deleted = DeleteObject(alias(dc));
    type = GetObjectType(dc);
    CHECK(deleted == !strict, "malformed DC deletion result");
    CHECK(type == (strict ? OBJ_MEMDC : 0), "live DC survives in strict mode");
    if (type) CHECK(DeleteDC(dc), "full DC cleanup");

    brush = CreateSolidBrush(RGB(1,2,3));
    CHECK(brush != NULL, "brush allocation");
    deleted = DeleteObject((HGDIOBJ)((uintptr_t)brush & 0xffff));
    CHECK(deleted == !strict, "short brush deletion result");
    CHECK(GetObjectType(brush) == (strict ? OBJ_BRUSH : 0), "live brush survives in strict mode");
    if (GetObjectType(brush)) CHECK(DeleteObject(brush), "full brush cleanup");

    region = CreateRectRgn(0,0,10,10);
    CHECK(region != NULL, "region allocation");
    CHECK(DeleteObject((HGDIOBJ)((uintptr_t)region | 0xdeadbeef00000000ULL)), "full handle with ignored upper bits");
    CHECK(!GetObjectType(region), "full alias deleted object");
    CHECK(!DeleteObject(NULL), "null rejected");
    CHECK(DeleteObject(GetStockObject(BLACK_BRUSH)), "stock deletion remains harmless");
    CHECK(GetObjectType(GetStockObject(BLACK_BRUSH)) == OBJ_BRUSH, "stock object survives");
    printf("%s: %s mode, %d failures\n", failures ? "FAIL" : "PASS", strict ? "strict" : "legacy", failures);
    return failures ? 1 : 0;
}
