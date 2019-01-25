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

//语音数据数组大小
#define MAX_SIZE_STR 65530

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


typedef int (*RegCallBackFuncPointer_data)(DataRecvCallback acall);
typedef int (*RegCallBackFuncPointer_network)(NetWorkStatusCallback acall);
typedef int (*RegCallBackFuncPointer_voice)(PcmVoiceCallback acall);
typedef int (*RegCallBackFuncPointer_voice_spe_api)(PcmVoiceCallback acall,char voiceData[]);//special api for test of  voicedata,char* voiceData

//8 void
typedef void (*Init_ptr)(char paras[]);//初始化自组网
typedef void (*TellLocalIpAddr_ptr)(char ipAddr[]);//设置本机自组网ip
typedef void (*QueryNetWorkStatus_ptr)(char cmd[]);//查询网络状态
typedef void (*SetParameters_ptr)(char paras[]);//设置自组网参数
typedef void (*SetPttState_ptr)(char state[]);//设置ptt状态
typedef void (*SendBDMsg2AdHoc_ptr)(char gga[]);//发送北斗消息到自组网
typedef void (*SendExtraData_ptr)(int type, char data[]);//执行shell命令TODO
typedef void (*SetIsNeedHeadForUserData_ptr)(bool isNeedHead);//设置用户数据是否需要头部

//10 int 
typedef int (*OpenAdHocDevice_ptr)();//打开自组网设备 0:打开成功, 非0:打开失败
typedef int (*CloseAdHocDevice_ptr)();//关闭自组网设备
typedef int (*ReOpenAdHocDevice_ptr)();//重打开自组网设备
typedef int (*SetEnabled_ptr)(bool enabled);//设置数据传输模式，ip或非ip true :非ip模式，false :ip模式
typedef int (*SendData_ptr)( char data[]);//发送数据
typedef int (*SendData3_ptr)(char srcAddr[], char desAddr[], char data[]);//发送数据
typedef int (*SendDataPri_ptr)(char srcAddr[],  char desAddr[],  char data[], int pri);//发送数据 TODO
//typedef int (*SendPcmVoice_ptr)(char* voiceData);//发送话音数据
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


using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::adhoc;

mozilla::dom::ConfigCallback* aConfigCallback = nullptr;
mozilla::dom::IpCallback* aIpCallback = nullptr;
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




	//动态库地址
	const char *dlib_path = "/system/lib/libadhocd.so";
	void* handle_global;//dlopen的全局handle

void dlInit()
{
	handle_global = dlopen(dlib_path, RTLD_GLOBAL | RTLD_NOW);
	if (handle_global == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		ADHOCLOG("MozAdhoc::dlerror########[ %s ]#####",dlerror());
		ADHOCLOG("####Error#### error sorry, dlopen failed,please check your .SO lived under:%s exists.",dlib_path);
	} else {
		ADHOCLOG(",dlopen successed.");
	}
}

void dlDeInit()
{
	dlclose(handle_global);
	ADHOCLOG("dlclose finished!");
}

MozAdhoc::MozAdhoc(nsPIDOMWindow* aWindow)
	: mWindow(aWindow)
{
	ADHOCLOG("MozAdhoc enter MozAdhoc constructing\n");
	sAdhocInstance = this;
	dlInit();
	return;
}

MozAdhoc::~MozAdhoc()
{
	ADHOCLOG("MozAdhoc enter ~MozAdhoc  destructing\n");
	sAdhocInstance = NULL;
	dlDeInit();
	return;
}


//TODO public function to get func by FuncName from so with dlopen 
void
//auto
MozAdhoc::DlopenFuncWithName(char* FuncName)
{
	//done with: dlInit() and dlDeInit() 
}


//conver nsAString to char* 
void  StringConverter(const nsAString& aPara,char* result,int size)
{
	nsAString::const_iterator start, end;
	aPara.BeginReading(start);
	aPara.EndReading(end);
	NS_ConvertUTF16toUTF8 cname(aPara);
	char* temp_str = cname.BeginWriting();
	strncpy(result,temp_str,size);//copy value to result ,pass from caller
	ADHOCLOG("aVoiceData  =%s ",temp_str);
}


static 
	int
