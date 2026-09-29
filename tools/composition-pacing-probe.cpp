// Regression for hidden composition producers accidentally waiting for window v-sync.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define CHECK(x) do { HRESULT hr=(x); if(FAILED(hr)){printf("FAIL line %d hr=%08lx\n",__LINE__,(unsigned long)hr);exit(2);} }while(0)
static double sample(ID3D11Device *device,ID3D11DeviceContext *context,IDXGIFactory2 *factory,UINT sync,UINT flags,bool windowed=false) {
    DXGI_SWAP_CHAIN_DESC1 desc={};desc.Width=800;desc.Height=600;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;desc.Flags=flags;
    HWND hwnd=nullptr;IDXGISwapChain1 *chain=nullptr;
    if(windowed) {
        hwnd=CreateWindowExW(0,L"STATIC",L"Visible swapchain pacing regression",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            40,40,840,660,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!hwnd)exit(5);desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        CHECK(factory->CreateSwapChainForHwnd(device,hwnd,&desc,nullptr,nullptr,&chain));
    } else CHECK(factory->CreateSwapChainForComposition(device,&desc,nullptr,&chain));
    IDXGISwapChain2 *chain2=nullptr;HANDLE event=nullptr;
    if(flags) { CHECK(chain->QueryInterface(__uuidof(IDXGISwapChain2),(void**)&chain2));CHECK(chain2->SetMaximumFrameLatency(1));event=chain2->GetFrameLatencyWaitableObject();if(!event)exit(3); }
    LARGE_INTEGER begin,end,freq;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&begin);
    for(unsigned i=0;i<30;i++) {
        if(event && WaitForSingleObject(event,5000)!=WAIT_OBJECT_0){puts("FAIL frame-latency wait timed out");exit(4);}
        ID3D11Texture2D *buffer=nullptr;ID3D11RenderTargetView *view=nullptr;
        CHECK(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&buffer));CHECK(device->CreateRenderTargetView(buffer,nullptr,&view));
        FLOAT color[]={i/30.0f,0.2f,0.4f,1};context->ClearRenderTargetView(view,color);view->Release();buffer->Release();
        CHECK(chain->Present(sync,0));
    }
    QueryPerformanceCounter(&end);
    if(event)CloseHandle(event);if(chain2)chain2->Release();chain->Release();if(hwnd)DestroyWindow(hwnd);
    return 1000.0*(end.QuadPart-begin.QuadPart)/freq.QuadPart;
}
int main() {
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    CHECK(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    IDXGIDevice *dxgi=nullptr;IDXGIAdapter *adapter=nullptr;IDXGIFactory2 *factory=nullptr;
    CHECK(device->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi));CHECK(dxgi->GetAdapter(&adapter));CHECK(adapter->GetParent(__uuidof(IDXGIFactory2),(void**)&factory));
    int failures=0;
    for(UINT flags : {0u,UINT(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)}) {
        double immediate=sample(device,context,factory,0,flags),requestedSync=sample(device,context,factory,1,flags);
        bool ok=requestedSync<=immediate*4+200;
        printf("%s hidden composition pacing flags=%u immediate_ms=%.3f requested_vsync_ms=%.3f\n",ok?"PASS":"FAIL",flags,immediate,requestedSync);
        failures+=!ok;
    }
    double visibleSync=sample(device,context,factory,1,0,true);
    bool visibleOk=visibleSync>200;
    printf("%s visible HWND retains requested v-sync ms=%.3f\n",visibleOk?"PASS":"FAIL",visibleSync);
    failures+=!visibleOk;
    factory->Release();adapter->Release();dxgi->Release();context->Release();device->Release();return failures?1:0;
}
