#include "MozAdhoc.h"
#include "mozilla/dom/MozAdhocBinding.h"
# include "nsXPCOMCIDInternal.h"
# include "nsIThreadManager.h"
# include "nsServiceManagerUtils.h"

#include "nsXULAppAPI.h"
#include <android/log.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
 
#define ADHOCLOG(fmt, ...) __android_log_print(ANDROID_LOG_WARN, "MozAdhoc", "%12s:%-5d,%s ," fmt,  __FILE__, __LINE__,(char*)__FUNCTION__, ##__VA_ARGS__)



//定义函数指针
typedef int (*CalculatorFuncPointer_init)(char *paras);
typedef int (*CalculatorFuncPointer)(int, int);

//typedef (int *)(DataRecvCallback)(char *srcAddr, unsigned char *pData);
/*
 *typedef int (*DataRecvCallback)(char *srcAddr, unsigned char *pData);
 *typedef int (*NetWorkStatusCallback)(int statusType, char *pParam);
 *typedef int (*PcmVoiceCallback)(int statusType, char *pParam);
 *
 *typedef int (*RegCallBackFuncPointer_data)(DataRecvCallback acall);
 *typedef int (*RegCallBackFuncPointer_network)(NetWorkStatusCallback acall);
 *typedef int (*RegCallBackFuncPointer_voice)(PcmVoiceCallback acall);
 *
 */


typedef void (*DataRecvCallback)(char *srcAddr, unsigned char *pData);
typedef void (*NetWorkStatusCallback)(int statusType, char *pParam);
typedef void (*PcmVoiceCallback)(int statusType, char *pParam);

typedef void (*RegCallBackFuncPointer_data)(DataRecvCallback acall);
typedef void (*RegCallBackFuncPointer_network)(NetWorkStatusCallback acall);
typedef void (*RegCallBackFuncPointer_voice)(PcmVoiceCallback acall);

//using namespace std;
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::adhoc;

mozilla::dom::DataCallback* aDataCallback = nullptr;
mozilla::dom::NetWorkCallback* aNetWorkCallback = nullptr;
mozilla::dom::PcmCallback* aPcmCallback = nullptr;
MozAdhoc* MozAdhoc::sAdhocInstance = NULL;

/*
 *static int OnDataRecvCallback(char *srcAddr, unsigned char *pData);
 *static int OnNetWorkStatusCallback(int statusType, char *pParam);
 *static int OnPcmVoiceCallback(int statusType, char *pParam);
 *
 */
  
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MozAdhoc)
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(MozAdhoc)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MozAdhoc)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MozAdhoc, mWindow)

MozAdhoc::MozAdhoc(nsPIDOMWindow* aWindow)
: mWindow(aWindow)
{
    ADHOCLOG("MozAdhoc enter MozAdhoc\n");
    sAdhocInstance = this;
    return;
}

MozAdhoc::~MozAdhoc()
{
    //ADHOCLOG("MozAdhoc enter ~MozAdhoc\n");
    sAdhocInstance = NULL;
    return;
}


///////所有接口如下:
  /*
   *boolean isEnabled();//查询数据传输模式 
   *boolean isNetWorkAvailable();//网络是否可用
   *void openAdHocDevice();//打开自组网设备 0:打开成功, 非0:打开失败
   *void closeAdHocDevice();//关闭自组网设备
   *void reOpenAdHocDevice();//重打开自组网设备
   */

  //void init(DOMString paras);//初始化自组网
  //void sendData( DOMString data);//发送数据
  /*void setEnabled (boolean enabled);//设置数据传输模式，ip或非ip true :非ip模式，false :ip模式*/

  /**
    @param srcAddr 源ip
    @param desAddr 目的ip
    @param data 数据内容
   **/
  
  /*unsigned long 可用*/


  /*
   *void tellLocalIpAddr(DOMString ipAddr);//设置本机自组网ip
   *void queryNetWorkStatus(DOMString cmd);//查询网络状态
   *char* getEthernetIP();//查询以太网ip
   *
   *void sendData(DOMString srcAddr, DOMString desAddr, DOMString data);//发送数据
   *void sendDataPri(DOMString srcAddr,  DOMString desAddr,  DOMString data, DOMString pri);//发送数据
   *void sendPcmVoice(DOMString voiceData);//发送话音数据
   *void sendBDMsg2AdHoc(DOMString  gga);//发送北斗消息到自组网
   *void sendExtraData (DOMString type, DOMString data);//执行shell命令
   */
  
  //void setParameters(DOMString paras);//设置自组网参数
  /*
   *void setPttState (DOMString state);//设置ptt状态
   *void setIsNeedHeadForUserData(boolean isNeedHead);//设置用户数据是否需要头部
   *void setEthernetIP(DOMString ipAddr);//设置以太网ip
   *void updateModem(DOMString fileName, DOMString data);//升级modem
   */
  
  /*
   *ConfigParams getConfigParams();//获取平台信息
   */

  /*
   *void addDataRecvListener(DataaCallback successCallback,optional AdhocCommandData command);//添加接收数据回调监听器    
   * *boolean addDataRecvListener(DataRecvCallback *pFunc);//添加接收数据回调监听器    
   * *boolean addNetWorkStatusListener(NetWorkStatusCallback *pFunc);//添加网络状态回调监听器
   * *boolean addPcmVoiceListener(PcmVoiceCallback *pFunc);//添加接收话音回调监听器
   */

