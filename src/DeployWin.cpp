
#include "wchar.h"
#include <string>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include "DeployWin.h"
#include "unistd.h"
#include "windows.h"
#include "psapi.h"
#include "ntdef.h"
#include "ntstatus.h"


/*
待复制的文件的队列
*/
static std::queue<std::string> dllqueue;
static std::mutex dllqueuelock;
static void dllqueueadd(std::string dllpath)
{
    std::lock_guard<std::mutex> lock(dllqueuelock);
    dllqueue.push(dllpath);
}
static bool dllqueueisempty()
{
    return dllqueue.empty();
}
static std::string dllqueueget()
{
    std::lock_guard<std::mutex> lock(dllqueuelock);
    std::string ret;
    if(!dllqueueisempty())
    {
        ret=dllqueue.front();
        dllqueue.pop();
    }
    return ret;
}

/*
是否运行标志
*/
static bool IsRunning=false;

/*
是否正在拷贝标志
*/
static bool IsCopying=true;

/*
获取环境变量函数
*/
static std::string GetEnv(std::string key)
{
    if(key.empty())
    {
        return std::string();
    }
    const char* value=getenv(key.c_str());
    if(value!=NULL)
    {
        return value;
    }
    else
    {
        return std::string();
    }
}

/*
获取当前进程目录
*/
static std::string GetCurrentProcessDir()
{
    char buff[MAX_PATH]= {0};
    GetModuleFileNameA(NULL,buff,MAX_PATH);
    std::string exepath(buff);
    for(size_t i=0; i<exepath.length(); i++)
    {
        if(exepath.c_str()[exepath.length()-1-i]=='\\')
        {
            return exepath.substr(0,exepath.length()-i);
        }
    }
    return std::string(".\\");
}

/*
获取系统目录
*/
static std::string GetSystemRoot()
{
    return GetEnv("SYSTEMROOT");
}


