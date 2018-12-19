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


///////所有接口如下:
///////////////////////////////////////////////////////////////////////// 
/**
  @param srcAddr 源ip
  @param desAddr 目的ip
  @param data 数据内容
 **/
/*
 *typedef int (*DataRecvCallback)(char *srcAddr, unsigned char *pData);
 *typedef int (*NetWorkStatusCallback)(int statusType, char *pParam);
 *typedef int (*PcmVoiceCallback)(int statusType, char *pParam);
 *
 *struct{
 *    char mPlatform[32];
 *    char mUEKind[32];
 *    char mHW[32];
 *    char mOS[32];
 *    int mType;
 *}ConfigParams;
 *
 *
 *void init(char paras[]);//初始化自组网
 *void tellLocalIpAddr(char ipAddr[]);//设置本机自组网ip
 *void queryNetWorkStatus(char cmd[]);//查询网络状态
 *void setParameters(char paras[]);//设置自组网参数
 *void setPttState (char state[]);//设置ptt状态
 *void sendBDMsg2AdHoc(char  gga[]);//发送北斗消息到自组网
 *void sendExtraData (int type, char data[]);//执行shell命令
 *void setIsNeedHeadForUserData(bool isNeedHead);//设置用户数据是否需要头部
 *
 *
 *int openAdHocDevice();//打开自组网设备 0:打开成功, 非0:打开失败
 *int closeAdHocDevice();//关闭自组网设备
 *int reOpenAdHocDevice();//重打开自组网设备
 *int setEnabled (bool enabled);//设置数据传输模式，ip或非ip true :非ip模式，false :ip模式
 *int sendData( char data[]);//发送数据
 *int sendData3(char srcAddr[], char desAddr[], char data[]);//发送数据
 *int sendDataPri(char srcAddr[],  char desAddr[],  char data[], int pri);//发送数据
 *int sendPcmVoice(char voiceData[]);//发送话音数据
 *int setEthernetIP(char ipAddr[]);//设置以太网ip
 *int updateModem(char fileName[], char data[]);//升级modem
 *
 *
 *ConfigParams getConfigParams();//获取平台信息
 *char* getEthernetIP();//查询以太网ip
 *
 *
 *bool isEnabled();//查询数据传输模式 
 *bool isNetWorkAvailable();//网络是否可用
 *bool addDataRecvListener(DataRecvCallback *pFunc);//添加接收数据回调监听器    
 *bool addNetWorkStatusListener(NetWorkStatusCallback *pFunc);//添加网络状态回调监听器
 *bool addPcmVoiceListener(PcmVoiceCallback *pFunc);//添加接收话音回调监听器
 *
 */

///////////////////////////////////////////////////////////////////////// 




//定义函数指针
////////////////////////////////////////////////////// 

/**
  @param srcAddr 源ip
  @param desAddr 目的ip
  @param data 数据内容
 **/

typedef struct{
	char mPlatform[32];
	char mUEKind[32];
	char mHW[32];
	char mOS[32];
	int mType;
}ConfigParams;

typedef int (*DataRecvCallback)(char *srcAddr, unsigned char *pData);
typedef int (*NetWorkStatusCallback)(int statusType, char *pParam);
typedef int (*PcmVoiceCallback)(int statusType, char *pParam);

/*
 *typedef void (*DataRecvCallback)(char *srcAddr, unsigned char *pData);
 *typedef void (*NetWorkStatusCallback)(int statusType, char *pParam);
 *typedef void (*PcmVoiceCallback)(int statusType, char *pParam);
 */

/*
 *typedef void (*RegCallBackFuncPointer_data)(DataRecvCallback acall);
 *typedef void (*RegCallBackFuncPointer_network)(NetWorkStatusCallback acall);
 *typedef void (*RegCallBackFuncPointer_voice)(PcmVoiceCallback acall);
 */

typedef int (*RegCallBackFuncPointer_data)(DataRecvCallback acall);
typedef int (*RegCallBackFuncPointer_network)(NetWorkStatusCallback acall);
typedef int (*RegCallBackFuncPointer_voice)(PcmVoiceCallback acall);

//8 void
typedef void (*Init_ptr)(char paras[]);//初始化自组网
typedef void (*TellLocalIpAddr_ptr)(char ipAddr[]);//设置本机自组网ip
typedef void (*QueryNetWorkStatus_ptr)(char cmd[]);//查询网络状态
typedef void (*SetParameters_ptr)(char paras[]);//设置自组网参数
typedef void (*SetPttState_ptr)(char state[]);//设置ptt状态
typedef void (*SendBDMsg2AdHoc_ptr)(char gga[]);//发送北斗消息到自组网
//typedef void (*SendExtraData_ptr)(int type, char data[]);//执行shell命令TODO
typedef void (*SendExtraData_ptr)(char type[], char data[]);//执行shell命令 TODO
typedef void (*SetIsNeedHeadForUserData_ptr)(bool isNeedHead);//设置用户数据是否需要头部

