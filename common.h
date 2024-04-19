#pragma once
#define  _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <sys/timeb.h>
#include <stdarg.h>

#include <windows.h>
#include <psapi.h>
#include <gl/gl.h>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_CXX17
#define GLM_FORCE_AVX2
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp> 

#include <vector>

#pragma comment(lib,"kernel32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"netapi32.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"propsys.lib")
#pragma comment(lib,"shlwapi.lib") 

#define PRINT(val,type) printf("%s:\t" type "\n",#val,val);

const float pi=3.141592653589793;

typedef glm::vec2 float2;
typedef glm::vec3 float3;
typedef glm::vec4 float4;
typedef glm::ivec2 int2;
typedef glm::ivec3 int3;
typedef glm::ivec4 int4;
typedef glm::mat2 float2x2;
typedef glm::mat3 float3x3;
typedef glm::mat4 float4x4;

typedef glm::mat2x3 float2x3;
typedef glm::mat2x4 float2x4;

template<typename T> static int print(T arg);
template<typename T,typename ...TV>
int print(T arg,TV ...vargs){
    int x=print<T>(arg);
    return x+print(vargs...);
}
#define PRINT_BASIC_RAW(type,format) \
    template<> int print<type>(type arg){return printf(format,arg);}
#define PRINT_BASIC(type,format) \
    PRINT_BASIC_RAW(type,format) \
    PRINT_BASIC_RAW(type*,"%p ") \
    PRINT_BASIC_RAW(const type*,"%p ")

PRINT_BASIC_RAW(char,"%c");
PRINT_BASIC_RAW(char*,"%s");
PRINT_BASIC_RAW(void*,"%p ");
PRINT_BASIC_RAW(const char*,"%s");
PRINT_BASIC_RAW(const void*,"%p ");
PRINT_BASIC(bool,"%d ");
PRINT_BASIC(short,"%d ");
PRINT_BASIC(int,"%d ");
PRINT_BASIC(long,"%ld ");
PRINT_BASIC(long long,"%lld ");
PRINT_BASIC(unsigned char,"%u ");
PRINT_BASIC(unsigned short,"%u ");
PRINT_BASIC(unsigned int,"%u ");
PRINT_BASIC(unsigned long,"%lu ");
PRINT_BASIC(unsigned long long,"%llu ");
PRINT_BASIC(float,"%f ");
PRINT_BASIC(double,"%f ");
PRINT_BASIC(long double,"%Lf ");

template<> int print<const wchar_t*>(const wchar_t* arg){return wprintf(L"%s",arg);}
template<> int print<wchar_t*>(wchar_t* arg){return wprintf(L"%s",arg);}

#undef PRINT_BASIC
#undef PRINT_BASIC_RAW
static int println(){return print('\n');}
template<typename ...TV>
static int println(TV ...vargs){return print(vargs...,'\n');}
#define lprintln(...) println(#__VA_ARGS__,":\t",__VA_ARGS__)


template<> int print<float2>(float2 v){return print('[',v.x,',',v.y,']');}
template<> int print<float3>(float3 v){return print('[',v.x,',',v.y,',',v.z,']');}
template<> int print<float4>(float4 v){return print('[',v.x,',',v.y,',',v.z,',',v.w,']');}

template<> int print<int2>(int2 v){return print('[',v.x,',',v.y,']');}
template<> int print<int3>(int3 v){return print('[',v.x,',',v.y,',',v.z,']');}
template<> int print<int4>(int4 v){return print('[',v.x,',',v.y,',',v.z,',',v.w,']');}

template<> int print<float2x2>(float2x2 m){
    float2 *cols=(float2*)&m;
    int x=0;
    x+=println('[',cols[0],',');
    x+=println(' ',cols[1],']');
    return x;
}

template<> int print<float3x3>(float3x3 m){
    float3 *cols=(float3*)&m;
    int x=0;
    x+=println('[',cols[0],',');
    x+=println(' ',cols[1],',');
    x+=println(' ',cols[2],']');
    return x;
}

static_assert(sizeof(float3)==4*4);

template<> int print<float4x4>(float4x4 m){
    float4 *cols=(float4*)&m;
    int x=0;
    x+=println('[',cols[0],',');
    x+=println(' ',cols[1],',');
    x+=println(' ',cols[2],']');
    x+=println(' ',cols[3],']');
    return x;
}

template<uint32_t n>
static void glVertex(glm::vec<n,float,glm::defaultp> v);

template<> void glVertex<2>(float2 v){glVertex2fv((float*)&v);}
template<> void glVertex<3>(float3 v){glVertex3fv((float*)&v);}
template<> void glVertex<4>(float4 v){glVertex4fv((float*)&v);}

inline static void glColor(float3 c){glColor3f(c.r,c.g,c.b);}
inline static void glTranslate(float3 c){glTranslatef(c.r,c.g,c.b);}
inline static void glScale(float3 c){glScalef(c.r,c.g,c.b);}

inline static float map(float t,float t0,float t1,float s0,float s1){
    return s0+(s1-s0)*(t-t0)/(t1-t0);
}

inline static float deg(float rad){return rad*180/pi;}

inline static float pos(bool p){return p?1.0f:-1.0f;}

inline static float3 m4v3(float4x4 mtx,float3 v,bool translate){return (mtx*float4(v.x,v.y,v.z,(float)translate)).xyz();}

static inline void gl_check(const char *file,int line){
    if(auto glerr=glGetError();glerr!=GL_NO_ERROR){
        const char *ename=NULL;
        #define CASE(x) case x:{ename=#x;break;}
        switch(glerr){
        CASE(GL_NO_ERROR);
        CASE(GL_INVALID_ENUM);
        CASE(GL_INVALID_VALUE);
        CASE(GL_INVALID_OPERATION);
        CASE(GL_STACK_OVERFLOW);
        CASE(GL_STACK_UNDERFLOW);
        CASE(GL_OUT_OF_MEMORY);
        }
        #undef CASE
        fprintf(stderr,"GL_CHECK() failed: error 0x%x (%s) on line %d of file %s\n",glerr,ename,line,file);
        exit(1);
    }
}

#define GL_CHECK() gl_check(__FILE__,__LINE__)

static BOOL win32_check(BOOL ret,const char *ret_str,const char *file,int line,BOOL exit){
    if(!ret){
        int error=GetLastError();
        assert(error);
        static char msg[128];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,error,0,msg,sizeof(msg),NULL
        );
        fprintf(
            stderr,"WIN32_CHECK(%s) failed: file %s line %d errno %d msg: %s\n",
            ret_str,file,line,error,msg
        );
        
        if(exit) assert(0);// ExitProcess(1);
        LocalFree(msg);
    }
    return ret;
}

#define WIN32_CHECK(call) win32_check(call,#call,__FILE__,__LINE__,1)
#define WIN32_CHECK_NOEXIT(call) win32_check(call,#call,__FILE__,__LINE__,0)

#define EXPORT extern "C" __declspec(dllexport)

static inline double itime(){
    struct timeb now;
    ftime(&now);
    return (double)(now.time%(60*60*24))+now.millitm/1e3;
}

static inline void print_fps(){
    static double timer;
    double delta=itime()-timer;
    timer+=delta;
    printf("\rfps = %f ",1/delta);
    fflush(stdout);
}

static unsigned long get_proc_mem(){
    PROCESS_MEMORY_COUNTERS pmc={0};
    pmc.cb=sizeof(PROCESS_MEMORY_COUNTERS);
    GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof(pmc));
    return pmc.WorkingSetSize;
}

