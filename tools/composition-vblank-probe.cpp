// WaitForVBlank must pace GUI updates without occupying a CPU core.
#include <windows.h>
#include <dxgi1_2.h>
#include <cstdio>
static unsigned long long ticks(FILETIME t) { return (static_cast<unsigned long long>(t.dwHighDateTime)<<32)|t.dwLowDateTime; }
int main() {
    IDXGIFactory1 *factory=nullptr; IDXGIAdapter1 *adapter=nullptr; IDXGIOutput *output=nullptr;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&factory)) ||
       FAILED(factory->EnumAdapters1(0,&adapter)) || FAILED(adapter->EnumOutputs(0,&output))) return 2;
    DXGI_OUTPUT_DESC desc={}; DEVMODEW mode={}; mode.dmSize=sizeof(mode);
    if(FAILED(output->GetDesc(&desc)) || !EnumDisplaySettingsW(desc.DeviceName,ENUM_CURRENT_SETTINGS,&mode)
       || mode.dmDisplayFrequency<=1) return 4;
    double expected=120000.0/mode.dmDisplayFrequency;
    LARGE_INTEGER frequency,start,end; FILETIME creation,exit,k0,u0,k1,u1;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    GetThreadTimes(GetCurrentThread(),&creation,&exit,&k0,&u0);
    for(unsigned i=0;i<120;i++) if(FAILED(output->WaitForVBlank())) return 3;
    GetThreadTimes(GetCurrentThread(),&creation,&exit,&k1,&u1); QueryPerformanceCounter(&end);
    double wall=1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart;
    double cpu=(ticks(k1)+ticks(u1)-ticks(k0)-ticks(u0))/10000.0;
    bool ok=wall>expected*0.8 && wall<expected*1.5+50 && cpu<wall*0.1;
    printf("%s vblank pacing wall_ms=%.3f cpu_ms=%.3f cpu_fraction=%.3f expected_wall_ms=%.3f\n",ok?"PASS":"FAIL",wall,cpu,cpu/wall,expected);
    output->Release();adapter->Release();factory->Release();return ok?0:1;
}
