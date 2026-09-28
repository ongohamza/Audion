#include <windows.h>
#include <d3d11.h>
#include <d3d10.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>
#include <cstring>

// Private experimental ABI, intentionally independent of the stock DXVK vtables.
static const GUID snapshot_iid = {0x4ce354e9,0xda73,0x48bb,{0x94,0xd0,0x9a,0xab,0x72,0x73,0x02,0x3d}};
struct Snapshot : IUnknown { virtual HRESULT STDMETHODCALLTYPE GetCompositionSnapshot(IDXGISurface **surface, UINT64 *serial) = 0; };
static int failures;
#define CHECK(x) do { HRESULT result=(x); if (FAILED(result)) { printf("ERROR line %d: %08lx\n",__LINE__,(unsigned long)result); exit(2); } } while(0)
static void expect(bool value,const char *name) { printf("%s %s\n",value?"PASS":"FAIL",name); failures += !value; }
static unsigned pixel(ID3D11Device *dev, ID3D11DeviceContext *ctx, IDXGISurface *surf, unsigned x=0, unsigned y=0) {
    ID3D11Texture2D *src=nullptr,*read=nullptr;
    CHECK(surf->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&src));
    D3D11_TEXTURE2D_DESC d; src->GetDesc(&d);
    d.Usage=D3D11_USAGE_STAGING; d.BindFlags=0; d.MiscFlags=0; d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(dev->CreateTexture2D(&d,nullptr,&read)); ctx->CopyResource(read,src);
    D3D11_MAPPED_SUBRESOURCE map; CHECK(ctx->Map(read,0,D3D11_MAP_READ,0,&map));
    unsigned value=((unsigned*)((char*)map.pData+y*map.RowPitch))[x];
    ctx->Unmap(read,0); read->Release(); src->Release(); return value;
}
static void fill(ID3D11Device *dev, ID3D11DeviceContext *ctx, IDXGISwapChain1 *sc,unsigned color,const D3D11_BOX *box=nullptr) {
    ID3D11Texture2D *tex=nullptr; CHECK(sc->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&tex));
    D3D11_TEXTURE2D_DESC d; tex->GetDesc(&d);
    std::vector<unsigned> data(d.Width*d.Height,color);
    ctx->UpdateSubresource(tex,0,box,data.data(),d.Width*4,0); tex->Release();
}
static IDXGISurface *acquire(IDXGISwapChain1 *sc,Snapshot *api,UINT64 *serial) {
    IDXGISurface *s=nullptr;
    if (api) CHECK(api->GetCompositionSnapshot(&s,serial));
    else CHECK(sc->GetBuffer(0,__uuidof(IDXGISurface),(void**)&s)); // Reproduce current Wine reader.
    if (!s) { puts("ERROR snapshot returned no surface"); exit(2); } return s;
}
int main(int argc,char **argv) {
    ID3D11Device *dev=nullptr; ID3D11DeviceContext *ctx=nullptr;
    CHECK(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx));
    if(argc>1 && !strcmp(argv[1],"unmap-lock")) {
        ID3D10Multithread *mt=nullptr;CHECK(ctx->QueryInterface(__uuidof(ID3D10Multithread),(void**)&mt));
        D3D11_BUFFER_DESC desc={};desc.ByteWidth=256;desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        ID3D11Buffer *buffer=nullptr;CHECK(dev->CreateBuffer(&desc,nullptr,&buffer));
        D3D11_MAPPED_SUBRESOURCE mapped;CHECK(ctx->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped));
        HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),done=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        mt->Enter();
        std::thread worker([&]{SetEvent(ready);ctx->Unmap(buffer,0);SetEvent(done);});
        WaitForSingleObject(ready,INFINITE);
        expect(WaitForSingleObject(done,100)==WAIT_TIMEOUT,"buffer Unmap obeys forced immediate-context lock before reading mapped-image count");
        mt->Leave();expect(WaitForSingleObject(done,5000)==WAIT_OBJECT_0,"Unmap completes after context lock released");worker.join();
        // Exercise the relevant shared counter: image Map/Unmap in one thread
        // while dynamic-buffer Map/Unmap runs in another.
        D3D11_TEXTURE2D_DESC td={};td.Width=td.Height=16;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
        td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE;
        ID3D11Texture2D *texture=nullptr;CHECK(dev->CreateTexture2D(&td,nullptr,&texture));
        std::atomic<unsigned> errors{0};
        std::thread images([&]{for(unsigned i=0;i<1000;i++){D3D11_MAPPED_SUBRESOURCE m;if(FAILED(ctx->Map(texture,0,D3D11_MAP_WRITE,0,&m))){errors++;continue;}*(unsigned*)m.pData=i;ctx->Unmap(texture,0);}});
        for(unsigned i=0;i<1000;i++){D3D11_MAPPED_SUBRESOURCE m;if(FAILED(ctx->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&m))){errors++;continue;}*(unsigned*)m.pData=i;ctx->Unmap(buffer,0);}
        images.join();expect(errors==0,"concurrent image and buffer map/unmap complete successfully");
        texture->Release();CloseHandle(ready);CloseHandle(done);buffer->Release();mt->Release();ctx->Release();dev->Release();return failures?1:0;
    }
    IDXGIDevice *gd=nullptr; IDXGIAdapter *adapter=nullptr; IDXGIFactory2 *factory=nullptr;
    CHECK(dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&gd)); CHECK(gd->GetAdapter(&adapter));
    CHECK(adapter->GetParent(__uuidof(IDXGIFactory2),(void**)&factory));
    for(unsigned format=0;format<2;format++) for(unsigned count=2;count<=3;count++) {
        printf("Buffers=%u format=%s\n",count,format?"RGBA":"BGRA");
        DXGI_SWAP_CHAIN_DESC1 desc={}; desc.Width=16;desc.Height=16;desc.Format=format?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT;
        desc.BufferCount=count;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
        IDXGISwapChain1 *sc=nullptr;CHECK(factory->CreateSwapChainForComposition(dev,&desc,nullptr,&sc));
        Snapshot *api=nullptr; HRESULT qi=sc->QueryInterface(snapshot_iid,(void**)&api);
        expect(qi==S_OK,"snapshot capability exists"); UINT64 serial=0,first=0; IDXGISurface *s=nullptr;
        if(argc>1) {
            expect(api && api->GetCompositionSnapshot(&s,&serial)==DXGI_ERROR_UNSUPPORTED && !s,
                "unprotected context is rejected without changing protection state");
            if(api)api->Release();sc->Release();factory->Release();adapter->Release();gd->Release();ctx->Release();dev->Release();
            return failures?1:0;
        }
        if(api) {
            expect(api->GetCompositionSnapshot(&s,&serial)==S_FALSE && !s,"no frame before first present");
            CHECK(sc->Present(0,DXGI_PRESENT_TEST));
            expect(api->GetCompositionSnapshot(&s,&serial)==S_FALSE && !s,"TEST present does not publish");
        }
        fill(dev,ctx,sc,0xffff0000); CHECK(sc->Present(0,0));
        fill(dev,ctx,sc,0xff00ff00);
        IDXGISurface *retained=acquire(sc,api,&first);
        expect(pixel(dev,ctx,retained)==0xffff0000,"unpresented green does not replace presented red");
        if(count==2 && !format) {
            HWND hwnd=CreateWindowExW(0,L"STATIC",L"Composition snapshot regression",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                20,20,160,120,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            IDCompositionDevice *comp=nullptr; IDCompositionTarget *target=nullptr; IDCompositionVisual *visual=nullptr;
            CHECK(DCompositionCreateDevice(gd,__uuidof(IDCompositionDevice),(void**)&comp));
            CHECK(comp->CreateTargetForHwnd(hwnd,TRUE,&target));CHECK(comp->CreateVisual(&visual));
            CHECK(visual->SetContent(sc));CHECK(target->SetRoot(visual));CHECK(comp->Commit());
            Sleep(250);HDC dc=GetDC(hwnd);COLORREF color=GetPixel(dc,2,2);ReleaseDC(hwnd,dc);
            expect(color==RGB(255,0,0),"Wine compositor draws presented red instead of unpresented green");
            CHECK(target->SetRoot(nullptr));CHECK(comp->Commit());visual->Release();target->Release();comp->Release();DestroyWindow(hwnd);
        }
        CHECK(sc->Present(0,0));
        s=acquire(sc,api,&serial);expect(pixel(dev,ctx,s)==0xff00ff00,"next successful present publishes green");s->Release();
        if(api) expect(serial>first,"present serial advances");
        expect(pixel(dev,ctx,retained)==0xffff0000,"retained snapshot remains red across presentation");
        D3D11_BOX box={4,4,0,8,8,1};fill(dev,ctx,sc,0xff0000ff,&box);
        RECT dirty={4,4,8,8};DXGI_PRESENT_PARAMETERS params={};params.DirtyRectsCount=1;params.pDirtyRects=&dirty;
        CHECK(sc->Present1(0,0,&params));s=acquire(sc,api,&serial);
        expect(pixel(dev,ctx,s,0,0)==0xff00ff00 && pixel(dev,ctx,s,5,5)==0xff0000ff,"dirty rectangle preserves full presented frame");s->Release();
        // pScrollRect names the destination in the current frame.
        RECT scroll={8,4,12,8};POINT offset={4,0};params.pScrollRect=&scroll;params.pScrollOffset=&offset;
        box={0,0,0,2,2,1};fill(dev,ctx,sc,0xffffffff,&box);dirty={0,0,2,2};
        CHECK(sc->Present1(0,0,&params));s=acquire(sc,api,&serial);
        printf("scroll pixels destination=%08x dirty=%08x source=%08x\n",pixel(dev,ctx,s,9,5),pixel(dev,ctx,s,0,0),pixel(dev,ctx,s,5,5));
        expect(pixel(dev,ctx,s,9,5)==0xff0000ff && pixel(dev,ctx,s,0,0)==0xffffffff,"scroll and dirty rectangles compose together");s->Release();
        CHECK(sc->ResizeBuffers(count,20,20,DXGI_FORMAT_UNKNOWN,0));
        if(api) expect(api->GetCompositionSnapshot(&s,&serial)==S_FALSE && !s,"resize invalidates unpublished generation");
        expect(pixel(dev,ctx,retained)==0xffff0000,"retained snapshot survives resize");retained->Release();
        fill(dev,ctx,sc,0xffabcdef);CHECK(sc->Present(0,0));s=acquire(sc,api,&serial);
        DXGI_SURFACE_DESC sd;CHECK(s->GetDesc(&sd));expect(sd.Width==20 && pixel(dev,ctx,s)==0xffabcdef,"resized first presentation publishes new dimensions");s->Release();
        if(api) {
            std::atomic<unsigned> errors{0};
            std::thread producer([&] {
                for(unsigned i=0;i<60;i++) {
                    if(i%10==0 && FAILED(sc->ResizeBuffers(count,20+i,20+i,DXGI_FORMAT_UNKNOWN,0))) errors++;
                    fill(dev,ctx,sc,i%2?0xff112233:0xff445566);
                    if(FAILED(sc->Present(0,0))) errors++;
                }
            });
            for(unsigned i=0;i<60;i++) {
                IDXGISurface *frame=nullptr;UINT64 id=0;HRESULT hr=api->GetCompositionSnapshot(&frame,&id);
                if(hr==S_FALSE) { if(frame) errors++;continue; }
                if(hr!=S_OK || !frame) { errors++;continue; }
                unsigned firstPixel=pixel(dev,ctx,frame);
                if(firstPixel!=0xffabcdef && firstPixel!=0xff112233 && firstPixel!=0xff445566) errors++;
                if(pixel(dev,ctx,frame)!=firstPixel) errors++;
                frame->Release();
            }
            producer.join();expect(errors==0,"concurrent snapshots, presents and resizes preserve complete immutable frames");
        }
        s=acquire(sc,api,&serial);unsigned finalPixel=pixel(dev,ctx,s);
        if(api) api->Release();
        expect(sc->Release()==0,"snapshot does not retain swapchain ownership");
        expect(pixel(dev,ctx,s)==finalPixel,"snapshot survives swapchain destruction");s->Release();
    }
    factory->Release();adapter->Release();gd->Release();ctx->Release();dev->Release();
    printf("%d failures\n",failures);return failures?1:0;
}