//10 int 
typedef int (*OpenAdHocDevice_ptr)();//打开自组网设备 0:打开成功, 非0:打开失败
typedef int (*CloseAdHocDevice_ptr)();//关闭自组网设备
typedef int (*ReOpenAdHocDevice_ptr)();//重打开自组网设备
typedef int (*SetEnabled_ptr)(bool enabled);//设置数据传输模式，ip或非ip true :非ip模式，false :ip模式
typedef int (*SendData_ptr)( char data[]);//发送数据
typedef int (*SendData3_ptr)(char srcAddr[], char desAddr[], char data[]);//发送数据
typedef int (*SendDataPri_ptr)(char srcAddr[],  char desAddr[],  char data[], int pri);//发送数据 TODO
//typedef int (*SendDataPri_ptr)(char srcAddr[],  char desAddr[],  char data[], char pri[]);//发送数据
typedef int (*SendPcmVoice_ptr)(char voiceData[]);//发送话音数据
typedef int (*SetEthernetIP_ptr)(char ipAddr[]);//设置以太网ip
typedef int (*UpdateModem_ptr)(char fileName[], char data[]);//升级modem

//2 diff 
typedef ConfigParams (*GetConfigParams_ptr)();//获取平台信息
typedef char* (*GetEthernetIP_ptr)();//查询以太网ip

// 5 bool TODO
typedef bool (*IsEnabled_ptr)();//查询数据传输模式 
typedef bool (*IsNetWorkAvailable_ptr)();//网络是否可用
typedef bool (*AddDataRecvListener_ptr)(DataRecvCallback *pFunc);//添加接收数据回调监听器    
typedef bool (*AddNetWorkStatusListener_ptr)(NetWorkStatusCallback *pFunc);//添加网络状态回调监听器
typedef bool (*AddPcmVoiceListener_ptr)(PcmVoiceCallback *pFunc);//添加接收话音回调监听器

/////////////////////////////////////////////////// 


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



//动态库地址
const char *dlib_path = "/system/lib/libadhocyaya.so";

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
		Init_ptr  dl_func = (Init_ptr)dlsym(handle, FuncName);
		char paras[] = "testfunc"; 
		dl_func(paras);
		dlclose(handle);
	}
	//return dl_func;
}


char*  StringConverter(const nsAString& aPara)
{
	nsAString::const_iterator start, end;
	aPara.BeginReading(start);
	aPara.EndReading(end);
	NS_ConvertUTF16toUTF8 cname(aPara);
	char* buffer = cname.BeginWriting();
	return buffer;
}

static 
//void
	int
OnDataRecvCallback(char *srcAddr, unsigned char *pData)
{
	//this callback will called by so func 
	ADHOCLOG("###########callback connect result: %s %s,###########",srcAddr,pData);

	nsAutoString addr, data;
	addr.AssignWithConversion(srcAddr);
	data.AssignWithConversion((const char *)pData);
	MozAdhoc::GetAdhocInstance()->CallbackJsForData(addr, data);
	return 233;
}



static 
//void
	int
OnNetWorkStatusCallback(int statusType, char *pParam)
{
	//this callback will called by so func 
	ADHOCLOG("###########callback connect###########");
	nsAutoString param;
	param.AssignWithConversion((const char *)pParam);
	MozAdhoc::GetAdhocInstance()->CallbackJsForNet(statusType, param);
	return 233;
}

static 
//void
	int
OnPcmVoiceCallback(int statusType, char *pParam)
{
	//this callback will called by so func 
	ADHOCLOG("###########callback connect###########");
	nsAutoString param;
	param.AssignWithConversion((const char *)pParam);
	MozAdhoc::GetAdhocInstance()->CallbackJsForPcm(statusType, param);
	return 233;
}


	void
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
				RegCallBackFuncPointer_data dl_func1 = ( RegCallBackFuncPointer_data)dlsym(handle, "addRecvDataListener");
				ADHOCLOG("dlsym addDataRecvListener successed.");
				dl_func1(OnDataRecvCallback);//register call back of so 
				ADHOCLOG("register addDataRecvListener successed.");
				break;
			}

		case 2:
			{
				RegCallBackFuncPointer_network dl_func2 = ( RegCallBackFuncPointer_network)dlsym(handle, "addNetWorkStatusListener");
				ADHOCLOG("dlsym AddNetWorkStatusListener successed.");
				dl_func2(OnNetWorkStatusCallback);//register call back of so 
				ADHOCLOG("register successed.");
				break;
			}
		case 3:
			{
				RegCallBackFuncPointer_voice dl_func3 = ( RegCallBackFuncPointer_voice)dlsym(handle, "addPcmVoiceListener");
				ADHOCLOG("dlsym AddPcmVoiceListener successed.");
				dl_func3(OnPcmVoiceCallback);//register call back of so 
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


/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
/*void 8*/
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
	nsresult
MozAdhoc::Init(const nsAString& aParas)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		ADHOCLOG("MozAdhoc::dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("Init,dlopen successed.");
		Init_ptr dl_func = (Init_ptr)dlsym(handle, "init");

		char* aParas_cv = StringConverter(aParas);
		dl_func(aParas_cv);
		ADHOCLOG("conv to c++ : %s",aParas_cv);
		//dl_func(buffer);
		dlclose(handle);//? wether this okay?
	}
	return NS_OK;
}
	nsresult
