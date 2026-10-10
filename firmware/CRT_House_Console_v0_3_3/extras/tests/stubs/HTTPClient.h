#pragma once
#include "WiFi.h"
static int fakeHttpCode=200;static std::string lastURL;
class HTTPClient{public:void useHTTP10(bool){}void setReuse(bool){}void setConnectTimeout(int){}void setTimeout(int){}
 bool begin(WiFiClient&,const char* p){lastURL=p;return true;}void addHeader(const char*,const char*){}int GET(){return fakeHttpCode;}
 int getSize(){return 100;}void end(){}};
