//this is interface of adhoc to gaia apps
//[CheckAnyPermissions="adhoc", AvailableIn=CertifiedApps]

[NoInterfaceObject]
interface MozAdhoc{
	[NewObject]

/*void 8*/
	void init(DOMString paras);//初始化自组网
	void tellLocalIpAddr(DOMString ipAddr);//设置本机自组网ip
	void queryNetWorkStatus(DOMString cmd);//查询网络状态
	void setParameters(DOMString paras);//设置自组网参数
	void setPttState (DOMString state);//设置ptt状态
	void sendBDMsg2AdHoc(DOMString  gga);//发送北斗消息到自组网
	void sendExtraData (DOMString type, DOMString data);//执行shell命令
	void setIsNeedHeadForUserData(boolean isNeedHead);//设置用户数据是否需要头部 TODO


/*int 10*/
	short openAdHocDevice();//打开自组网设备 0:打开成功, 非0:打开失败
	short closeAdHocDevice();//关闭自组网设备
	short reOpenAdHocDevice();//重打开自组网设备
	short setEnabled (boolean enabled);//设置数据传输模式，ip或非ip.true :非ip模式，false :ip模式
	short sendData( DOMString data);//发送数据
	short sendData(DOMString srcAddr, DOMString desAddr, DOMString data);//发送数据
	short sendDataPri(DOMString srcAddr,DOMString desAddr,DOMString data, short pri);//发送数据 TODO
	short sendPcmVoice(DOMString voiceData);//发送话音数据
	short setEthernetIP(DOMString ipAddr);//设置以太网ip
	short updateModem(DOMString fileName, DOMString data);//升级modem


/*diff 2*/
	/*ConfigParams getConfigParams();//获取平台信息*/
	/*char* getEthernetIP();//查询以太网ip TODO*/
	void getEthernetIP();//查询以太网ip

/*bool 5*/
	/*short isEnabled();//查询数据传输模式 换int */
	/*short isNetWorkAvailable();//网络是否可用*/
	boolean isEnabled();//查询数据传输模式 换int 
	boolean isNetWorkAvailable();//网络是否可用
	void addDataRecvListener(DataCallback callback);//添加接收数据回调监听器    
	void addNetWorkStatusListener(NetWorkCallback callback);//添加网络状态回调监听器
	void addPcmVoiceListener(PcmCallback callback);//添加接收话音回调监听器

///////所有接口如下:
///////////////////////////////////////////////////////////////////////// 

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
};

callback DataCallback = void (DOMString addr, DOMString data);
callback NetWorkCallback = void (long statusType, DOMString pParam);
callback PcmCallback = void (long statusType, DOMString pParam);

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
/*
 *
 *[>void 8<]
 *    void init(DOMString paras);//初始化自组网
 *    void tellLocalIpAddr(DOMString ipAddr);//设置本机自组网ip
 *    void queryNetWorkStatus(DOMString cmd);//查询网络状态
 *    void setParameters(DOMString paras);//设置自组网参数
 *    void setPttState (DOMString state);//设置ptt状态
 *    void sendBDMsg2AdHoc(DOMString  gga);//发送北斗消息到自组网
 *    void sendExtraData (DOMString type, DOMString data);//执行shell命令
 *    void setIsNeedHeadForUserData(boolean isNeedHead);//设置用户数据是否需要头部 TODO
 *
 *
 *    [>int 10<]
 *    void openAdHocDevice();//打开自组网设备 0:打开成功, 非0:打开失败
 *    void closeAdHocDevice();//关闭自组网设备
 *    void reOpenAdHocDevice();//重打开自组网设备
 *    void setEnabled (boolean enabled);//设置数据传输模式，ip或非ip.true :非ip模式，false :ip模式
 *    void sendData( DOMString data);//发送数据
 *    void sendData(DOMString srcAddr, DOMString desAddr, DOMString data);//发送数据
 *    void sendDataPri(DOMString srcAddr,DOMString desAddr,DOMString data, short pri);//发送数据 TODO
 *    void sendPcmVoice(DOMString voiceData);//发送话音数据
 *    void setEthernetIP(DOMString ipAddr);//设置以太网ip
 *    void updateModem(DOMString fileName, DOMString data);//升级modem
 *
 *
 *[>diff 2<]
 *    [>ConfigParams getConfigParams();//获取平台信息<]
 *    [>char* getEthernetIP();//查询以太网ip TODO<]
 *    void getEthernetIP();//查询以太网ip
 *
 *[>bool 5<]
 *    boolean isEnabled();//查询数据传输模式 
 *    boolean isNetWorkAvailable();//网络是否可用
 *    void addDataRecvListener(DataCallback callback);//添加接收数据回调监听器    
 *    void addNetWorkStatusListener(NetWorkCallback callback);//添加网络状态回调监听器
 *    void addPcmVoiceListener(PcmCallback callback);//添加接收话音回调监听器
 *
 *
 */
