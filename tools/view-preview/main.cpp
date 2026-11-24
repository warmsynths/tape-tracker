#include "display.h"
#include "FT812.h"
#include "fourTrackView.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
DisplayMock display; strFont fonts[5];
Theme& DisplayMock::getTheme(){ return const_cast<Theme&>(themeDefault); }
static FILE* out; static uint32_t col; static int prim; static int vx[4096], vy[4096]; static int nv; static int psize;
void API_COLOR(uint32_t c){col=c;} void API_POINT_SIZE(uint16_t s){psize=s;} void API_BEGIN(uint8_t p){prim=p;nv=0;}
void API_VERTEX2F(int16_t x,int16_t y){ vx[nv]=x; vy[nv]=y; nv++; }
void API_END(void){ for(int i=0;i+1<nv && prim!=FTPOINTS;i+=2) fprintf(out,"%s %d %d %d %d %u\n", prim==RECTS?"R":"L", vx[i],vy[i],vx[i+1],vy[i+1],col);
  if(prim==FTPOINTS) for(int i=0;i<nv;i++) fprintf(out,"P %d %d %d %u\n",vx[i],vy[i],psize,col); nv=0; }
void mock_text(int x,int y,const char* s,int len,int font){ fprintf(out,"T %d %d %d %u ",x,y,font,col); fwrite(s,1,len,out); fprintf(out,"\n"); }
int main(int argc,char**argv){
  out=fopen(argv[1],"w");
  const int widths[5]={9,10,9,10,7}; for(int i=0;i<5;i++){fonts[i].width=widths[i]; fonts[i].handle=i;}
  static strFtView v; memset(&v,0,sizeof v);
  int scenario = argc>2?atoi(argv[2]):0;
  strcpy(v.project,"Four-Track"); strcpy(v.tempo,"Free"); strcpy(v.bars,"-"); strcpy(v.pos,"0:03.3"); strcpy(v.input,"LINE"); v.monitor=true;
  const char* srcs[4]={"LINE","PADS","MIC LOW","LINE"};
  for(int i=0;i<4;i++){ snprintf(v.lane[i].name,12,"Track %d",i+1); strcpy(v.lane[i].tag,srcs[i]); v.lane[i].meter=40+i*15; }
  if(scenario==0){ // stopped with parts on 1..3
    strcpy(v.status,"Playing"); v.statusKind=1; v.loopFrames=441180; v.posFrames=150000; v.barCount=0;
    for(int i=0;i<3;i++){ v.lane[i].hasPart=true; v.lane[i].waveCount=120; for(int c=0;c<120;c++) v.lane[i].wave[c]=(uint8_t)(40+ (c*7+i*50)%200); }
    v.lane[0].selected=true; v.lane[1].glitch=true; v.lane[2].muted=true; strcpy(v.lane[2].info,"Offset +522 ms");
    strcpy(v.lane[3].msg2,"Empty");
    strcpy(v.hint,"Rec arms Track 1 from LINE. Press the track again to change its source.");
    strcpy(v.loopMode,"Repeat"); strcpy(v.padsMode,"KEYS");
  } else { // recording first take
    strcpy(v.status,"Recording"); v.statusKind=2; strcpy(v.statusNote,"No Loop yet"); v.lane[0].active=true; v.lane[0].selected=true;
    strcpy(v.lane[0].msg1,"First take - elapsed"); strcpy(v.lane[0].msg2,"0:03.3"); for(int i=1;i<4;i++) strcpy(v.lane[i].msg2,"Empty");
    strcpy(v.hint,"Stop ends the take and keeps it."); strcpy(v.loopMode,"Repeat"); strcpy(v.padsMode,"PAD");
  }
  const char* names[8]={"Rec","Stop","Play","Pads:Keys","Track 1","Track 2","Track 3","Track 4"};
  for(int b=0;b<8;b++){ strcpy(v.btn[b],names[b]); v.btnKind[b]= b==0?1:b==1?2:b==2?3:0; }
  v.btnSelected=4;
  strControlProperties p; p.data=&v; p.value=0; cFourTrackView a(&p); p.value=1; cFourTrackView b(&p);
  a.update(); b.update(); fclose(out); return 0; }