OnDataRecvCallback(char *srcAddr, unsigned char *pData)
{
	//this callback will called by so func 
	ADHOCLOG("###########callback connect result: %s %s,###########",srcAddr,pData);
	nsAutoString addr, data;
	addr.AssignWithConversion(srcAddr);
	data.AssignWithConversion((const char *)pData);
	MozAdhoc::GetAdhocInstance()->CallbackJsForData(addr, data);
	return 1;
}



static 
	int
OnNetWorkStatusCallback(int statusType, char *pParam)
{
	//this callback will called by so func 
	ADHOCLOG("###########callback connect###########");
	nsAutoString param;
	param.AssignWithConversion((const char *)pParam);
	MozAdhoc::GetAdhocInstance()->CallbackJsForNet(statusType, param);
	return 1;
}

static 
	int
OnPcmVoiceCallback(int statusType, char *pParam)
{
	//this callback will called by so func 
	ADHOCLOG("###########callback connect###########");
	nsAutoString param;
	param.AssignWithConversion((const char *)pParam);
	MozAdhoc::GetAdhocInstance()->CallbackJsForPcm(statusType, param);
	return 1;
}


	void
MozAdhoc::RegisterCallBackListener(int FuncNum )
{//register call back into so 
	ADHOCLOG("register listener_func into so...");
	void *handle = handle_global;
	if (handle != NULL) {
		switch(FuncNum)
		{
		case 1:
			{
				RegCallBackFuncPointer_data dl_func1 = ( RegCallBackFuncPointer_data)dlsym(handle, "addDataRecvListener");
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
		case 4:
			{//TODO
				/*
				 *RegCallBackFuncPointer_voice_spe_api dl_func4 = ( RegCallBackFuncPointer_voice_spe_api)dlsym(handle, "addPcmVoiceListener_spe_api");
				 *ADHOCLOG("dlsym AddPcmVoiceListener_spe_api successed.");
				 *dl_func4(OnPcmVoiceCallback,voiceData_gloabal);//register call back of so 
				 *ADHOCLOG("voiceData_gloabal=%s ",voiceData_gloabal);
				 */
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

	}
}   


/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
/*void 8*/
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
	nsresult
MozAdhoc::Init(const nsAString& aParas)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		Init_ptr dl_func = (Init_ptr)dlsym(handle, "init");
		char* aParas_cv = (char*)malloc(size+1);
		StringConverter(aParas,aParas_cv ,size);
		dl_func(aParas_cv);
		ADHOCLOG("conv to c++ : %s",aParas_cv);
		free(aParas_cv);
	}
	return NS_OK;
}
	nsresult
MozAdhoc:: TellLocalIpAddr(const nsAString& aIpAddr)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		TellLocalIpAddr_ptr dl_func = (TellLocalIpAddr_ptr )dlsym(handle, "tellLocalIpAddr");
		char* aIpAddr_cv = (char*)malloc(size+1);
		StringConverter(aIpAddr,aIpAddr_cv  ,size);
		ADHOCLOG("conv to c++ : %s",aIpAddr_cv);
		dl_func(aIpAddr_cv );
		free(aIpAddr_cv );
	}

	return NS_OK; 
}//设置本机自组网ip


	nsresult
MozAdhoc:: QueryNetWorkStatus(const nsAString& aCmd)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		QueryNetWorkStatus_ptr dl_func = (QueryNetWorkStatus_ptr )dlsym(handle, "queryNetWorkStatus");
		char* aCmd_cv= (char*)malloc(size+1);
		StringConverter(aCmd,aCmd_cv ,size);
		dl_func(aCmd_cv);
		free(aCmd_cv);
	}

	return NS_OK; 
}//查询网络状态

	nsresult
MozAdhoc:: SetParameters(const nsAString& aParas)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SetParameters_ptr dl_func = (SetParameters_ptr )dlsym(handle, "setParameters");
		char* aParas_cv = (char*)malloc(size+1);
		StringConverter(aParas,aParas_cv  ,size);
		dl_func(aParas_cv);
		free(aParas_cv );
	}

	return NS_OK; 
}//设置自组网参数


	nsresult
