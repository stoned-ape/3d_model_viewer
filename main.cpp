#include "common.h"
#include "obj_file.h"
#include "las_file.h"
#include "quat.h"

#include <shobjidl.h>
#include <objbase.h>
#include <shlwapi.h>


void make_bmp(const wchar_t *file_name,int width,int height,void *pixels){
    __pragma(pack(push,1)) struct bmp_header{
        uint16_t magic; //0x4d42
        uint32_t file_size;
        uint32_t app; //0
        uint32_t offset; //54
        uint32_t info_size; //40
        int32_t width;
        int32_t height;
        uint16_t planes; //1
        uint16_t bits_per_pix; //32 (four bytes)
        uint32_t comp; //0
        uint32_t comp_size; //size in bytes of the pixel buffer (w*h*4)
        uint32_t xres; //0
        uint32_t yres; //0
        uint32_t cols_used; //0
        uint32_t imp_cols; //0
    }__pragma(pack(pop,1));

    static_assert(sizeof(bmp_header)==54,"");

    bmp_header head={0};
    head.magic=0x4d42;
    head.app=0;
    head.offset=sizeof(bmp_header);
    head.info_size=40;
    head.width=width;
    head.height=height;
    head.planes=1;
    head.bits_per_pix=32;
    head.comp_size=width*height*head.bits_per_pix/8;
    head.file_size=sizeof(bmp_header)+head.comp_size;

    auto ptr=(uint8_t*)pixels;
    for(int i=0;i<width*height;i++){
        uint8_t tmp=ptr[4*i+0];
        ptr[4*i+0]=ptr[4*i+2];
        ptr[4*i+2]=tmp;
    }

    HANDLE bmp=CreateFileW(file_name,GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);
    assert(bmp!=INVALID_HANDLE_VALUE);
    unsigned long m;
    assert(WriteFile(bmp,&head,sizeof head,&m,0));
    assert(m==sizeof(bmp_header));
    assert(WriteFile(bmp,pixels,head.comp_size,&m,0));
    assert(m==head.comp_size);
    CloseHandle(bmp);
}