MozAdhoc:: TellLocalIpAddr(const nsAString& aIpAddr)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		TellLocalIpAddr_ptr dl_func = (TellLocalIpAddr_ptr )dlsym(handle, "tellLocalIpAddr");
		char* aIpAddr_cv = StringConverter(aIpAddr);
		dl_func(aIpAddr_cv );
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return NS_OK; 
}//设置本机自组网ip


	nsresult
MozAdhoc:: QueryNetWorkStatus(const nsAString& aCmd)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		QueryNetWorkStatus_ptr dl_func = (QueryNetWorkStatus_ptr )dlsym(handle, "queryNetWorkStatus");
		char* aCmd_cv= StringConverter(aCmd);
		dl_func(aCmd_cv);
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return NS_OK; 
}//查询网络状态

	nsresult
MozAdhoc:: SetParameters(const nsAString& aParas)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SetParameters_ptr dl_func = (SetParameters_ptr )dlsym(handle, "setParameters");
		char* aParas_cv = StringConverter(aParas);
		dl_func(aParas_cv);
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return NS_OK; 
}//设置自组网参数


	nsresult
MozAdhoc:: SetPttState(const nsAString& aState)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SetPttState_ptr dl_func = (SetPttState_ptr )dlsym(handle, "setPttState");
		char* aState_cv = StringConverter(aState);
		dl_func(aState_cv );
		//ADHOCLOG("return successed. ret=%s",ret);
		dlclose(handle);
	}

	return NS_OK; 
}//设置ptt状态


	nsresult
MozAdhoc:: SendBDMsg2AdHoc(const nsAString& aGga)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SendBDMsg2AdHoc_ptr dl_func = (SendBDMsg2AdHoc_ptr )dlsym(handle, "sendBDMsg2AdHoc");
		char* aData_cv = StringConverter(aGga);
		dl_func(aData_cv);
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return NS_OK; 
}//发送北斗消息到自组网


	nsresult
MozAdhoc:: SendExtraData (const nsAString& aType, const nsAString& aData)//TODO atype should be int
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SendExtraData_ptr dl_func = (SendExtraData_ptr )dlsym(handle, "sendExtraData");
		char* aType_cv = StringConverter(aType);
		char* aData_cv = StringConverter(aData);
		dl_func(aType_cv,aData_cv);
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return NS_OK; 
}//执行shell命令


	nsresult
MozAdhoc::SetIsNeedHeadForUserData(bool& aIsNeedHead)
{
	ADHOCLOG("******* enter*********");


	return NS_OK; 
}//设置用户数据是否需要头部 TODO



/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
//int 10
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 

int16_t
//nsresult 
MozAdhoc::	OpenAdHocDevice()
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		OpenAdHocDevice_ptr dl_func = (OpenAdHocDevice_ptr)dlsym(handle, "openAdHocDevice");
		int ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	} 


	return 233; 
	//return NS_OK; 
}


int16_t
//nsresult 
MozAdhoc::CloseAdHocDevice()
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		CloseAdHocDevice_ptr dl_func = (CloseAdHocDevice_ptr )dlsym(handle, "closeAdHocDevice");
		int ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 233; 
	//return NS_OK; 
}

int16_t
//nsresult 
MozAdhoc::ReOpenAdHocDevice()
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		ReOpenAdHocDevice_ptr dl_func = (ReOpenAdHocDevice_ptr )dlsym(handle, "reOpenAdHocDevice");
		int ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 233; 
	//return NS_OK; 
}

int16_t
//nsresult
MozAdhoc::SetEnabled (bool& aEnabled)
{

	ADHOCLOG("******* enter*********");

	return 233; 
	//return NS_OK; 

}//设置数据传输模式，ip或非ip.true :非ip模式，false :ip模式 TODO


int16_t
//nsresult
MozAdhoc::SendData(const nsAString& aData)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SendData_ptr dl_func = (SendData_ptr )dlsym(handle, "sendData");
		char* aData_cv = StringConverter(aData);
		int ret = dl_func(aData_cv);
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	} 

	return 233; 
	//return NS_OK;
}