MozAdhoc:: SetPttState(const nsAString& aState)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SetPttState_ptr dl_func = (SetPttState_ptr )dlsym(handle, "setPttState");
		char* aState_cv = (char*)malloc(size+1);
		StringConverter(aState,aState_cv  ,size);
		dl_func(aState_cv );
		free(aState_cv );
	}

	return NS_OK; 
}//设置ptt状态


	nsresult
MozAdhoc:: SendBDMsg2AdHoc(const nsAString& aGga)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SendBDMsg2AdHoc_ptr dl_func = (SendBDMsg2AdHoc_ptr )dlsym(handle, "sendBDMsg2AdHoc");
		char* aData_cv = (char*)malloc(size+1);
		StringConverter(aGga,aData_cv  ,size);
		dl_func(aData_cv);
		free(aData_cv );
	}

	return NS_OK; 
}//发送北斗消息到自组网


	nsresult
MozAdhoc:: SendExtraData (int16_t aType, const nsAString& aData)//TODO atype should be int
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SendExtraData_ptr dl_func = (SendExtraData_ptr )dlsym(handle, "sendExtraData");
		int aType_cv = aType;
		char* aData_cv = (char*)malloc(size+1);
		StringConverter(aData,aData_cv  ,size);
		dl_func(aType_cv,aData_cv);
		free(aData_cv );
	}
	return NS_OK; 
}//执行shell命令


	nsresult
MozAdhoc::SetIsNeedHeadForUserData(bool& aIsNeedHead)
{
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SetIsNeedHeadForUserData_ptr dl_func = (SetIsNeedHeadForUserData_ptr )dlsym(handle, "setIsNeedHeadForUserData");
		dl_func(aIsNeedHead);
	}
	return NS_OK; 
}//设置用户数据是否需要头部 TODO



/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
//int 10
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 

	int16_t
MozAdhoc::	OpenAdHocDevice()
{
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		OpenAdHocDevice_ptr dl_func = (OpenAdHocDevice_ptr)dlsym(handle, "openAdHocDevice");
		int ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
	}
	return 1; 
}


	int16_t
MozAdhoc::CloseAdHocDevice()
{
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		CloseAdHocDevice_ptr dl_func = (CloseAdHocDevice_ptr )dlsym(handle, "closeAdHocDevice");
		int ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
	}
	return 1; 
}

	int16_t
MozAdhoc::ReOpenAdHocDevice()
{
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		ReOpenAdHocDevice_ptr dl_func = (ReOpenAdHocDevice_ptr )dlsym(handle, "reOpenAdHocDevice");
		int ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
	}
	return 1; 
}

	int16_t
MozAdhoc::SetEnabled (bool& aEnabled)
{

	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SetEnabled_ptr dl_func = (SetEnabled_ptr )dlsym(handle, "setEnabled");
		int ret = dl_func(aEnabled);
		ADHOCLOG("return successed. ret=%d",ret);
	}
	return 1; 

}//设置数据传输模式，ip或非ip.true :非ip模式，false :ip模式 TODO


	int16_t
MozAdhoc::SendData(const nsAString& aData)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SendData_ptr dl_func = (SendData_ptr )dlsym(handle, "sendData");
		char* aData_cv = (char*)malloc(size+1);
		StringConverter(aData,aData_cv  ,size);
		int ret = dl_func(aData_cv);
		ADHOCLOG("return successed. ret=%d",ret);
		free(aData_cv );
	}

	return 1; 
}


	int16_t
MozAdhoc:: SendData(const nsAString& aSrcAddr, const nsAString& aDesAddr, const nsAString& aData)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SendData3_ptr dl_func = (SendData3_ptr )dlsym(handle, "sendData3");
		char* aSrcAddr_cv = (char*)malloc(size+1);
		StringConverter(aSrcAddr,aSrcAddr_cv  ,size);
		char* aDesAddr_cv = (char*)malloc(size+1);
		StringConverter(aDesAddr,aDesAddr_cv  ,size);
		char* aData_cv = (char*)malloc(size+1);
		StringConverter(aData,aData_cv  ,size);
		int ret = dl_func(aSrcAddr_cv,aDesAddr_cv,aData_cv);

		ADHOCLOG("return successed. ret=%d",ret);
		free(aDesAddr_cv );
		free(aSrcAddr_cv );
		free(aData_cv );
	}

	return 1; 
}//发送数据



	int16_t
