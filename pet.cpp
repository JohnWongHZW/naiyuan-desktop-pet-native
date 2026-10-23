#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>

using namespace Gdiplus;
namespace pet {
constexpr int ID_IDLE=101, ID_CLICK=102, ID_BGM=103, ID_SOUND=104;
constexpr int CMD_BOUNCE=201, CMD_BGM=202, CMD_EXIT=203;
const wchar_t* CLASS_NAME=L"NaiyuanNativePet";
struct Resource { const BYTE* data; DWORD size; };
Resource resource(int id) {
    auto module=GetModuleHandleW(nullptr);
    auto found=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
    if (!found) throw std::runtime_error("missing resource");
    return {static_cast<const BYTE*>(LockResource(LoadResource(module,found))),SizeofResource(module,found)};
}

class Animation {
    IStream* stream=nullptr;
    std::unique_ptr<Image> image;
    std::vector<UINT> delays;
    UINT index=0;
    ULONGLONG next=0;
public:
    ~Animation() { image.reset(); if(stream) stream->Release(); }
    void load(int id) {
        auto r=resource(id);
        stream=SHCreateMemStream(r.data,r.size);
        if(!stream) throw std::runtime_error("image stream");
        image.reset(Image::FromStream(stream,FALSE));
        if(!image || image->GetLastStatus()!=Ok) throw std::runtime_error("GIF decode");
        UINT count=image->GetFrameCount(&FrameDimensionTime);
        delays.assign(std::max(1u,count),100);
        UINT size=image->GetPropertyItemSize(PropertyTagFrameDelay);
        if(size) {
            std::vector<BYTE> property(size);
            auto item=reinterpret_cast<PropertyItem*>(property.data());
            if(image->GetPropertyItem(PropertyTagFrameDelay,size,item)==Ok) {
                auto values=static_cast<UINT*>(item->value);
                for(UINT i=0;i<delays.size() && (i+1)*sizeof(UINT)<=item->length;i++)
                    delays[i]=std::max(20u,values[i]*10);
            }
        }
        reset(GetTickCount64());
    }
    void reset(ULONGLONG now) {
        index=0; image->SelectActiveFrame(&FrameDimensionTime,0); next=now+delays[0];
    }
    bool advance(ULONGLONG now) {
        bool changed=false;
        // Keep GIF time independent of rendering / scheduler jitter.
        for(int catchup=0;now>=next && catchup<500;catchup++) {
            index=(index+1)%delays.size(); next+=delays[index]; changed=true;
        }
        if(changed) image->SelectActiveFrame(&FrameDimensionTime,index);
        return changed;
    }
    Image* get() {return image.get();}
    UINT frames() const {return static_cast<UINT>(delays.size());}
    UINT frame() const {return index;}
};

class Audio {
    std::wstring tempFile;
    bool opened=false, playing=false;
    bool command(const wchar_t* text) { return mciSendStringW(text,nullptr,0,nullptr)==0; }
public:
    ~Audio() {
        PlaySoundW(nullptr,nullptr,0);
        if(opened) command(L"close pet_bgm");
        if(!tempFile.empty()) DeleteFileW(tempFile.c_str());
    }
    bool open() {
        if(opened) return true;
        wchar_t dir[MAX_PATH], file[MAX_PATH];
        if(!GetTempPathW(MAX_PATH,dir) || !GetTempFileNameW(dir,L"nyp",0,file)) return false;
        tempFile=file;
        auto r=resource(ID_BGM);
        HANDLE out=CreateFileW(file,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);
        if(out==INVALID_HANDLE_VALUE) return false;
        DWORD written=0;
        BOOL ok=WriteFile(out,r.data,r.size,&written,nullptr); CloseHandle(out);
        if(!ok || written!=r.size) return false;
        std::wstring cmd=L"open \""+tempFile+L"\" type mpegvideo alias pet_bgm";
        opened=command(cmd.c_str());
        return opened;
    }
    bool play() {
        if(!open()) return false;
        playing=command(L"play pet_bgm repeat");
        return playing;
    }
    void pause() {if(opened && command(L"pause pet_bgm")) playing=false;}
    bool isPlaying() const {return playing;}
    bool click() {
        auto r=resource(ID_SOUND);
        return PlaySoundW(reinterpret_cast<LPCWSTR>(r.data),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT)!=FALSE;
    }
    std::wstring mode() {
        wchar_t value[64]={};
        if(opened) mciSendStringW(L"status pet_bgm mode",value,64,nullptr);
        return value;
    }
};

struct Motion {
    double x=0,y=0,vx=4,vy=4,angle=0,speed=0;
    double scale=1;
    RECT work={0,0,1920,1080};
    int side=200;
    bool bouncing=false,dragging=false,touching=false,rotationPaused=false;
    void resetRotation() {angle=0;speed=0;rotationPaused=false;}
    void toggleBounce() {
        bouncing=!bouncing;
        if(bouncing) {touching=false; if(vx==0 && vy==0) vx=vy=4;}
        else resetRotation();
    }
    void release(double dx,double dy) {
        dragging=false;
        if(bouncing) {
            if(dx==0 && dy==0) {vx=(rand()%2?4:-4);vy=(rand()%2?4:-4);}
            else {vx=std::clamp(dx/scale,-15.,15.);vy=std::clamp(dy/scale,-15.,15.);}
        }
    }
    bool step() {
        bool changed=false;
        if(bouncing && !dragging) {
            double nx=x+vx*scale,ny=y+vy*scale;
            bool hit=false;
            double right=std::max(work.left,work.right-side),bottom=std::max(work.top,work.bottom-side);
            if(nx<=work.left || nx>=right) {vx=-vx; nx=std::clamp(nx,double(work.left),right);hit=true;}
            if(ny<=work.top || ny>=bottom) {vy=-vy; ny=std::clamp(ny,double(work.top),bottom);hit=true;}
            if(hit && !touching) {touching=true; if(speed==0) speed=(rand()%2?.5:-.5);rotationPaused=false;}
            else if(!hit) touching=false;
            x=nx;y=ny;changed=true;
        }
        if(speed!=0 && !rotationPaused) {angle=std::fmod(angle+speed,360.);changed=true;}
        return changed;
    }
};

class Pet {
public:
    HWND hwnd=nullptr;
    Motion motion;
    Animation idle,clicked;
    Audio audio;
    bool special=false,resumeBgm=false,moved=false;
    ULONGLONG specialEnd=0;
    POINT offset={},start={},last={},velocity={};
    HDC dc=nullptr;
    HBITMAP dib=nullptr;
    HGDIOBJ oldBitmap=nullptr;
    BYTE* pixels=nullptr;
    int dibSide=0;
    std::unique_ptr<Bitmap> canvas;
    ~Pet() {freeCanvas();}
    void freeCanvas() {
        canvas.reset();
        if(dc) {SelectObject(dc,oldBitmap);DeleteDC(dc);dc=nullptr;}
        if(dib) {DeleteObject(dib);dib=nullptr;}
    }
    void workArea() {
        POINT origin={0,0}; MONITORINFO info={sizeof(info)};
        GetMonitorInfoW(MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY),&info);
        motion.work=info.rcWork;
    }
    void dpi(UINT value) {motion.scale=value/96.;motion.side=MulDiv(200,value,96);}
    void load() {idle.load(ID_IDLE);clicked.load(ID_CLICK);workArea();}
    void render() {
        const int side=motion.side;
        if(!canvas || dibSide!=side) {
            freeCanvas();
            BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=side;info.bmiHeader.biHeight=-side;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            dc=CreateCompatibleDC(nullptr);
            dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
            if(!dib || !dc) throw std::runtime_error("window bitmap");
            oldBitmap=SelectObject(dc,dib);dibSide=side;
            canvas=std::make_unique<Bitmap>(side,side,side*4,PixelFormat32bppPARGB,pixels);
        }
        {
            Graphics g(canvas.get());
            g.Clear(Color(0,0,0,0));
            g.SetCompositingMode(CompositingModeSourceOver);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.SetPixelOffsetMode(PixelOffsetModeHalf);
            g.TranslateTransform(side/2.f,side/2.f);
            g.RotateTransform(static_cast<REAL>(motion.angle));
            Image* image=special?clicked.get():idle.get();
            float ratio=std::min(float(side)/image->GetWidth(),float(side)/image->GetHeight());
            float width=image->GetWidth()*ratio,height=image->GetHeight()*ratio;
            ImageAttributes attrs;attrs.SetWrapMode(WrapModeTileFlipXY);
            g.DrawImage(image,RectF(-width/2,-height/2,width,height),0,0,
                        static_cast<REAL>(image->GetWidth()),static_cast<REAL>(image->GetHeight()),UnitPixel,&attrs);
            g.Flush(FlushIntentionSync);
        }
        POINT dest={LONG(std::lround(motion.x)),LONG(std::lround(motion.y))},src={0,0};SIZE size={side,side};
        BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if(!UpdateLayeredWindow(hwnd,nullptr,&dest,&size,dc,&src,0,&blend,ULW_ALPHA))
            throw std::runtime_error("layered window");
    }
    void beginDrag(POINT point, POINT client) {
        motion.dragging=true;motion.rotationPaused=true;moved=false;
        offset=client;start=last=point;velocity={0,0};SetCapture(hwnd);
    }
    void track(POINT point) {
        if(!motion.dragging) return;
        if(std::abs(point.x-start.x)>3*motion.scale || std::abs(point.y-start.y)>3*motion.scale) moved=true;
        velocity={point.x-last.x,point.y-last.y};
        if(velocity.x || velocity.y) {motion.x=point.x-offset.x;motion.y=point.y-offset.y;}
        last=point;
    }
    void endDrag(bool allowClick,ULONGLONG now) {
        if(!motion.dragging) return;
        motion.release(velocity.x,velocity.y);
        ReleaseCapture();
        if(allowClick && !moved && !special) click(now);
    }
    void click(ULONGLONG now) {
        if(special) return;
        special=true;specialEnd=now+4700;clicked.reset(now);
        audio.click();resumeBgm=audio.isPlaying();if(resumeBgm) audio.pause();render();
    }
    void toggleBgm() {
        resumeBgm=false;
        if(audio.isPlaying()) audio.pause();
        else if(!audio.play()) MessageBoxW(hwnd,L"暂时无法播放背景音乐。",L"奶螈桌宠",MB_OK|MB_ICONINFORMATION);
    }
    void tick(ULONGLONG now,bool pollCursor=true) {
        bool dirty=false;
        if(motion.dragging && pollCursor) {POINT point;GetCursorPos(&point);track(point);dirty=true;}
        if(special && now>=specialEnd) {
            special=false;idle.reset(now);if(resumeBgm) audio.play();resumeBgm=false;dirty=true;
        }
        dirty=(special?clicked:idle).advance(now)||dirty;
        dirty=motion.step()||dirty;
        if(dirty) render();
    }
    void command(int id) {
        if(id==CMD_BOUNCE) {motion.toggleBounce();render();}
        if(id==CMD_BGM) toggleBgm();
        if(id==CMD_EXIT) DestroyWindow(hwnd);
    }
    void menu(POINT point) {
        HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,CMD_BOUNCE,motion.bouncing?L"关闭 满屏弹跳":L"开启 满屏弹跳");
        AppendMenuW(menu,MF_STRING,CMD_BGM,L"开启/关闭 BGM");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,CMD_EXIT,L"退出");
        SetForegroundWindow(hwnd);
        int id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,hwnd,nullptr);
        DestroyMenu(menu);PostMessageW(hwnd,WM_NULL,0,0);command(id);
    }
};

LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto p=reinterpret_cast<Pet*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        p=static_cast<Pet*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        p->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));
    }
    if(!p) return DefWindowProcW(hwnd,msg,wp,lp);
    try {
        switch(msg) {
        case WM_TIMER:p->tick(GetTickCount64());return 0;
        case WM_LBUTTONDOWN: {
            POINT point;GetCursorPos(&point);
            p->beginDrag(point,{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});return 0;
        }
        case WM_MOUSEMOVE:
            if(p->motion.dragging) {
                POINT point;GetCursorPos(&point);
                if(std::abs(point.x-p->start.x)>3*p->motion.scale || std::abs(point.y-p->start.y)>3*p->motion.scale) p->moved=true;
            }
            return 0;
        case WM_LBUTTONUP:p->endDrag(true,GetTickCount64());return 0;
        case WM_CAPTURECHANGED:if(p->motion.dragging) p->endDrag(false,GetTickCount64());return 0;
        case WM_CANCELMODE:p->endDrag(false,GetTickCount64());return 0;
        case WM_CONTEXTMENU: {
            POINT point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(point.x==-1 && point.y==-1) GetCursorPos(&point);
            p->menu(point);return 0;
        }
        case WM_COMMAND:p->command(LOWORD(wp));return 0;
        case WM_DPICHANGED: {
            RECT* rect=reinterpret_cast<RECT*>(lp);p->dpi(HIWORD(wp));
            p->motion.x=rect->left;p->motion.y=rect->top;p->workArea();p->render();return 0;
        }
        case WM_DISPLAYCHANGE:case WM_SETTINGCHANGE:p->workArea();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT: {PAINTSTRUCT ps;BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;}
        case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
        }
    } catch(...) {
        MessageBoxW(hwnd,L"桌宠暂时无法显示，请关闭后重新打开。",L"奶螈桌宠",MB_OK|MB_ICONERROR);
        DestroyWindow(hwnd);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

void create(Pet& p,bool show=true) {
    HINSTANCE module=GetModuleHandleW(nullptr);
    WNDCLASSEXW wc={sizeof(wc)};wc.lpfnWndProc=windowProc;wc.hInstance=module;
    wc.lpszClassName=CLASS_NAME;wc.hCursor=LoadCursorW(nullptr,IDC_HAND);
    wc.hIcon=LoadIconW(module,MAKEINTRESOURCEW(1));wc.hIconSm=wc.hIcon;
    RegisterClassExW(&wc);
    p.load();
    DWORD exStyle=WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST;
#ifdef PET_UI_TEST
    // Only the UI test build exposes a taskbar window to automation tools.
    exStyle=WS_EX_LAYERED|WS_EX_APPWINDOW|WS_EX_TOPMOST;
#endif
    HWND hwnd=CreateWindowExW(exStyle,CLASS_NAME,L"奶螈桌宠 · 原生版",
        WS_POPUP,0,0,200,200,nullptr,nullptr,module,&p);
    if(!hwnd) throw std::runtime_error("create window");
    p.dpi(GetDpiForWindow(hwnd));
    p.motion.x=p.motion.work.right-250*p.motion.scale;p.motion.y=p.motion.work.bottom-250*p.motion.scale;
    p.render();
    if(show) {ShowWindow(hwnd,SW_SHOWNOACTIVATE);SetTimer(hwnd,1,16,nullptr);}
}
}

#ifndef PET_TEST
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    ULONG_PTR token;GdiplusStartupInput startup;
    if(GdiplusStartup(&token,&startup,nullptr)!=Ok) return 1;
    srand(static_cast<unsigned>(GetTickCount64()));
    timeBeginPeriod(1);
    int result=0;
    try {
        pet::Pet p;pet::create(p);
        MSG msg;
        while(GetMessageW(&msg,nullptr,0,0)>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    } catch(...) {MessageBoxW(nullptr,L"桌宠无法启动，请检查文件是否完整。",L"奶螈桌宠",MB_OK|MB_ICONERROR);result=1;}
    timeEndPeriod(1);GdiplusShutdown(token);CoUninitialize();return result;
}
#endif
