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

    void RegisterCallBackListener(int FuncNum);

    void DlopenFuncWithName(char*);
    //auto DlopenFuncWithName(char*);

    NS_IMETHODIMP
        Init(const nsAString& aParas);

    NS_IMETHODIMP
        SendData(const nsAString& aData);

    NS_IMETHODIMP
        SetParameters(const nsAString& aParas);

    NS_IMETHODIMP
        AddDataRecvListener(DataCallback& aCallback);

    NS_IMETHODIMP
        AddNetWorkStatusListener(NetWorkCallback& aCallback);

    NS_IMETHODIMP
        AddPcmVoiceListener(PcmCallback& aCallback);


    static MozAdhoc *GetAdhocInstance()
    {
        return sAdhocInstance;
    }
    nsresult CallbackJsForData(const nsAString &aAddr, const nsAString &aData);
    nsresult CallbackJsForNet(const int32_t &statusType, const nsAString &pParam);
    nsresult CallbackJsForPcm(const int32_t &statusType, const nsAString &pParam);


private:
    virtual ~MozAdhoc();
    static MozAdhoc *sAdhocInstance ;


protected:
    nsCOMPtr<nsPIDOMWindow> mWindow;
};

}// namespace adhoc
}// namespace dom
}// namespace mozilla
#endif
