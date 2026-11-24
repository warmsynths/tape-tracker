#pragma once
#include <stdint.h>
#include <stddef.h>
enum { controlIdSimpleBox = 1 };
typedef int control_id_t;
struct strFont { uint8_t data[148]; uint32_t size; int16_t handle; uint32_t address; int32_t source; uint16_t width; uint16_t height; uint8_t format; uint16_t linestride; };
struct strControlProperties { const char* text=nullptr; int value=0; void* data=nullptr; uint32_t style=0; const uint32_t* colors=nullptr; uint16_t x=0,y=0,w=0,h=0; };
void mock_text(int x,int y,const char* s,int len,int font);
class cDisplayControl {
 public:
  virtual void update()=0; virtual void setStyle(uint32_t)=0; virtual void setText(const char*)=0; virtual void setValue(int)=0; virtual void setColors(const uint32_t*)=0; virtual void setData(void*)=0;
  cDisplayControl(control_id_t,uint8_t){} virtual ~cDisplayControl(){}
 protected:
  uint32_t style=0; const char* text=nullptr; const strFont* font=nullptr; int value=0; uint16_t posX=0,posY=0,width=0,height=0; const uint32_t* colors=nullptr;
  void string2Bitmaps(int16_t x,int16_t y,const char* s,int8_t len,bool){ mock_text(x,y,s,len,font->handle); }
};
