#pragma once
#include "Arduino.h"
constexpr int WL_IDLE_STATUS=0,WL_CONNECTED=3,WL_CONNECT_FAILED=4,WL_DISCONNECTED=6,WL_NO_SHIELD=255,WIFI_STA=1;
using WiFiEvent_t=int;
struct WiFiEventInfo_t{struct{uint8_t reason=0;}wifi_sta_disconnected;};
constexpr int ARDUINO_EVENT_WIFI_STA_CONNECTED=1,ARDUINO_EVENT_WIFI_STA_GOT_IP=2,
  ARDUINO_EVENT_WIFI_STA_DISCONNECTED=3,ARDUINO_EVENT_WIFI_STA_STOP=4;
class IPAddress{public:bool fromString(const char*){return true;}String toString()const{return "192.0.2.40";}};
struct WiFiMock{
 int state=WL_CONNECTED,beginResult=WL_IDLE_STATUS,modeCalls=0,beginCalls=0,reconnectCalls=0;
 bool modeResult=true,reconnectResult=true;
 void(*eventCallback)(WiFiEvent_t,WiFiEventInfo_t)=nullptr;
 int status()const{return state;}int RSSI()const{return -46;}
 IPAddress localIP(){return IPAddress();}
 bool mode(int){++modeCalls;return modeResult;}
 void persistent(bool){}void setSleep(bool){}void setAutoReconnect(bool){}
 void onEvent(void(*cb)(WiFiEvent_t,WiFiEventInfo_t)){eventCallback=cb;}
 int begin(const char*,const char*){++beginCalls;return beginResult;}
 bool reconnect(){++reconnectCalls;return reconnectResult;}
};
static WiFiMock WiFi;
static bool probeResult=true;static uint16_t lastProbeTimeout=0;
static std::string mockMpdResponse,mockMpdCommand;
class WiFiClient{
 std::string response;size_t pos=0;
 public:
 bool connect(IPAddress,uint16_t port,uint16_t t){lastProbeTimeout=t;if(port==6600)response=mockMpdResponse;return probeResult;}
 void stop(){response.clear();pos=0;}
 int available(){return int(response.size()-pos);}
 int read(uint8_t* b,int n){int got=std::min(n,available());memcpy(b,response.data()+pos,got);pos+=got;return got;}
 int read(){return pos<response.size()?(unsigned char)response[pos++]:-1;}
 size_t print(const char* cmd){mockMpdCommand=cmd;return strlen(cmd);}
 bool connected(){return pos<response.size();}
};