MozAdhoc:: SendDataPri(const nsAString& aSrcAddr,const nsAString& aDesAddr,const nsAString& aData, int16_t aPri)//TODO pri -> int 
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SendDataPri_ptr dl_func = (SendDataPri_ptr )dlsym(handle, "sendDataPri");
		char* aSrcAddr_cv= (char*)malloc(size+1);
		char* aDesAddr_cv= (char*)malloc(size+1);
		char* aData_cv= (char*)malloc(size+1);
		StringConverter(aSrcAddr,aSrcAddr_cv,size);
		StringConverter(aDesAddr,aDesAddr_cv,size);
		StringConverter(aData,aData_cv,size);
		int aPri_cv = aPri;
		dl_func(aSrcAddr_cv,aDesAddr_cv,aData_cv,aPri_cv);
		ADHOCLOG("%s, %s, %s, %d",aSrcAddr_cv,aDesAddr_cv,aData_cv,aPri_cv);
		free(aSrcAddr_cv);
		free(aDesAddr_cv);
		free(aData_cv);
	}

	return 1; 
}//发送数据


	int16_t
MozAdhoc:: SendPcmVoice(const nsAString& aVoiceData)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SendPcmVoice_ptr dl_func = (SendPcmVoice_ptr )dlsym(handle, "sendPcmVoice");
		char* aData_cv=(char*)malloc(size+1);
		StringConverter(aVoiceData,aData_cv ,size);
		ADHOCLOG("sizeof =%d,aVoiceData =%s ",sizeof(aData_cv),aData_cv);
		dl_func(aData_cv);
		free(aData_cv);
	}

	return 1; 
}//发送话音数据


	int16_t
MozAdhoc:: SetEthernetIP(const nsAString& aIpAddr)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		SetEthernetIP_ptr dl_func = (SetEthernetIP_ptr )dlsym(handle, "setEthernetIP");
		char* aIpAddr_cv = (char*)malloc(size+1);
		StringConverter(aIpAddr,aIpAddr_cv  ,size);
		dl_func(aIpAddr_cv );
		free(aIpAddr_cv );
	}

	return 1; 
}//设置以太网ip


	int16_t
MozAdhoc:: UpdateModem(const nsAString& aFileName, const nsAString& aData)
{
	int size = 1024;
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		UpdateModem_ptr dl_func = (UpdateModem_ptr )dlsym(handle, "updateModem");
		char* aFileName_cv = (char*)malloc(size+1);
		StringConverter(aFileName,aFileName_cv  ,size);
		char* aData_cv = (char*)malloc(size+1);
		StringConverter(aData,aData_cv  ,size);
		int ret = dl_func(aFileName_cv ,aData_cv);
		ADHOCLOG("return successed. ret=%d",ret);
		free(aFileName_cv  );
		free(aData_cv );
	}

	return 1; 
}//升级modem

/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 




/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
/*diff 2*/
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
/*ConfigParams getConfigParams();//获取平台信息*/
/*char* getEthernetIP();//查询以太网ip TODO*/


//ConfigParams 
	bool
MozAdhoc::GetConfigParams(ConfigCallback& aCallback)
{
	aConfigCallback = &aCallback;
	ConfigParams ret;
	void *handle = handle_global;
	if (handle != NULL) {
		GetConfigParams_ptr dl_func = (GetConfigParams_ptr )dlsym(handle, "getConfigParams");
		ret = dl_func();
		ADHOCLOG("return successed. ret=%s",ret.mPlatform);
		nsAutoString aPlatform,aUekind,aHw,aOs;
		int aType;
		aPlatform.AssignWithConversion(ret.mPlatform);
		aUekind.AssignWithConversion(ret.mOS);
		aHw.AssignWithConversion(ret.mHW);
		aOs.AssignWithConversion(ret.mOS);
		aType = ret.mType;
		MozAdhoc::GetAdhocInstance()->CallbackJsForConfig(aPlatform,aUekind,aHw,aOs, aType);//platform,uekind,hw,os,type
	}//获取平台信息
	return true;
}

/*char* getEthernetIP();	*/
	bool
