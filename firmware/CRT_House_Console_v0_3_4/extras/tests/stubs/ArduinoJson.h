#pragma once
#include "Arduino.h"
#include <memory>
#include <map>
#define ARDUINOJSON_VERSION_MAJOR 6
struct JN {enum Type {NIL,BOOL,NUM,STR,OBJ,ARR} type=NIL;double number=0;bool boolean=false;std::string text;std::map<std::string,std::shared_ptr<JN>> obj;std::vector<std::shared_ptr<JN>> arr;};
class JV;using JsonObjectConst=JV;
class JA;
class JV{
 public:std::shared_ptr<JN> n;
 JV():n(new JN){}explicit JV(std::shared_ptr<JN> v):n(v?v:std::make_shared<JN>()){}
 JV operator[](const char* k)const{n->type=JN::OBJ;auto& v=n->obj[k];if(!v)v=std::make_shared<JN>();return JV(v);}
 JV operator[](int i)const{n->type=JN::ARR;while(i>=int(n->arr.size()))n->arr.push_back(std::make_shared<JN>());return JV(n->arr[i]);}
 JV& operator=(const char* t){n->type=JN::STR;n->text=t?t:"";return *this;}
 JV& operator=(bool b){n->type=JN::BOOL;n->boolean=b;return *this;}
 JV& operator=(int x){n->type=JN::NUM;n->number=x;return *this;}
 JV& operator=(float x){n->type=JN::NUM;n->number=x;return *this;}
 JV& operator=(double x){n->type=JN::NUM;n->number=x;return *this;}
 bool isNull()const{return n->type==JN::NIL;}
 template<class T> bool is()const;
 template<class T> T as()const;
 const char* operator|(const char* fallback)const{return n->type==JN::STR?n->text.c_str():fallback;}
 template<class T>T operator|(T fallback)const{return (n->type==JN::NUM||n->type==JN::BOOL)?as<T>():fallback;}
};
class JA{public:std::shared_ptr<JN> n;explicit JA(std::shared_ptr<JN> v):n(v){}size_t size()const{return n->arr.size();}
 struct It{std::vector<std::shared_ptr<JN>>::iterator it;JV operator*()const{return JV(*it);}It& operator++(){++it;return *this;}bool operator!=(const It& b)const{return it!=b.it;}};
 It begin()const{return {n->arr.begin()};}It end()const{return {n->arr.end()};}JV operator[](int i)const{return JV(n->arr[i]);}};
using JsonArrayConst=JA;
template<class T>inline T JV::as()const{return T(n->type==JN::BOOL?n->boolean:n->number);}
template<>inline const char* JV::as<const char*>()const{return n->text.c_str();}
template<>inline JV JV::as<JV>()const{return *this;}
template<>inline JA JV::as<JA>()const{return JA(n);}
template<class T>inline bool JV::is()const{return n->type==JN::NUM;}
template<>inline bool JV::is<bool>()const{return n->type==JN::BOOL;}
template<>inline bool JV::is<const char*>()const{return n->type==JN::STR;}
template<>inline bool JV::is<JA>()const{return n->type==JN::ARR;}
class JsonDocument:public JV{public:void clear(){n=std::make_shared<JN>();}};
class DynamicJsonDocument:public JsonDocument{public:explicit DynamicJsonDocument(size_t z):c(z){}size_t capacity()const{return c;}size_t c;};
template<size_t N>class StaticJsonDocument:public JsonDocument{};
struct DeserializationError{bool bad=false;operator bool()const{return bad;}const char* c_str()const{return "InvalidInput";}};
namespace DeserializationOption{struct Filter{explicit Filter(const JsonDocument&) {}};struct NestingLimit{explicit NestingLimit(int){}};}
static JV fakeJson;
template<class R,class...T>DeserializationError deserializeJson(DynamicJsonDocument& d,R&,T...){d.n=fakeJson.n;return {};}