//动态库地址
const char *dlib_path = "/system/lib/libadhocyaya.so";
 

static 
void
OnDataRecvCallback(char *srcAddr, unsigned char *pData)
{
    //this callback will called by so func 
    ADHOCLOG("###########callback connect result: %s %s,###########",srcAddr,pData);

    nsAutoString addr, data;
    addr.AssignWithConversion(srcAddr);
    data.AssignWithConversion((const char *)pData);
    MozAdhoc::GetAdhocInstance()->CallbackJsForData(addr, data);

}



static 
void
OnNetWorkStatusCallback(int statusType, char *pParam)
{
    //this callback will called by so func 
    ADHOCLOG("###########callback connect###########");
    nsAutoString param;
    param.AssignWithConversion((const char *)pParam);
    MozAdhoc::GetAdhocInstance()->CallbackJsForNet(statusType, param);
}

static 
void
OnPcmVoiceCallback(int statusType, char *pParam)
{
    //this callback will called by so func 
    ADHOCLOG("###########callback connect###########");
    nsAutoString param;
    param.AssignWithConversion((const char *)pParam);
    MozAdhoc::GetAdhocInstance()->CallbackJsForPcm(statusType, param);
}

    

void
//MozAdhoc::RegisterCallBackListener( char* RegFUncName )
MozAdhoc::RegisterCallBackListener(int FuncNum )
{//register call back into so 
    ADHOCLOG("register listener_func into so...");
    void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        ADHOCLOG("MozAdhoc::dlerror########[ %s ]#####",dlerror());
    } else {
        ADHOCLOG("dlopen successed begin to register func into so.");

        switch(FuncNum)
        {
        case 1:
            {
                RegCallBackFuncPointer_data listener_func1 = ( RegCallBackFuncPointer_data)dlsym(handle, "addRecvDataListener");
                ADHOCLOG("dlsym addDataRecvListener successed.");
                listener_func1(OnDataRecvCallback);//register call back of so 
                ADHOCLOG("register addDataRecvListener successed.");
                break;
            }

        case 2:
            {
                RegCallBackFuncPointer_network listener_func2 = ( RegCallBackFuncPointer_network)dlsym(handle, "addNetWorkStatusListener");
                ADHOCLOG("dlsym AddNetWorkStatusListener successed.");
                listener_func2(OnNetWorkStatusCallback);//register call back of so 
                ADHOCLOG("register successed.");
                break;
            }
        case 3:
            {
                RegCallBackFuncPointer_voice listener_func3 = ( RegCallBackFuncPointer_voice)dlsym(handle, "addPcmVoiceListener");
                ADHOCLOG("dlsym AddPcmVoiceListener successed.");
                listener_func3(OnPcmVoiceCallback);//register call back of so 
                ADHOCLOG("register successed.");
                break;
            }

        default:
            {
                ADHOCLOG("#########register error RegFuncNum not matched please check your codes!!!#########");
                break;
            }
        }
        ADHOCLOG("register func into so finished.");

        dlclose(handle);
    }
}   


//TODO public function to get func by FuncName 
void
//auto
MozAdhoc::DlopenFuncWithName(char* FuncName)
{
    void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        ADHOCLOG("MozAdhoc::dlerror########[ %s ]#####",dlerror());
    } else {
        ADHOCLOG(",dlopen successed.");
        //获取add函数
        //注意：函数指针接收的add函数有几个参数和什么返回类型要一致
        //CalculatorFuncPointer_init listener_func = (CalculatorFuncPointer_init)dlsym(handle, "init");
        CalculatorFuncPointer_init listener_func = (CalculatorFuncPointer_init)dlsym(handle, FuncName);
        char paras[] = "testfunc"; 
        listener_func(paras);
        dlclose(handle);
    }
    //return listener_func;
}


