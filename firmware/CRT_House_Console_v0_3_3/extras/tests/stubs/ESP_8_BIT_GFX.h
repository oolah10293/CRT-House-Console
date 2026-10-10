#pragma once
#include "Arduino.h"
struct TextRecord{int x,y,size;uint16_t color;std::string text;};
static std::vector<TextRecord> drawnText;
class Adafruit_GFX {
 public:
  Adafruit_GFX(int w,int h):_width(w),_height(h){}
  virtual ~Adafruit_GFX(){}
  virtual void drawPixel(int16_t,int16_t,uint16_t)=0;
  virtual void drawFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c){for(int i=0;i<w;i++)drawPixel(x+i,y,c);}
  virtual void drawFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c){for(int i=0;i<h;i++)drawPixel(x,y+i,c);}
  virtual void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){for(int j=0;j<h;j++)drawFastHLine(x,y+j,w,c);}
  int width()const{return _width;}int height()const{return _height;}
  void fillScreen(uint16_t c){fillRect(0,0,_width,_height,c);}
  void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
  void drawLine(int x0,int y0,int x1,int y1,uint16_t c){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;
    for(;;){drawPixel(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}
  }
  void fillTriangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){
    int minx=std::min({x0,x1,x2}),maxx=std::max({x0,x1,x2});int miny=std::min({y0,y1,y2}),maxy=std::max({y0,y1,y2});
    for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){int a=(x-x0)*(y1-y0)-(y-y0)*(x1-x0),b=(x-x1)*(y2-y1)-(y-y1)*(x2-x1),d=(x-x2)*(y0-y2)-(y-y2)*(x0-x2);
      if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))drawPixel(x,y,c);}
  }
  void setTextWrap(bool){}void setTextSize(uint8_t v){sz=v;}void setTextColor(uint16_t v){col=v;}void setCursor(int x,int y){cx=x;cy=y;}
  void print(const String& t){drawnText.push_back({cx,cy,sz,col,t.s});for(char ch:t.s){if(ch!=' ')fillRect(cx,cy,5*sz,7*sz,col);cx+=6*sz;}}
 protected:int16_t _width,_height;int cx=0,cy=0,sz=1;uint16_t col=255;
};
class ESP_8_BIT_GFX:public Adafruit_GFX{
 public:std::vector<uint8_t> pixels;
  ESP_8_BIT_GFX(bool,int):Adafruit_GFX(256,240),pixels(256*240){}
  void begin(){}void waitForFrame(){}
  void drawPixel(int16_t x,int16_t y,uint16_t c)override{if(x>=0&&x<256&&y>=0&&y<240)pixels[y*256+x]=c;}
};