int16_t
//nsresult
MozAdhoc:: SendData(const nsAString& aSrcAddr, const nsAString& aDesAddr, const nsAString& aData)
{
	ADHOCLOG("******* enter*********");

	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SendData3_ptr dl_func = (SendData3_ptr )dlsym(handle, "sendData3");
		char* aSrcAddr_cv = StringConverter(aSrcAddr);
		char* aDesAddr_cv = StringConverter(aDesAddr);
		char* aData_cv = StringConverter(aData);
		int ret = dl_func(aSrcAddr_cv,aDesAddr_cv,aData_cv);

		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	} 

	return 233; 
	//return NS_OK; 
}//发送数据



int16_t
//nsresult
MozAdhoc:: SendDataPri(const nsAString& aSrcAddr,const nsAString& aDesAddr,const nsAString& aData, int16_t aPri)//TODO pri -> int 
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SendDataPri_ptr dl_func = (SendDataPri_ptr )dlsym(handle, "sendDataPri");
		char* aSrcAddr_cv = StringConverter(aSrcAddr);
		char* aDesAddr_cv = StringConverter(aDesAddr);
		char* aData_cv = StringConverter(aData);
		//char* aPri_cv = StringConverter(aPri);
		int aPri_cv = aPri;
		int ret = dl_func(aSrcAddr_cv,aDesAddr_cv,aData_cv,aPri_cv);
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 233; 
	//return NS_OK; 
}//发送数据


int16_t
//nsresult
MozAdhoc:: SendPcmVoice(const nsAString& aVoiceData)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SendPcmVoice_ptr dl_func = (SendPcmVoice_ptr )dlsym(handle, "sendPcmVoice");
		char* aData_cv = StringConverter(aVoiceData);
		int ret = dl_func(aData_cv);
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 233; 
	//return NS_OK; 
}//发送话音数据


int16_t
//nsresult
MozAdhoc:: SetEthernetIP(const nsAString& aIpAddr)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		SetEthernetIP_ptr dl_func = (SetEthernetIP_ptr )dlsym(handle, "setEthernetIP");
		char* aIpAddr_cv = StringConverter(aIpAddr);
		dl_func(aIpAddr_cv );
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 233; 
	//return NS_OK; 
}//设置以太网ip


int16_t
//nsresult
MozAdhoc:: UpdateModem(const nsAString& aFileName, const nsAString& aData)
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		UpdateModem_ptr dl_func = (UpdateModem_ptr )dlsym(handle, "updateModem");
		char* aFileName_cv = StringConverter(aFileName);
		char* aData_cv = StringConverter(aData);
		int ret = dl_func(aFileName_cv ,aData_cv);
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 233; 
	//return NS_OK; 
}//升级modem

/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 




/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
/*diff 2*/
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
/*ConfigParams getConfigParams();//获取平台信息*/
/*char* getEthernetIP();//查询以太网ip TODO*/
//void getEthernetIP();//查询以太网ip



ConfigParams getConfigParams()
{
	ConfigParams ret;
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		//SendData_ptr3 dl_func = (SendData_ptr3 )dlsym(handle, "getConfigParams");
		//dl_func(aSrcAddr_cv,aDesAddr_cv,aData_cv);
		//ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}//获取平台信息
	return ret;
}

/*char* getEthernetIP();//查询以太网ip*/
	nsresult 
MozAdhoc::GetEthernetIP()
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		GetEthernetIP_ptr dl_func = (GetEthernetIP_ptr )dlsym(handle, "getEthernetIP");
		char* ret = dl_func();
		ADHOCLOG("return successed. ret=%s",ret);
		dlclose(handle);
	}

	return NS_OK; 
}

/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 







/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
//bool 5
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 

//short isEnabled();//查询数据传输模式 换int 
//short isNetWorkAvailable();//网络是否可用


//int16_t
	bool
MozAdhoc::IsEnabled()
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		IsEnabled_ptr dl_func = (IsEnabled_ptr )dlsym(handle, "isEnabled");
		bool ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 1;

}//查询数据传输模式 换int 
//int16_t
	bool
MozAdhoc::IsNetWorkAvailable()
{
	ADHOCLOG("******* enter*********");
	void *handle = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW); 
	if (handle == NULL) {
		fprintf(stderr, "%s", dlerror());
		ADHOCLOG(" dlerror########[ %s ]#####",dlerror());
	} else {
		ADHOCLOG("dlopen successed.");
		IsNetWorkAvailable_ptr dl_func = (IsNetWorkAvailable_ptr)dlsym(handle, "isNetWorkAvailable");
		bool ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
		dlclose(handle);
	}

	return 1;


}//网络是否可用

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

/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 






/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
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


	JSObject*
MozAdhoc::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
	return MozAdhocBinding::Wrap(aCx, this, aGivenProto);
}