nsresult
MozAdhoc::Init(const nsAString& aParas)
{
    void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        ADHOCLOG("MozAdhoc::dlerror########[ %s ]#####",dlerror());
    } else {
        ADHOCLOG("Init,dlopen successed.");
        CalculatorFuncPointer_init listener_func = (CalculatorFuncPointer_init)dlsym(handle, "init");
        
        nsAString::const_iterator start, end;
        aParas.BeginReading(start);
        aParas.EndReading(end);

        NS_ConvertUTF16toUTF8 cname(aParas);
        char* buffer = cname.BeginWriting();

        ADHOCLOG("conv to c++ : %s",buffer);
        listener_func(buffer);
        dlclose(handle);//? wether this okay?
    }
    return NS_OK;
}


nsresult
MozAdhoc::SendData(const nsAString& aData)
{
    ADHOCLOG(" enter SendData,methold of call dlopen");
    //链接并打开动态库
    void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        ADHOCLOG("MozAdhoc::OnVoiceData dlerror########[ %s ]#####",dlerror());
    } else {

        ADHOCLOG("MozAdhoc::OnVoiceData,dlopen successed.");
        //获取add函数
        //注意：函数指针接收的add函数有几个参数和什么返回类型要一致
        CalculatorFuncPointer add_func = (CalculatorFuncPointer)dlsym(handle, "add");
        int add_ret = add_func(10, 20);
        ADHOCLOG("add function result : %d \n", add_ret);
        ADHOCLOG("MozAdhoc::OnVoiceData add");

        dlclose(handle);
    } 

        return NS_OK;
}



nsresult
MozAdhoc::SetParameters(const nsAString& aParas)
{
    return NS_OK;
}

nsresult
MozAdhoc::CallbackJsForData(const nsAString &aAddr, const nsAString &aData)
{
    ADHOCLOG("MozAdhoc::CallbackJsForData");
    mozilla::ErrorResult rv;
    if(aDataCallback){
        aDataCallback->Call(aAddr, aData, rv);
    }
    return NS_OK;
}

nsresult
MozAdhoc::CallbackJsForNet(const int32_t &statusType, const nsAString &pParam)
{
    ADHOCLOG("MozAdhoc::CallbackJsForNet");
    mozilla::ErrorResult rv;
    if(aNetWorkCallback){
        aNetWorkCallback->Call(statusType, pParam, rv);
    }
    return NS_OK;
}


nsresult
MozAdhoc::CallbackJsForPcm(const int32_t &statusType, const nsAString &pParam)
{
    ADHOCLOG("MozAdhoc::CallbackJsForPcm");
    mozilla::ErrorResult rv;
    if(aPcmCallback){
        aPcmCallback->Call(statusType, pParam, rv);
    }
    return NS_OK;
}


nsresult
MozAdhoc::AddDataRecvListener(DataCallback& aCallback)
{ 
    int RegFuncNum = 1 ;
    ADHOCLOG("MozAdhoc::AddDataRecvListener");
    aDataCallback = &aCallback;
    RegisterCallBackListener(RegFuncNum);
    return NS_OK;
}

nsresult
MozAdhoc::AddNetWorkStatusListener(NetWorkCallback& aCallback)
{
    int RegFuncNum = 2 ;
    //char* RegFUncName = "addNetWorkStatusListener" ;
    ADHOCLOG("MozAdhoc::AddNetWorkStatusListener");
    aNetWorkCallback = &aCallback;
    RegisterCallBackListener(RegFuncNum);
    return NS_OK;
}

nsresult
MozAdhoc::AddPcmVoiceListener(PcmCallback& aCallback)
{
    int RegFuncNum = 3 ;
    //char* RegFUncName = "addPcmVoiceListener" ;
    ADHOCLOG("MozAdhoc::AddPcmVoiceListener");
    aPcmCallback = &aCallback;
    RegisterCallBackListener(RegFuncNum);
    return NS_OK;
}


JSObject*
MozAdhoc::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
    return MozAdhocBinding::Wrap(aCx, this, aGivenProto);
}

