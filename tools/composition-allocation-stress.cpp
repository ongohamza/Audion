#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_3.h>
#include <dcomp.h>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#define CHECK(x) do { HRESULT h=(x); if(FAILED(h)){printf("FAIL %d hr=%08lx\n",__LINE__,(unsigned long)h);exit(2);} }while(0)
int main(int argc,char**argv) {
    unsigned frames=argc>1?atoi(argv[1]):3000;
    ID3D11Device *gpu=nullptr;CHECK(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&gpu,nullptr,nullptr));
    IDXGIDevice *gd=nullptr;IDXGIAdapter *adapter=nullptr;IDXGIFactory2 *factory=nullptr;
    CHECK(gpu->QueryInterface(__uuidof(IDXGIDevice),(void**)&gd));CHECK(gd->GetAdapter(&adapter));CHECK(adapter->GetParent(__uuidof(IDXGIFactory2),(void**)&factory));
    DXGI_SWAP_CHAIN_DESC1 desc={};desc.Width=800;desc.Height=600;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    IDXGISwapChain1 *sc=nullptr;CHECK(factory->CreateSwapChainForComposition(gpu,&desc,nullptr,&sc));
    ID2D1Factory1 *df=nullptr;D2D1_FACTORY_OPTIONS options={};CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),&options,(void**)&df));
    ID2D1Device *dd=nullptr;ID2D1DeviceContext *ctx=nullptr;CHECK(df->CreateDevice(gd,&dd));CHECK(dd->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&ctx));
    ID2D1SolidColorBrush *brush=nullptr;D2D1_COLOR_F green={0,1,0,1};CHECK(ctx->CreateSolidColorBrush(&green,nullptr,&brush));
    ID2D1PathGeometry *path=nullptr;ID2D1GeometrySink *sink=nullptr;CHECK(df->CreatePathGeometry(&path));CHECK(path->Open(&sink));
    sink->BeginFigure({0,0},D2D1_FIGURE_BEGIN_FILLED);sink->AddBezier({{100,0},{200,80},{70,100}});sink->AddLine({0,0});sink->EndFigure(D2D1_FIGURE_END_CLOSED);CHECK(sink->Close());sink->Release();
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"D2D composition stress",WS_OVERLAPPEDWINDOW|WS_VISIBLE,40,40,840,660,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    IDCompositionDevice *comp=nullptr;IDCompositionTarget *target=nullptr;IDCompositionVisual *visual=nullptr;
    CHECK(DCompositionCreateDevice(gd,__uuidof(IDCompositionDevice),(void**)&comp));CHECK(comp->CreateTargetForHwnd(hwnd,TRUE,&target));CHECK(comp->CreateVisual(&visual));CHECK(visual->SetContent(sc));CHECK(target->SetRoot(visual));CHECK(comp->Commit());
    std::atomic<bool> stop(false);
    std::atomic<unsigned> allocations(0);
    std::thread allocator([&] {
        while (!stop.load()) {
            ID3D11Texture2D *textures[32]={};
            for (unsigned i=0;i<32;i++) {
                D3D11_TEXTURE2D_DESC td={}; td.Width=64+i;td.Height=64;
                td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
                td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
                td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                CHECK(gpu->CreateTexture2D(&td,nullptr,&textures[i]));
            }
            for (auto texture:textures)texture->Release();
            allocations.fetch_add(32);
        }
    });
    ID2D1Bitmap1 *bitmap=nullptr;
    for(unsigned frame=0;frame<frames;frame++) {
        if(frame%17==0) {
            ctx->SetTarget(nullptr);if(bitmap)bitmap->Release();bitmap=nullptr;
            CHECK(sc->ResizeBuffers(2,800+(frame%5)*8,600+(frame%7)*8,DXGI_FORMAT_UNKNOWN,0));
            IDXGISurface *surface=nullptr;CHECK(sc->GetBuffer(0,__uuidof(IDXGISurface),(void**)&surface));
            D2D1_BITMAP_PROPERTIES1 props={};props.pixelFormat={DXGI_FORMAT_R8G8B8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED};props.dpiX=props.dpiY=96;props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            CHECK(ctx->CreateBitmapFromDxgiSurface(surface,&props,&bitmap));surface->Release();ctx->SetTarget(bitmap);
        }
        ctx->BeginDraw();D2D1_COLOR_F bg={0.15f,0.1f,0.1f,1};ctx->Clear(&bg);
        for(unsigned i=0;i<120;i++) {
            D2D1_MATRIX_3X2_F matrix={1,0,0,1,float((i*13+frame)%600),float((i*23)%450)};ctx->SetTransform(&matrix);
            D2D1_COLOR_F color={float(i%5)/4,0.7f,float(i%7)/6,1};brush->SetColor(&color);
            if(i%3)ctx->FillGeometry(path,brush,nullptr);else ctx->DrawGeometry(path,brush,1.5f,nullptr);
        }
        CHECK(ctx->EndDraw());CHECK(sc->Present(0,0));
        if(frame%100==0){printf("frame %u\n",frame);fflush(stdout);}
        MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    stop.store(true);allocator.join();printf("concurrent allocations %u\n",allocations.load());
    CHECK(target->SetRoot(nullptr));CHECK(comp->Commit());visual->Release();target->Release();comp->Release();DestroyWindow(hwnd);
    ctx->SetTarget(nullptr);bitmap->Release();path->Release();brush->Release();ctx->Release();dd->Release();df->Release();sc->Release();factory->Release();adapter->Release();gd->Release();gpu->Release();
    printf("PASS %u D2D geometry/present frames with resizing and live compositor\n",frames);return 0;
}
