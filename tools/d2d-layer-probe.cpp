#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d2d1_3.h>
#include <cstdio>
#include <cfloat>
#define CHECK(x) do { HRESULT hr = (x); if (FAILED(hr)) { printf("FAIL line %d HRESULT %08lx\n", __LINE__, (unsigned long)hr); return 2; } } while (0)
static HRESULT STDMETHODCALLTYPE fail_create_buffer(ID3D11Device *, const D3D11_BUFFER_DESC *,
    const D3D11_SUBRESOURCE_DATA *, ID3D11Buffer **buffer)
{
    if (buffer) *buffer = nullptr;
    return E_OUTOFMEMORY;
}
int main()
{
    ID3D11Device *gpu = nullptr;
    CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &gpu, nullptr, nullptr));
    IDXGIDevice *dxgi = nullptr;
    CHECK(gpu->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi));
    ID2D1Factory1 *factory = nullptr;
    D2D1_FACTORY_OPTIONS options = {};
    CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void **)&factory));
    ID2D1Device *device = nullptr;
    CHECK(factory->CreateDevice(dxgi, &device));
    ID2D1DeviceContext *ctx = nullptr;
    CHECK(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &ctx));
    D2D1_BITMAP_PROPERTIES1 props = {};
    props.pixelFormat = {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED};
    props.dpiX = props.dpiY = 96;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    ID2D1Bitmap1 *target = nullptr, *readback = nullptr;
    D2D1_SIZE_U size = {16,16};
    CHECK(ctx->CreateBitmap(size, nullptr, 0, &props, &target));
    props.bitmapOptions = (D2D1_BITMAP_OPTIONS)(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW);
    CHECK(ctx->CreateBitmap(size, nullptr, 0, &props, &readback));
    ID2D1SolidColorBrush *brush = nullptr;
    D2D1_COLOR_F green = {0,1,0,1}, red = {1,0,0,1};
    CHECK(ctx->CreateSolidColorBrush(&green, nullptr, &brush));
    ctx->SetTarget(target);
    D2D1_RECT_F bounds = {4,4,8,8}, whole = {0,0,16,16};
    D2D1_LAYER_PARAMETERS1 layer = {};
    layer.contentBounds = bounds;
    layer.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
    layer.maskTransform = {1,0,0,1,0,0};
    layer.opacity = 1;
    int failures = 0;
    ID2D1RectangleGeometry *mask = nullptr;
    CHECK(factory->CreateRectangleGeometry(&bounds, &mask));
    const char *names[] = {"AxisAlignedClip control", "PushLayer bounds", "Group opacity", "Nested layers", "Geometry mask", "Transformed mask", "Opacity brush", "Sprite tint/transform", "Transparent clear in opaque target layer", "DPI changed before pop", "Sprite atlas clamp", "Infinite transformed bounds", "Ellipse geometry mask", "Opacity brush snapshot", "Ignore-alpha layer", "Empty inverted bounds"};
    ID2D1EllipseGeometry *ellipse = nullptr;
    D2D1_ELLIPSE circle = {{6,6},2,2};
    CHECK(factory->CreateEllipseGeometry(&circle, &ellipse));
    ID2D1DeviceContext3 *ctx3 = nullptr;
    CHECK(ctx->QueryInterface(__uuidof(ID2D1DeviceContext3), (void **)&ctx3));
    ID2D1SpriteBatch *sprites = nullptr;
    CHECK(ctx3->CreateSpriteBatch(&sprites));
    D2D1_RECT_F dest = {0,0,4,4};
    D2D1_RECT_U src = {0,0,1,1};
    // AddSprites specifies component-wise modulation of the sampled pixel.
    D2D1_COLOR_F tint = {0,0.5f,0,0.5f};
    D2D1_MATRIX_3X2_F translation = {1,0,0,1,4,4};
    CHECK(sprites->AddSprites(1, &dest, &src, &tint, &translation, sizeof(dest), sizeof(src), sizeof(tint), sizeof(translation)));
    unsigned white = 0xffffffff;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
    ID2D1Bitmap1 *sprite_bitmap = nullptr;
    CHECK(ctx->CreateBitmap(D2D1_SIZE_U{1,1}, &white, 4, &props, &sprite_bitmap));
    ID2D1SolidColorBrush *half = nullptr;
    D2D1_COLOR_F half_color = {1,1,1,0.5f};
    CHECK(ctx->CreateSolidColorBrush(&half_color, nullptr, &half));
    ID2D1Bitmap1 *opaque = nullptr, *atlas = nullptr;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;
    CHECK(ctx->CreateBitmap(size, nullptr, 0, &props, &opaque));
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
    unsigned atlas_data[] = {0xffff0000, 0xff00ff00};
    CHECK(ctx->CreateBitmap(D2D1_SIZE_U{2,1}, atlas_data, 8, &props, &atlas));
    D2D1_RECT_U atlas_src = {1,0,2,1};
    CHECK(sprites->AddSprites(1, &dest, &atlas_src, nullptr, &translation, sizeof(dest), sizeof(atlas_src), 0, sizeof(translation)));
    for (int test = 0; test < 16; ++test) {
        ctx->SetTarget(test == 8 ? opaque : target);
        ctx->SetDpi(96,96);
        ctx->SetTransform(D2D1::Matrix3x2F::Identity());
        layer.contentBounds = bounds;
        layer.opacity = test == 2 || test == 3 ? 0.5f : 1;
        layer.geometricMask = test == 4 || test == 5 ? mask : nullptr;
        layer.maskTransform = {1,0,0,1,0,0};
        half->SetColor(&half_color);
        layer.opacityBrush = test == 6 || test == 13 ? half : nullptr;
        layer.layerOptions = test == 14 ? D2D1_LAYER_OPTIONS1_IGNORE_ALPHA : D2D1_LAYER_OPTIONS1_NONE;
        if (test == 15) layer.contentBounds = {8,8,4,4};
        if (test == 4 || test == 5) layer.contentBounds = whole;
        if (test == 5) layer.maskTransform = {1,0,0,1,4,0};
        if (test == 11) {
            layer.contentBounds = {-FLT_MAX,-FLT_MAX,FLT_MAX,FLT_MAX};
            ctx->SetTransform(D2D1::Matrix3x2F(2,2,-2,2,0,0));
        }
        if (test == 12) layer.geometricMask = ellipse;
        ctx->BeginDraw();
        ctx->Clear(&red);
        if (test == 7 || test == 10) {
            ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            ctx3->DrawSpriteBatch(sprites, test == 10 ? 1 : 0, 1, test == 10 ? atlas : sprite_bitmap,
                test == 10 ? D2D1_BITMAP_INTERPOLATION_MODE_LINEAR : D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                test == 10 ? D2D1_SPRITE_OPTIONS_CLAMP_TO_SOURCE_RECTANGLE : D2D1_SPRITE_OPTIONS_NONE);
        } else {
        if (test) ctx->PushLayer(&layer, nullptr);
        else ctx->PushAxisAlignedClip(&bounds, D2D1_ANTIALIAS_MODE_ALIASED);
        if (test == 13) half->SetColor(&green);
        if (test == 11) ctx->SetTransform(D2D1::Matrix3x2F::Identity());
        if (test == 3) ctx->PushLayer(&layer, nullptr);
        ctx->FillRectangle(&whole, brush);
        if (test == 2) ctx->FillRectangle(&whole, brush); // Opacity applies once to the group.
        if (test == 3) ctx->PopLayer();
        if (test == 8 || test == 14) ctx->Clear(nullptr);
        if (test == 9) ctx->SetDpi(192,192);
        if (test) ctx->PopLayer(); else ctx->PopAxisAlignedClip();
        }
        CHECK(ctx->EndDraw());
        CHECK(readback->CopyFromBitmap(nullptr, test == 8 ? opaque : target, nullptr));
        D2D1_MAPPED_RECT map = {};
        CHECK(readback->Map(D2D1_MAP_OPTIONS_READ, &map));
        unsigned outside = *(unsigned *)(map.bits + map.pitch + 4);
        unsigned inside = *(unsigned *)(map.bits + 5 * map.pitch + (test == 5 ? 9 : test == 10 ? 4 : 5) * 4);
        unsigned expected = test == 8 || test == 15 ? 0xffff0000 : test == 14 ? 0xff000000 : test == 2 || test == 6 || test == 7 || test == 13 ? 0xff808000 : test == 3 ? 0xffbf4000 : 0xff00ff00;
        bool pass = outside == (test == 11 ? 0xff00ff00 : 0xffff0000);
        for (unsigned shift = 0; shift < 32; shift += 8) {
            int delta = int((inside >> shift) & 255) - int((expected >> shift) & 255);
            pass = pass && delta >= -1 && delta <= 1;
        }
        if (test == 5) pass = pass && *(unsigned *)(map.bits + 5 * map.pitch + 5 * 4) == 0xffff0000;
        printf("%s: outside=%08x inside=%08x expected=%08x %s\n", names[test], outside, inside, expected, pass ? "PASS" : "FAIL");
        failures += !pass;
        CHECK(readback->Unmap());
    }
    ctx->SetDpi(96,96);
    ctx->SetTarget(target);
    layer.contentBounds = bounds;
    layer.geometricMask = nullptr;
    ctx->BeginDraw();
    ctx->PopLayer();
    HRESULT error = ctx->EndDraw();
    printf("Unmatched pop: %08lx %s\n", (unsigned long)error, error == D2DERR_POP_CALL_DID_NOT_MATCH_PUSH ? "PASS" : "FAIL");
    failures += error != D2DERR_POP_CALL_DID_NOT_MATCH_PUSH;
    ctx->BeginDraw();
    ctx->PushLayer(&layer, nullptr);
    error = ctx->EndDraw();
    printf("Unbalanced layer: %08lx %s\n", (unsigned long)error, error == D2DERR_PUSH_POP_UNBALANCED ? "PASS" : "FAIL");
    failures += error != D2DERR_PUSH_POP_UNBALANCED;
    ctx->BeginDraw();
    ctx->Clear(&red);
    CHECK(ctx->EndDraw());
    CHECK(readback->CopyFromBitmap(nullptr,target,nullptr));
    D2D1_MAPPED_RECT map = {};
    CHECK(readback->Map(D2D1_MAP_OPTIONS_READ,&map));
    bool restored = *(unsigned *)map.bits == 0xffff0000;
    printf("Unbalanced recovery target: %s\n", restored ? "PASS" : "FAIL");
    failures += !restored;
    CHECK(readback->Unmap());
    ctx->BeginDraw();
    layer.layerOptions = D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
    ctx->PushLayer(&layer, nullptr);
    ctx->PopLayer();
    error = ctx->EndDraw();
    printf("Unsupported background preserves error: %08lx %s\n", (unsigned long)error, error == E_NOTIMPL ? "PASS" : "FAIL");
    failures += error != E_NOTIMPL;
    layer.layerOptions = D2D1_LAYER_OPTIONS1_NONE;
    ctx->BeginDraw();
    ctx->PushLayer(&layer, nullptr);
    ctx->PopAxisAlignedClip();
    ctx->PushAxisAlignedClip(&whole,D2D1_ANTIALIAS_MODE_ALIASED);
    ctx->PopLayer();
    error = ctx->EndDraw();
    printf("Clip cannot consume layer bounds: %08lx %s\n", (unsigned long)error, error == D2DERR_POP_CALL_DID_NOT_MATCH_PUSH ? "PASS" : "FAIL");
    failures += error != D2DERR_POP_CALL_DID_NOT_MATCH_PUSH;
    ctx->BeginDraw();
    ctx->PushLayer(&layer, nullptr);
    ctx->FillRectangle(&whole,brush);
    ID3D11Device1 *gpu1 = nullptr;
    CHECK(gpu->QueryInterface(__uuidof(ID3D11Device1), (void **)&gpu1));
    void **vtable = *(void ***)gpu1;
    void *original_create_buffer = vtable[3];
    DWORD old_protection, ignored_protection;
    if (!VirtualProtect(&vtable[3],sizeof(void *),PAGE_EXECUTE_READWRITE,&old_protection)) return 2;
    vtable[3] = (void *)fail_create_buffer;
    ctx->PopLayer();
    vtable[3] = original_create_buffer;
    VirtualProtect(&vtable[3],sizeof(void *),old_protection,&ignored_protection);
    gpu1->Release();
    error = ctx->EndDraw();
    printf("PopLayer allocation failure: %08lx %s\n", (unsigned long)error, error == E_OUTOFMEMORY ? "PASS" : "FAIL");
    failures += error != E_OUTOFMEMORY;
    return failures;
}
