#ifndef mozilla_dom_adhoc_MozAdhoc_h
#define mozilla_dom_adhoc_MozAdhoc_h
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/MozAdhocBinding.h"
#include "mozilla/dom/CallbackObject.h"

namespace mozilla{
class ErrorResult;
namespace dom{
namespace adhoc{

class MozAdhoc final :  public nsISupports
						,public nsWrapperCache

{
public:
	NS_DECL_CYCLE_COLLECTING_ISUPPORTS
		NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MozAdhoc)

		MozAdhoc(nsPIDOMWindow* aWindow);

	nsPIDOMWindow* GetParentObject()const{return mWindow;};
	virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
	/*
	 *nsresult
	 *    GetAdhoc_ip(const nsAString& aAdhoc_ip);
	 */

	void RegisterCallBackListener(int FuncNum);

	void DlopenFuncWithName(char*);
	//auto DlopenFuncWithName(char*);



	//////////////////void 8 ////////////////// ////////////////// ////////////////// ////////////////// //////////////////
	nsresult
		Init(const nsAString& aParas);
	nsresult
		TellLocalIpAddr(const nsAString& aIpAddr);//设置本机自组网ip
	nsresult
		QueryNetWorkStatus(const nsAString& aCmd);//查询网络状态
	nsresult
		SetParameters(const nsAString& aParas);//设置自组网参数
	nsresult
		SetPttState (const nsAString& aState);//设置ptt状态
	nsresult
		SendBDMsg2AdHoc(const nsAString& aGga);//发送北斗消息到自组网
	nsresult
		SendExtraData (const nsAString& aType, const nsAString& aData);//执行shell命令
	nsresult
		SetIsNeedHeadForUserData(bool& aIsNeedHead);




	//////////////////int 10 ////////////////// ////////////////// ////////////////// ////////////////// //////////////////

	//nsresult
	int16_t
		OpenAdHocDevice();
	//nsresult
	int16_t
		CloseAdHocDevice();
	//nsresult
	int16_t
		ReOpenAdHocDevice();	
	//nsresult
	int16_t
		SetEnabled (bool& aEnabled);//设置数据传输模式，ip或非ip.true :非ip模式，false :ip模式
	//nsresult
	int16_t
		SendData(const nsAString& aData);
	//nsresult
	int16_t
		SendData(const nsAString& aSrcAddr, const nsAString& aDesAddr, const nsAString& aData);//发送数据
	//nsresult
	int16_t
		SendDataPri(const nsAString& aSrcAddr,const nsAString& aDesAddr,const nsAString& aData, int16_t aPri);//发送数据
	//nsresult
	int16_t
		SendPcmVoice(const nsAString& aVoiceData);//发送话音数据
	//nsresult
	int16_t
		SetEthernetIP(const nsAString& aIpAddr);//设置以太网ip
	//nsresult
	int16_t
		UpdateModem(const nsAString& aFileName, const nsAString& aData);//升级modem




	//////////////////diff 2 ////////////////// ////////////////// ////////////////// ////////////////// //////////////////

	/*ConfigParams getConfigParams();//获取平台信息 TODO*/
	//ConfigParams 
	//GetConfigParams();//获取平台信息 TODO
	/*char* getEthernetIP();//查询以太网ip TODO*/
	//nsresult
	bool
		//char* 
		GetEthernetIP(IpCallback& aCallback);//查询以太网ip




	//////////////////bool 5 TODO ////////////////// ////////////////// ////////////////// ////////////////// //////////////////

	bool
		//int16_t
		IsEnabled();//查询数据传输模式 换int 
	bool
		//int16_t
		IsNetWorkAvailable();//网络是否可用
	nsresult
		AddDataRecvListener(DataCallback& aCallback);//添加接收数据回调监听器    
	nsresult
		AddNetWorkStatusListener(NetWorkCallback& aCallback);//添加网络状态回调监听器
	nsresult
		AddPcmVoiceListener(PcmCallback& aCallback);//添加接收话音回调监听器


	////////////////// ////////////////// ////////////////// ////////////////// ////////////////// //////////////////




	static MozAdhoc *GetAdhocInstance()
	{
		return sAdhocInstance;
	}
	nsresult CallbackJsForIp(const nsAString &aAddr, const nsAString &aData);
	nsresult CallbackJsForData(const nsAString &aAddr, const nsAString &aData);
	nsresult CallbackJsForNet(const int32_t &statusType, const nsAString &pParam);
	nsresult CallbackJsForPcm(const int32_t &statusType, const nsAString &pParam);


private:
	virtual ~MozAdhoc();
	static MozAdhoc *sAdhocInstance ;

	//nsString madhoc_ip;

protected:
	nsCOMPtr<nsPIDOMWindow> mWindow;
};

}// namespace adhoc
}// namespace dom
}// namespace mozilla
#endif