/*
注册加载dll回调
*/
typedef const UNICODE_STRING* PCUNICODE_STRING;

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
{
    ULONG Flags;                    //Reserved.
    PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
    PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA
{
    ULONG Flags;                    //Reserved.
    PCUNICODE_STRING FullDllName;   //The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;   //The base file name of the DLL module.
    PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;              //The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA
{
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef const LDR_DLL_NOTIFICATION_DATA * PCLDR_DLL_NOTIFICATION_DATA;

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

typedef  VOID CALLBACK LdrDllNotification(
    _In_     ULONG                       NotificationReason,
    _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
    _In_opt_ PVOID                       Context
);

typedef  LdrDllNotification * PLDR_DLL_NOTIFICATION_FUNCTION;

typedef NTSTATUS NTAPI (*LdrRegisterDllNotification_t)(
    _In_     ULONG                          Flags,
    _In_     PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
    _In_opt_ PVOID                          Context,
    _Out_    PVOID                          *Cookie
);

typedef NTSTATUS NTAPI (*LdrUnregisterDllNotification_t)(
    _In_ PVOID Cookie
);

static LdrRegisterDllNotification_t LdrRegisterDllNotification=NULL;
static LdrUnregisterDllNotification_t LdrUnregisterDllNotification=NULL;

static bool loadfunctions()
{
    //加载函数
    LdrRegisterDllNotification=NULL;
    LdrUnregisterDllNotification=NULL;

    HMODULE ntdll=LoadLibraryA("ntdll.dll");
    if(ntdll!=NULL)
    {
        LdrRegisterDllNotification=( LdrRegisterDllNotification_t)GetProcAddress(ntdll,"LdrRegisterDllNotification");
        LdrUnregisterDllNotification=(LdrUnregisterDllNotification_t)GetProcAddress(ntdll,"LdrUnregisterDllNotification");
    }

    if(LdrRegisterDllNotification==NULL || LdrUnregisterDllNotification==NULL)
    {
        return false;
    }
    else
    {
        return true;
    }
}


static VOID CALLBACK DeployWinLdrDllNotification(
    _In_     ULONG                       NotificationReason,
    _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
    _In_opt_ PVOID                       Context
)
{
    //dll通知回调
    switch(NotificationReason)
    {
    case LDR_DLL_NOTIFICATION_REASON_LOADED:
    {
        printf("DLL Loaded:%S\n",NotificationData->Loaded.FullDllName->Buffer);
        char buff[MAX_PATH+1]= {0};
        snprintf(buff,sizeof(buff)-1,"%S",NotificationData->Loaded.FullDllName->Buffer);
        dllqueueadd(buff);
    }
    break;
    case LDR_DLL_NOTIFICATION_REASON_UNLOADED:
    {
        printf("DLL Unloaded:%S\n",NotificationData->Loaded.FullDllName->Buffer);
    }
    break;

    default:
        break;
    }

}

/*
枚举已加载的模块
*/
static void enumloadeddll()
{
    HANDLE process=GetCurrentProcess();
    {
        //最大枚举4096个依赖
        HMODULE module_list[4096]= {0};
        DWORD count=0;
        if(EnumProcessModules(process,module_list,sizeof(module_list),&count))
        {
            for(size_t i=0; i<count/sizeof(module_list[0]); i++)
            {
                char buff[MAX_PATH+1]= {0};
                GetModuleFileNameA(module_list[i],buff,MAX_PATH);
                printf("DLL Loaded(Enum):%s\n",buff);
                dllqueueadd(buff);
            }
        }
    }

}

/*
启动与线程相关
*/
class deploywin_startup;
static void deploywin_thread(deploywin_startup * obj);

static class deploywin_startup
{
public:
    deploywin_startup()
    {
        Cookie=NULL;
        IsRunning=false;

        if(!loadfunctions())
        {
            return;
        }

        //当检测到环境变量DEPLOYWIN=1时启动DeployWin线程
        if(GetEnv("DEPLOYWIN")=="1")
        {
            printf("DeployWin is starting.\nTo avoid this,DO NOT SET DEPLOYWIN=1.\n");
            if(LdrRegisterDllNotification(0,DeployWinLdrDllNotification,this,&Cookie)!= STATUS_SUCCESS)
            {
                return;
            }
            IsRunning=true;
            std::thread(deploywin_thread,(this)).detach();

            printf("SystemDir:%s\n",GetSystemRoot().c_str());
            printf("Dir:%s\n",GetCurrentProcessDir().c_str());

            //枚举已加载的DLL
            enumloadeddll();

            {
                //等待当前队列的dll拷贝完成(主要是枚举的dll)
                //若main函数快速退出（常见于console程序），则不等待有可能拷贝不能完成。在main函数执行前拷贝完成防止拷贝中断。
                while(!dllqueueisempty() || IsCopying)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }

        }

    }
    ~deploywin_startup()
    {
        if(Cookie!=NULL)
        {
            LdrUnregisterDllNotification(Cookie);
        }
    }
private:
    PVOID Cookie;
} g_deploywin_startup;

static bool dllneedcopy(std::string dll)
{
    std::string sysroot=GetSystemRoot();
    std::string processdir=GetCurrentProcessDir();

    if(dll.empty())
    {
        return false;
    }

    if(0==strcasecmp(processdir.c_str(),dll.substr(0, processdir.length()).c_str()))
    {
        //排除当前目录下的dll
        return false;
    }

    if(0==strcasecmp(sysroot.c_str(),dll.substr(0,sysroot.length()).c_str()))
    {
        //排除系统目录下的dll
        return false;
    }

    if(0==strcasecmp(".exe",dll.substr(dll.length()-strlen(".exe")).c_str()))
    {
        //排除exe文件
        return false;
    }

    return true;
}
static bool dllcopy(std::string dll)
{
    if(dll.empty())
    {
        return false;
    }
    std::string processdir=GetCurrentProcessDir();
    if(processdir.at(processdir.length()-1)!='\\')
    {
        processdir+='\\';
    }
    std::string dllname;
    {
        //获取dll名称
        for(size_t i=0; i<dll.length(); i++)
        {
            if(dll.at(dll.length()-i-1)=='\\')
            {
                dllname=dll.substr(dll.length()-i);
                break;
            }
        }
        if(dllname.empty())
        {
            dllname=dll;
        }
    }
    std::string newdll=(processdir+dllname);
    return 0!=CopyFileA(dll.c_str(),newdll.c_str(),0);
}

static void deploywin_thread(deploywin_startup * obj)
{
    while(true)
    {
        while(!dllqueueisempty())
        {
            IsCopying=true;
            std::string dll=dllqueueget();
            if(dllneedcopy(dll))
            {
                printf("DLL(%s) need copy!\n",dll.c_str());
                if(dllcopy(dll))
                {
                    printf("DLL(%s) copy success!\n",dll.c_str());
                }
            }
            IsCopying=false;

        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/*
外部接口
*/

bool DeployWinIsRunning()
{
    return IsRunning;
}
