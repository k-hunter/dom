//this is interface of adhoc to gaia apps
//[CheckAnyPermissions="adhoc", AvailableIn=CertifiedApps]

[NoInterfaceObject]
interface MozAdhoc{
  [NewObject]
  /*
   *boolean isEnabled();//查询数据传输模式 
   *boolean isNetWorkAvailable();//网络是否可用
   *void openAdHocDevice();//打开自组网设备 0:打开成功, 非0:打开失败
   *void closeAdHocDevice();//关闭自组网设备
   *void reOpenAdHocDevice();//重打开自组网设备
   */

  void init(DOMString paras);//初始化自组网
  void sendData( DOMString data);//发送数据
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
  
  void setParameters(DOMString paras);//设置自组网参数
  /*
   *void setPttState (DOMString state);//设置ptt状态
   *void setIsNeedHeadForUserData(boolean isNeedHead);//设置用户数据是否需要头部
   *void setEthernetIP(DOMString ipAddr);//设置以太网ip
   *void updateModem(DOMString fileName, DOMString data);//升级modem
   */

  void addDataRecvListener(DataCallback callback);//添加接收数据回调监听器    
  void addNetWorkStatusListener(NetWorkCallback callback);//添加网络状态回调监听器
  void addPcmVoiceListener(PcmCallback callback);//添加接收话音回调监听器
  /*
   *boolean addDataRecvListener(DataRecvCallback *pFunc);//添加接收数据回调监听器    
   *boolean addNetWorkStatusListener(NetWorkStatusCallback *pFunc);//添加网络状态回调监听器
   *boolean addPcmVoiceListener(PcmVoiceCallback *pFunc);//添加接收话音回调监听器
   *ConfigParams getConfigParams();//获取平台信息
   */

};

callback DataCallback = void (DOMString addr, DOMString data);
callback NetWorkCallback = void (long statusType, DOMString pParam);
callback PcmCallback = void (long statusType, DOMString pParam);