struct dialog_event_handler:IFileDialogEvents,IFileDialogControlEvents{
    // IUnknown methods
    IFACEMETHODIMP QueryInterface(REFIID riid,void **ppv){
        static const QITAB qit[]={
            QITABENT(dialog_event_handler,IFileDialogEvents),
            QITABENT(dialog_event_handler,IFileDialogControlEvents),
            {0},
        };
        return QISearch(this,qit,riid,ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef(){
        return InterlockedIncrement(&_cref);
    }
    IFACEMETHODIMP_(ULONG) Release(){
        long cref=InterlockedDecrement(&_cref);
        if(!cref) delete this;
        return cref;
    }
    // IFileDialogEvents methods
    IFACEMETHODIMP OnFileOk         (IFileDialog*){return S_OK;}
    IFACEMETHODIMP OnFolderChange   (IFileDialog*){return S_OK;}
    IFACEMETHODIMP OnHelp           (IFileDialog*){return S_OK;}
    IFACEMETHODIMP OnSelectionChange(IFileDialog*){return S_OK;}
    IFACEMETHODIMP OnTypeChange     (IFileDialog*){return S_OK;}
    IFACEMETHODIMP OnFolderChanging (IFileDialog*,IShellItem*){return S_OK;}
    IFACEMETHODIMP OnOverwrite      (IFileDialog*,IShellItem*,FDE_OVERWRITE_RESPONSE*)     {return S_OK;}
    IFACEMETHODIMP OnShareViolation (IFileDialog*,IShellItem*,FDE_SHAREVIOLATION_RESPONSE*){return S_OK;}
    // IFileDialogControlEvents methods
    IFACEMETHODIMP OnButtonClicked     (IFileDialogCustomize*,DWORD)      {return S_OK;}
    IFACEMETHODIMP OnControlActivating (IFileDialogCustomize*,DWORD)      {return S_OK;}
    IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*,DWORD,BOOL) {return S_OK;}
    IFACEMETHODIMP OnItemSelected      (IFileDialogCustomize*,DWORD,DWORD){return S_OK;}
    dialog_event_handler():_cref(1){};
    ~dialog_event_handler(){};
    long _cref;
};





const wchar_t *get_file_from_user(){
    HRESULT hr;
    IFileDialog *pfd=NULL;
    IFileDialogEvents *pfde=NULL;
    DWORD dwFlags;
    const COMDLG_FILTERSPEC c_rgSaveTypes[]={{L"OBJ Files (*.obj)",L"*.obj"},{L"LAS Files (*.las)",L"*.las"}};
    IShellItem *psiResult;
    wchar_t *file_path=NULL;
    // static char path[MAX_PATH];
    dialog_event_handler *pDialogEventHandler=NULL;

    hr= CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    assert(SUCCEEDED(hr));
    if (!SUCCEEDED(hr)) goto rel5;

    pDialogEventHandler=new dialog_event_handler();
    assert(pDialogEventHandler);
    hr=pDialogEventHandler->QueryInterface(IID_PPV_ARGS(&pfde));
    pDialogEventHandler->Release();
    pDialogEventHandler=NULL;


    if (!SUCCEEDED(hr)) goto rel4;
    DWORD cookie;
    hr = pfd->Advise(pfde, &cookie);
    if (!SUCCEEDED(hr)) goto rel4;
    hr = pfd->GetOptions(&dwFlags);
    if (!SUCCEEDED(hr)) goto rel2;
    hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
    if (!SUCCEEDED(hr)) goto rel2;
    hr = pfd->SetFileTypes(ARRAYSIZE(c_rgSaveTypes), c_rgSaveTypes);
    if (!SUCCEEDED(hr)) goto rel2;
    hr = pfd->Show(NULL);
    if (!SUCCEEDED(hr)) goto rel2;
    hr = pfd->GetResult(&psiResult);
    if (!SUCCEEDED(hr)) goto rel1;
    hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
    if (!SUCCEEDED(hr)) goto rel1;
rel1:
    psiResult->Release();
rel2:
    pfd->Unadvise(cookie);
rel3:
    pfde->Release();
rel4:
    pfd->Release();
rel5:
    return file_path;
}




void draw_float3(float3 v){
    glBegin(GL_LINES);
    //glColor3f(1,0,1);
    glVertex3f(0,0,0);
    glVertex3f(v.x,v.y,v.z);
    glEnd();
}


int label_printf(HWND l,const char *fmt,...){
    static char buf[128];
    va_list args; 
    va_start(args,fmt);
    int res=vsprintf(buf,fmt,args);
    SetWindowTextA(l,buf);
    va_end(args);
    return res;
}


struct state_t{
    bool left_mouse_down;
    bool right_mouse_down;
    float2 mouse_pos;
    float3 base_mouse_vec;
    float3 mouse_vec;
    float3 pan_base_mouse_vec;
    float3 pan_mouse_vec;
    _quat cam_q;
    float zoom;
    geo_mode_e gm;
    col_mode_e cm;
    obj_file of;
    las_file lf;
    int ps;
    int lw;
    HWND hwnd;
    HWND ps_hwnd;
    HWND lw_hwnd;
    bool is_obj;
    float4x4 mvm;
    float4x4 inv;
    float3 origin;
    bool will_save_bmp;
    bool ortho;
}state={
    .left_mouse_down=0,
    .right_mouse_down=false,
    .base_mouse_vec=float3(0,0,1),
    .mouse_vec=float3(0,0,1),
    .cam_q=_quat{1,float3(0)},
    .zoom=1,
    .gm=GM_FACES,
    .cm=CM_TEXTS,
    .of={0},
    .ps=1,
    .lw=1,
    .origin=float3(0),
    .will_save_bmp=false,
    .ortho=false,
};

bool load_new_file(bool first,const wchar_t *fname){
    bool dealloc=false;
    if(fname==NULL){ 
        fname=get_file_from_user();
        dealloc=true;
    }
    if(fname==NULL) return false;
    if(!first){
        if(state.is_obj) destroy_obj_file(&state.of);
        else free(state.lf.points);
    } 
    const wchar_t *ext=fname+wcslen(fname)-3;
    if(0==wcscmp(ext,L"obj")) state.is_obj=1;
    else if(0==wcscmp(ext,L"las")) state.is_obj=0;
    else assert(0);
    if(state.is_obj) create_obj_file(&state.of,fname);
    else             create_las_file(&state.lf,fname);
    SetWindowTextW(state.hwnd,fname);
    if(dealloc) CoTaskMemFree((void*)fname);
    return true;
}


LRESULT CALLBACK WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam){
    switch(message){
        case WM_LBUTTONDOWN:{
            state.left_mouse_down=1;
            state.base_mouse_vec=state.mouse_vec;
            break;
        }
        case WM_LBUTTONUP:{
            state.left_mouse_down=0;
            _quat tmp_q=from_to(state.base_mouse_vec,state.mouse_vec);
            state.cam_q=tmp_q*state.cam_q;
            state.base_mouse_vec=float3(0);
            break;
        }
        case WM_RBUTTONDOWN:{
            state.right_mouse_down=1;
            state.pan_base_mouse_vec=state.pan_mouse_vec;
            break;
        }
        case WM_RBUTTONUP:{
            state.right_mouse_down=0;
            //_quat tmp_q=from_to(state.base_mouse_vec,state.mouse_vec);
            //state.cam_q=tmp_q*state.cam_q;
            state.origin+=state.pan_mouse_vec-state.pan_base_mouse_vec;
            state.pan_base_mouse_vec=float3(0);
            break;
        }
        case WM_COMMAND:{
            int idx=LOWORD(wparam);
            switch(idx){
            case 0: state.gm=GM_VERTS;break;
            case 1: state.gm=GM_EDGES;break;
            case 2: state.gm=GM_FACES;break;
            case 3: state.cm=CM_NORMS;break;
            case 4: state.cm=CM_TXCDS;break;
            case 5: state.cm=CM_TEXTS;break;
            case 6: load_new_file(false,NULL);break;
            case 7: 
                state.ps=glm::clamp(state.ps+1,1,10);
                label_printf(state.ps_hwnd,"   %d",state.ps);
                break;
            case 8: 
                state.ps=glm::clamp(state.ps-1,1,10);
                label_printf(state.ps_hwnd,"   %d",state.ps);
                break;
            case 9: 
                state.lw=glm::clamp(state.lw+1,1,10);
                label_printf(state.lw_hwnd,"   %d",state.lw);
                break;
            case 10: 
                state.lw=glm::clamp(state.lw-1,1,10);
                label_printf(state.lw_hwnd,"   %d",state.lw);
                break;
            case 11: state.origin+=m4v3(state.inv,float3(0,+.1,0),0);break;
            case 12: state.origin+=m4v3(state.inv,float3(-.1,0,0),0);break;
            case 13: state.origin+=m4v3(state.inv,float3(+.1,0,0),0);break;
            case 14: state.origin+=m4v3(state.inv,float3(0,-.1,0),0);break;
            case 15: state.origin=float3(0,0,0);break;
            case 16: state.will_save_bmp=true;break;
            case 17: state.ortho=true;break;
            case 18: state.ortho=false;break;
            }
            break;
        }
        case WM_MOUSEWHEEL:{
            short delta=GET_WHEEL_DELTA_WPARAM(wparam);
            float off=-.001;
            float dz=1-delta*off;
            state.zoom*=dz;
            state.zoom=glm::clamp(state.zoom,.5f,32.0f);
            break;
        }
        case WM_CLOSE: 
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd,message,wparam,lparam);
}


