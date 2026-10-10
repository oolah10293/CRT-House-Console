#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <ctime>
using std::isfinite;
class String {
 public:
  std::string s;
  String(){} String(const char* p):s(p?p:""){} String(std::string p):s(p){} String(char p):s(1,p){}
  String(int n):s(std::to_string(n)){}
  int length()const{return s.size();}const char* c_str()const{return s.c_str();}
  String& operator+=(char c){s+=c;return *this;}
  String& operator+=(const String& c){s+=c.s;return *this;}
  friend String operator+(const String& a,const String& b){return String(a.s+b.s);}
  bool operator==(const char* b)const{return s==(b?b:"");}
  String substring(int a)const{return String(a<int(s.size())?s.substr(std::max(0,a)):"");}
  String substring(int a,int b)const{return String(s.substr(std::max(0,a),std::max(0,b-a)));}
  void trim(){size_t f=s.find_first_not_of(" \t\r\n");if(f==std::string::npos){s.clear();return;} s=s.substr(f,s.find_last_not_of(" \t\r\n")-f+1);}
  int lastIndexOf(char c,int from=-1)const {auto p=s.rfind(c,from<0?std::string::npos:size_t(from));return p==std::string::npos?-1:int(p);}
  void replace(char a,char b){std::replace(s.begin(),s.end(),a,b);}
  void toUpperCase(){for(char& c:s)c=std::toupper((unsigned char)c);}
};
static uint32_t fakeNow=1000;
inline uint32_t millis(){return fakeNow;}
inline void delay(unsigned x){fakeNow+=x;}
inline void delayMicroseconds(unsigned){}
inline void pinMode(int,int){} inline void digitalWrite(int,int){}
inline int digitalRead(int){return 1;} inline int analogRead(int){return 240;}
inline void analogReadResolution(int){} inline void analogSetPinAttenuation(int,int){}
constexpr int HIGH=1,LOW=0,INPUT=0,OUTPUT=1,INPUT_PULLUP=2,ADC_11db=3;
struct SerialMock{void begin(int){}void println(const char*){}};static SerialMock Serial;
inline void configTzTime(const char* z,const char*,const char*){setenv("TZ",z,1);tzset();}