MozAdhoc::GetEthernetIP(IpCallback& aCallback)
{
	ADHOCLOG("******* enter*********");
	aIpCallback = &aCallback;
	char* ret = NULL; 
	void *handle = handle_global;
	if (handle != NULL) {
		GetEthernetIP_ptr dl_func = (GetEthernetIP_ptr )dlsym(handle, "getEthernetIP");
		ret = dl_func();
		ADHOCLOG("return successed. ret=%s",ret);
		nsAutoString ip;
		ip.AssignWithConversion(ret);
		MozAdhoc::GetAdhocInstance()->CallbackJsForIp(ip);
	}
	return ret;
}//查询以太网ip

/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 





/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 
//bool 5
/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 

//short isEnabled();//查询数据传输模式 换int 
//short isNetWorkAvailable();//网络是否可用


	bool
MozAdhoc::IsEnabled()
{
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		IsEnabled_ptr dl_func = (IsEnabled_ptr )dlsym(handle, "isEnabled");
		bool ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
	}
	return 1;
}//查询数据传输模式 换int 


	bool
MozAdhoc::IsNetWorkAvailable()
{
	ADHOCLOG("******* enter*********");
	void *handle = handle_global;
	if (handle != NULL) {
		IsNetWorkAvailable_ptr dl_func = (IsNetWorkAvailable_ptr)dlsym(handle, "isNetWorkAvailable");
		bool ret = dl_func();
		ADHOCLOG("return successed. ret=%d",ret);
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
	ADHOCLOG("MozAdhoc::AddNetWorkStatusListener");
	aNetWorkCallback = &aCallback;
	RegisterCallBackListener(RegFuncNum);
	return NS_OK;
}

	nsresult
MozAdhoc::AddPcmVoiceListener(PcmCallback& aCallback)
{
	int RegFuncNum = 3 ;
	ADHOCLOG("MozAdhoc::AddPcmVoiceListener");
	aPcmCallback = &aCallback;
	RegisterCallBackListener(RegFuncNum);
	return NS_OK;
}


////////////////////////////////special api to test pcmvoicedata //////////////////////////////
	nsresult
MozAdhoc::AddPcmVoiceListener_spe_api(PcmCallback& aCallback,const nsAString& aVoiceData)
{

	int size = MAX_SIZE_STR ;
	ADHOCLOG("register listener_func into so...");
	aPcmCallback = &aCallback;
	void *handle = handle_global;
	if (handle != NULL) {
		//int RegFuncNum = 4 ;
		//char* aVoiceData_cv=StringConverter(aVoiceData);//TODO
		//voiceData_gloabal = aData_cv;
		//RegisterCallBackListener(RegFuncNum);

		char* aData_cv = (char*)malloc(size+1);
		StringConverter(aVoiceData,aData_cv  ,size);
		RegCallBackFuncPointer_voice_spe_api dl_func4 = ( RegCallBackFuncPointer_voice_spe_api)dlsym(handle, "addPcmVoiceListener_spe_api");
		ADHOCLOG("aVoiceData sizeof p:%d,  =%s ",sizeof(aData_cv ),aData_cv  );
		dl_func4(OnPcmVoiceCallback,aData_cv);//register call back of so
		free(aData_cv);
	}
	return NS_OK;
}


/////////////////////////////////////////////////////////////////////// /////////////////////////////////////////////////////////////////////// 



///////////////////////////////////////////////////////////////////////callback /////////////////////////////////////////////////////////////////////// 
	nsresult 
MozAdhoc::CallbackJsForConfig(const nsAString &aPlatform,const nsAString &aUekind,const nsAString &aHw,const nsAString &aOs, int16_t aType)//platform,uekind,hw,os,type
{
	ADHOCLOG("MozAdhoc::CallbackJsForConfig");
	mozilla::ErrorResult rv;
	if(aConfigCallback ){
		aConfigCallback ->Call(aPlatform,aUekind,aHw,aOs, aType, rv);//platform,uekind,hw,os,type
	}
	return NS_OK;
}


	nsresult
MozAdhoc::CallbackJsForIp(const nsAString &aIp)
{
	ADHOCLOG("MozAdhoc::CallbackJsForIp");
	mozilla::ErrorResult rv;
	if(aIpCallback){
		aIpCallback->Call(aIp, rv);
	}
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


	JSObject*
MozAdhoc::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
	return MozAdhocBinding::Wrap(aCx, this, aGivenProto);
}


