// Differential tessellation coverage and a dense polyline Close() benchmark.
#include <windows.h>
#include <d2d1.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do { HRESULT hr=(x); if(FAILED(hr)){printf("FAIL line %d hr=%08lx\n",__LINE__,(unsigned long)hr);exit(2);} }while(0)
struct Sink : ID2D1TessellationSink {
    std::vector<D2D1_TRIANGLE> triangles;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
    void STDMETHODCALLTYPE AddTriangles(const D2D1_TRIANGLE *t,UINT n) override {triangles.insert(triangles.end(),t,t+n);}
    HRESULT STDMETHODCALLTYPE Close() override {return S_OK;}
};
static ID2D1PathGeometry *path(ID2D1Factory *factory,unsigned kind,unsigned count,bool hollow=false) {
    ID2D1PathGeometry *geometry=nullptr;ID2D1GeometrySink *sink=nullptr;
    CHECK(factory->CreatePathGeometry(&geometry));CHECK(geometry->Open(&sink));
    sink->SetFillMode(kind&1?D2D1_FILL_MODE_WINDING:D2D1_FILL_MODE_ALTERNATE);
    if(kind>=200) {
        // Repeated points, multiway crossings, and tangential curves at scales
        // where floating-point intersection parameters can coincide.
        float scale=kind%3==0?0.00001f:kind%3==1?1.0f:100000.0f;
        for(unsigned f=0;f<3;f++) {
            float centre=kind&1?float(f)*20.001f:0.0f;
            sink->BeginFigure(D2D1::Point2F((centre-10)*scale,0),D2D1_FIGURE_BEGIN_FILLED);
            for(unsigned i=0;i<32;i++) {
                if(kind&1) {
                    double angle=3.141592653589793*(1.0+double(i+1)/16.0);
                    double mid=angle-3.141592653589793/32.0;
                    double radius=10.0/std::cos(3.141592653589793/32.0);
                    D2D1_QUADRATIC_BEZIER_SEGMENT b={
                        {float(centre+radius*std::cos(mid))*scale,float(radius*std::sin(mid))*scale},
                        {float(centre+10*std::cos(angle))*scale,float(10*std::sin(angle))*scale}};
                    sink->AddQuadraticBezier(&b);
                } else {
                    float x=(i&1?10.0f:-10.0f)*scale, y=(float(i%4)-1.5f)*scale;
                    sink->AddLine(D2D1::Point2F(x,y));
                    if(i%3==0) sink->AddLine(D2D1::Point2F(x,y));
                }
            }
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }
        CHECK(sink->Close());sink->Release();return geometry;
    }
    unsigned state=kind+1;
    auto random=[&](){state=1664525*state+1013904223;return float(state>>8)/16777216.0f;};
    unsigned figures=kind<2?1:3;
    for(unsigned f=0;f<figures;f++) {
        sink->BeginFigure(D2D1::Point2F(0,float(f)*10),hollow?D2D1_FIGURE_BEGIN_HOLLOW:D2D1_FIGURE_BEGIN_FILLED);
        for(unsigned i=1;i<=count;i++) {
            float x=kind<2?float(i):random()*200;
            float y=kind<2?std::sin(i*0.05f)*20:random()*200;
            if(kind>=4 && i%3==0) {
                D2D1_QUADRATIC_BEZIER_SEGMENT b={{x+10,y-25},{x,y}};
                sink->AddQuadraticBezier(&b);
            } else sink->AddLine(D2D1::Point2F(x,y));
        }
        sink->EndFigure(hollow?D2D1_FIGURE_END_OPEN:D2D1_FIGURE_END_CLOSED);
    }
    CHECK(sink->Close());sink->Release();return geometry;
}
int main(int argc,char **argv) {
    if(argc<3 || argc>6){puts("usage: geometry-probe.exe record|verify triangles.bin [cases] [max-hollow-ms] [max-filled-ms]");return 2;}
    ID2D1Factory *factory=nullptr;CHECK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&factory));
    bool record=strcmp(argv[1],"record")==0;FILE *file=fopen(argv[2],record?"wb":"rb");if(!file)return 3;
    int failures=0;
    unsigned cases=argc>=4?strtoul(argv[3],nullptr,10):12;
    double hollowBudget=argc>=5?std::atof(argv[4]):0, filledBudget=argc>=6?std::atof(argv[5]):0;
    for(unsigned kind=0;kind<cases;kind++) {
        ID2D1PathGeometry *geometry=path(factory,kind,kind<2?300:12+kind%20);
        Sink sink;CHECK(geometry->Tessellate(nullptr,0.25f,&sink));geometry->Release();
        unsigned count=unsigned(sink.triangles.size());bool ok=true;
        if(record){fwrite(&count,sizeof(count),1,file);fwrite(sink.triangles.data(),sizeof(D2D1_TRIANGLE),count,file);}
        else {
            unsigned expected=0;if(fread(&expected,sizeof(expected),1,file)!=1)return 4;
            std::vector<D2D1_TRIANGLE> data(expected);if(fread(data.data(),sizeof(D2D1_TRIANGLE),expected,file)!=expected)return 4;
            ok=count==expected && (!count || !memcmp(data.data(),sink.triangles.data(),count*sizeof(D2D1_TRIANGLE)));
        }
        printf("%s geometry case=%u triangles=%u\n",ok?"PASS":"FAIL",kind,count);failures+=!ok;
    }
    fclose(file);
    LARGE_INTEGER start,end,freq;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&start);
    for(unsigned i=0;i<10;i++) path(factory,0,4000,true)->Release();
    QueryPerformanceCounter(&end);
    double elapsed=1000.0*(end.QuadPart-start.QuadPart)/freq.QuadPart;
    bool hollowOk=hollowBudget<=0 || elapsed<hollowBudget;
    printf("%s dense_polyline_10x4000_ms=%.3f budget_ms=%.3f\n",hollowOk?"PASS":"FAIL",elapsed,hollowBudget);
    failures+=!hollowOk;
    QueryPerformanceCounter(&start);
    for(unsigned i=0;i<10;i++) path(factory,0,4000,false)->Release();
    QueryPerformanceCounter(&end);
    elapsed=1000.0*(end.QuadPart-start.QuadPart)/freq.QuadPart;
    bool filledOk=filledBudget<=0 || elapsed<filledBudget;
    printf("%s filled_polyline_10x4000_ms=%.3f budget_ms=%.3f\n",filledOk?"PASS":"FAIL",elapsed,filledBudget);
    failures+=!filledOk;
    factory->Release();return failures?1:0;
}