int WINAPI WinMain(HINSTANCE hi,HINSTANCE hpi,char *args,int winshow){
    // if(!AttachConsole(-1)) AllocConsole();
    if(AttachConsole(-1))
    {
		freopen("CONIN$", "r",stdin);
		freopen("CONOUT$", "w",stdout);
		freopen("CONOUT$", "w",stderr);
		puts("stdout");
		fputs("stderr\n",stderr);
	}

    static char cwd[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH,cwd);
    puts(cwd);

    HRESULT hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    assert(SUCCEEDED(hr));

    WNDCLASSA wndclass={0};
    wndclass.style        =CS_OWNDC;
    wndclass.lpfnWndProc  =WndProc;
    wndclass.hInstance    =GetModuleHandle(NULL);
    wndclass.hCursor      =LoadCursor(NULL,IDC_ARROW);
    wndclass.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
    wndclass.lpszClassName="window_class";

    float width=1280;//640;
    float height=720;//480;
    const float gui_height=200;


    WIN32_CHECK(RegisterClassA(&wndclass));
    HWND hwnd=CreateWindowA(
        "window_class",
        "._____.",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height+gui_height,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    assert(hwnd);
    state.hwnd=hwnd;

    HWND gl_hwnd=CreateWindowA( "window_class",NULL,WS_CHILD,0,0,width,height,
                                hwnd,NULL,GetModuleHandle(NULL),NULL);
    assert(gl_hwnd);
    ShowWindow(gl_hwnd,SW_NORMAL);


    uint64_t button_id=0;
    HWND geo_mode_hwnd=CreateWindowA(
        "window_class",       
        NULL,
        WS_CHILD,
        0,height,130, 120,
        hwnd,             
        NULL,             
        GetModuleHandle(NULL),
        NULL
    );

    assert(geo_mode_hwnd);
    ShowWindow(geo_mode_hwnd,SW_NORMAL);

    const char *geo_mode_names[3]={"draw vertecies","draw edges","draw faces"};

    for(int i=0;i<3;i++){
        CreateWindowExA(
            0,"BUTTON",geo_mode_names[i],
            WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
            10, 10+i*40, 120, 30,
            geo_mode_hwnd,
            (HMENU)(button_id++),
            GetModuleHandle(NULL),
            NULL
        );
    }


    HWND col_mode_hwnd=CreateWindowA(
        "window_class",       
        NULL,
        WS_CHILD,
        130,height,200, 200,
        hwnd,             
        NULL,             
        GetModuleHandle(NULL),
        NULL
    );

    assert(col_mode_hwnd);
    ShowWindow(col_mode_hwnd,SW_NORMAL);

    const char *col_mode_names[3]={"color normals","color texture coordinates","color textures"};

    for(int i=0;i<3;i++){
        CreateWindowExA(
            0,"BUTTON",col_mode_names[i],
            WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
            10, 10+i*40, 200, 30,
            col_mode_hwnd,
            (HMENU)(button_id++),
            GetModuleHandle(NULL),
            NULL
        );
    }

    CreateWindowExA(
        0,"BUTTON","open file",WS_CHILD|WS_VISIBLE,
        340,height+10, 200, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL
    );

    const struct{
        const char *label;
        HWND *hwnd;
        int val;
    }data[2]={
        {
            .label=" point size",
            .hwnd=&state.ps_hwnd,
            .val=state.ps,
        },{
            .label=" line width",
            .hwnd=&state.lw_hwnd,
            .val=state.lw,
        },
    };

    for(int i=0;i<2;i++){
        HWND incr_hwnd=CreateWindowA(
            "window_class",NULL,WS_CHILD,
            340,height+10+40+40*i, 200, 30,
            hwnd,NULL,GetModuleHandle(NULL),NULL
        );
        assert(incr_hwnd);
        ShowWindow(incr_hwnd,SW_NORMAL);

        CreateWindowExA(
            0,"BUTTON","+",WS_CHILD | WS_VISIBLE,
            0,0, 33, 30,          
            incr_hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
        );

        *data[i].hwnd=CreateWindowExA(
            0,"STATIC"," ", WS_VISIBLE|WS_CHILD,
            33,0,33,30,
            incr_hwnd,NULL,GetModuleHandle(NULL),NULL 
        );
        label_printf(*data[i].hwnd,"   %d",data[i].val);

        CreateWindowExA(
            0,"BUTTON","-",WS_CHILD | WS_VISIBLE,
            66,0,33,30,          
            incr_hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
        );

        HWND label=CreateWindowExA(
            0,"STATIC"," ", WS_VISIBLE|WS_CHILD,
            100,0,100,30,
            incr_hwnd,NULL,GetModuleHandle(NULL),NULL 
        );
        SetWindowTextA(label,data[i].label);

    }

    const float mv_ctrl_w=200;

    CreateWindowExA(
        0,"BUTTON","up",WS_CHILD | WS_VISIBLE,
        550+mv_ctrl_w/3,height+10, mv_ctrl_w/3, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
    );

    CreateWindowExA(
        0,"BUTTON","left",WS_CHILD | WS_VISIBLE,
        550,height+10+40, mv_ctrl_w/3, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
    );

    CreateWindowExA(
        0,"BUTTON","right",WS_CHILD | WS_VISIBLE,
        550+2*mv_ctrl_w/3,height+10+40, mv_ctrl_w/3, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
    );

    CreateWindowExA(
        0,"BUTTON","down",WS_CHILD | WS_VISIBLE,
        550+mv_ctrl_w/3,height+10+80, mv_ctrl_w/3, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
    );

    CreateWindowExA(
        0,"BUTTON","reset",WS_CHILD | WS_VISIBLE,
        550+mv_ctrl_w/3,height+10+40, mv_ctrl_w/3, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL                        
    );

    CreateWindowExA(
        0,"BUTTON","take picture",WS_CHILD|WS_VISIBLE,
        760,height+10, 200, 30,          
        hwnd,(HMENU)(button_id++),GetModuleHandle(NULL),NULL
    );

    for(int i=1;i<3;i++){
        const char *mode_names[2]={"orthographic","perspective"};
        CreateWindowExA(
            0,"BUTTON",mode_names[i-1],
            WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
            760, height+10+i*40, 200, 30,
            hwnd,
            (HMENU)(button_id++),
            GetModuleHandle(NULL),
            NULL
        );
    }

    

    PIXELFORMATDESCRIPTOR pfd={0};
    pfd.nSize=sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;
    pfd.cColorBits=32;
    pfd.cDepthBits=24;
    pfd.cStencilBits=8;
    pfd.cAuxBuffers=0;
    pfd.iLayerType=PFD_MAIN_PLANE;

    HDC hdc=GetDC(gl_hwnd);
    int pf=ChoosePixelFormat(hdc,&pfd);
    assert(pf);
    WIN32_CHECK(SetPixelFormat(hdc,pf,&pfd));
    HGLRC ctx=wglCreateContext(hdc);
    assert(ctx);
    WIN32_CHECK(wglMakeCurrent(hdc,ctx));
    puts((const char*)glGetString(GL_VERSION));
    glEnable(GL_DEPTH_TEST);

    
    // create_obj_file(&state.of,L"objs\\plane1\\11803_Airplane_v1_l1.obj");
    // create_obj_file(&state.of,L"objs\\cat\\12221_Cat_v1_l3.obj");
    // create_obj_file(&state.of,L"objs\\color_foot\\color_foot.obj");

    if(!load_new_file(true,L"obj_files\\doge\\untitled.obj")) return 0;
    ShowWindow(hwnd,SW_NORMAL);
    

    bool running=true;
    while(running){
        MSG msg;
        // while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){
        GetMessageA(&msg,0,0,0);{
            if(msg.message==WM_QUIT){
                running=false;
            }else{
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        if(!running) break;

        // bool ortho=0;

        POINT mouse_pos;
        GetCursorPos(&mouse_pos);
        RECT orect,irect;
        GetWindowRect(gl_hwnd,&irect);
        GetWindowRect(hwnd,&orect);
        width =irect.right -irect.left;
        height=irect.bottom-irect.top;
        long xpos=mouse_pos.x-irect.left;
        long ypos=mouse_pos.y-irect.top;
        const float aspect=height/(float)width;
        state.mouse_vec=float3( map((float)xpos,0,width ,-1,+1),
                                map((float)ypos,0,height,+1,-1),state.ortho?-.1:-1);
        state.pan_mouse_vec=state.mouse_vec;

        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glViewport(0,0,width,height);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();


        glPointSize(state.ps);
        glLineWidth(state.lw);

        float hfov;
        float vfov;
        if(!state.ortho){
            glMatrixMode(GL_PROJECTION);
            float sc=.1;
            float n=3;
            float r=1/aspect;
            float t=1;
            hfov=2*atan2(r,n);
            vfov=2*atan2(t,n);
            glFrustum(-r*sc,r*sc,-t*sc,t*sc,n*sc,10);
            glMatrixMode(GL_MODELVIEW);

            glTranslatef(0,0,-2);
        }else{
            glScalef(1,1,.1);
            glScalef(aspect,1,-1);
        }



        float4x4 mvm;
        glGetFloatv(GL_MODELVIEW_MATRIX,&mvm[0][0]);
        mvm=glm::inverse(mvm);

        float4x4 prm;
        glGetFloatv(GL_PROJECTION_MATRIX,&prm[0][0]);
        prm=glm::inverse(prm);

        if(state.ortho){
            state.mouse_vec=m4v3(mvm,state.mouse_vec,1);
        }else{
            float4 a=(mvm*prm)*float4(state.mouse_vec,1);
            state.mouse_vec=a.xyz()/a.w;
            state.mouse_vec+=3.0f*(state.mouse_vec-float3(0,0,2));
        }



        //glColor3f(1,0,1);
        // draw_float3(state.mouse_vec);
        // draw_float3(state.base_mouse_vec);

        


        _quat q=state.cam_q;
               
        if(state.left_mouse_down){
            _quat tmp_q=from_to(state.base_mouse_vec,state.mouse_vec);
            q=tmp_q*state.cam_q;
        }

        glRotate(q);


        glScalef(state.zoom,state.zoom,state.zoom);

	    glGetFloatv(GL_MODELVIEW_MATRIX,&state.mvm[0][0]);
        state.inv=glm::inverse(state.mvm);

        state.pan_mouse_vec=m4v3(state.inv,state.pan_mouse_vec,1);

        // glColor3f(1,0,1);
        // draw_float3(state.pan_mouse_vec);
        // draw_float3(state.pan_base_mouse_vec);

        float3 panned_origin=state.origin;
               
        if(state.right_mouse_down){
            panned_origin+=state.pan_mouse_vec-state.pan_base_mouse_vec;
        }
        
        glTranslate(panned_origin);


        if(state.is_obj){
            float s=0;
            for(int i=0;i<3;i++){
                s=glm::max(s,glm::abs(state.of.box[1][i]-state.of.box[0][i]));
            }
            glPushMatrix();
            glTranslate(-(state.of.box[0]+state.of.box[1])/2.0f/s);
            glScalef(1/s,1/s,1/s);
            glColor3f(0,1,1);
            draw_obj_file(&state.of,state.gm,state.cm);


            glGetFloatv(GL_MODELVIEW_MATRIX,&mvm[0][0]);
            glGetFloatv(GL_PROJECTION_MATRIX,&prm[0][0]);

            glPopMatrix();
        }else{
            glPushMatrix();
            const float3 c=(state.lf.box[1]+state.lf.box[0])/2.0f;
            const float3 s=(state.lf.box[1]-state.lf.box[0]);
            glTranslate(-c/s);
            glScale(1.0f/s);
            glBegin(GL_POINTS);
            for(int i=0;i<state.lf.num_points;i++){
                glColor(state.lf.points[i].col);
                glVertex<3>(state.lf.points[i].v);
            }
            glEnd();
            glPopMatrix();
        } 

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        if(state.will_save_bmp){
            void *ptr=malloc(width*height*4);
            assert(ptr);
            
            glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,ptr);

            // static wchar_t buf[MAX_PATH];
            // swprintf(buf,MAX_PATH,L"%s\\render%d.bmp",state.save_dir,i);

            make_bmp(L"render.bmp",width,height,ptr);

            FILE *f=fopen("render_params.txt","w");
            assert(f);
            // FILE *tmp=stdout;
            // stdout=f;

            global_file_star=f;
            lprintln(width);
            lprintln(height);
            lprintln(hfov*180/pi);
            lprintln(vfov*180/pi);
            println();
            println("projection_matrix:");
            println(prm);
            println("model_view_matrix:");
            println(mvm);
            global_file_star=stdout;



            // stdout=tmp;
            fclose(f);

            free(ptr);

            state.will_save_bmp=false;
        }

        // glPopMatrix();
        GL_CHECK();
        SwapBuffers(hdc);
    }    
}