/* Core PICO-8 falso, so para testar o wrapper no PC (x86). Nao faz parte do build Android. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
typedef bool (*env_t)(unsigned, void *);
typedef void (*video_t)(const void *, unsigned, unsigned, size_t);
static env_t env; static video_t vid; static unsigned t;
struct info { const char *a, *b, *c; bool d, e; };
struct av { unsigned bw, bh, mw, mh; float ar; double fps, sr; };
static const uint32_t pal[16] = {0x000000,0x1D2B53,0x7E2553,0x008751,0xAB5236,0x5F574F,0xC2C3C7,0xFFF1E8,
 0xFF004D,0xFFA300,0xFFEC27,0x00E436,0x29ADFF,0x83769C,0xFF77A8,0xFFCCAA};
static uint16_t fbuf[128 * 128];
static void put(int x, int y, int c){ if(x<0||y<0||x>=128||y>=128) return; uint32_t p=pal[c];
 fbuf[y*128+x]=(uint16_t)((((p>>16)&255)>>3)<<11 | (((p>>8)&255)>>2)<<5 | ((p&255)>>3)); }
void retro_set_environment(env_t e){ env=e; int f=2; e(10,&f); }
void retro_set_video_refresh(video_t v){ vid=v; }
void retro_set_audio_sample(void*a){(void)a;} void retro_set_audio_sample_batch(void*a){(void)a;}
void retro_set_input_poll(void*a){(void)a;} void retro_set_input_state(void*a){(void)a;}
void retro_init(void){} void retro_deinit(void){} unsigned retro_api_version(void){return 1;}
void retro_get_system_info(struct info*i){ i->a="retro8"; i->b="fake"; i->c="p8|png"; i->d=true; i->e=false; }
void retro_get_system_av_info(struct av*a){ a->bw=a->bh=a->mw=a->mh=128; a->ar=1; a->fps=60; a->sr=44100; }
void retro_set_controller_port_device(unsigned a,unsigned b){(void)a;(void)b;} void retro_reset(void){}
size_t retro_serialize_size(void){return 0;} bool retro_serialize(void*a,size_t b){(void)a;(void)b;return false;}
bool retro_unserialize(const void*a,size_t b){(void)a;(void)b;return false;}
void retro_cheat_reset(void){} void retro_cheat_set(unsigned a,bool b,const char*c){(void)a;(void)b;(void)c;}
bool retro_load_game(const void*g){(void)g;return true;} bool retro_load_game_special(unsigned a,const void*b,size_t c){(void)a;(void)b;(void)c;return false;}
void retro_unload_game(void){} unsigned retro_get_region(void){return 0;}
void*retro_get_memory_data(unsigned i){(void)i;return 0;} size_t retro_get_memory_size(unsigned i){(void)i;return 0;}
void retro_run(void){
  int x,y,i; t++;
  for(i=0;i<128*128;i++) put(i%128,i/128,1);
  for(y=100;y<128;y++) for(x=0;x<128;x++) put(x,y,(x/8+y/8)&1?5:13);
  for(x=0;x<128;x+=16) for(y=0;y<4;y++) put(x+y,20+y,6);
  int px=30+(t%60); for(y=0;y<12;y++) for(x=0;x<12;x++) put(px+x,88+y,8);
  put(px+3,91,7); put(px+8,91,7);
  int ex=(int)(64+40*sinf(t*0.08f)); for(y=-7;y<=7;y++) for(x=-7;x<=7;x++) if(x*x+y*y<=49) put(ex+x,60+y,12);
  for(i=0;i<5;i++){ int cx=20+i*22, cy=44+(int)(4*sinf(t*0.1f+i)); for(y=-3;y<=3;y++)for(x=-3;x<=3;x++) if(x*x+y*y<=9) put(cx+x,cy+y,10);}
  for(x=4;x<60;x++){ put(x,6,7); put(x,7,7);} for(x=4;x<30;x++){ put(x,12,9); put(x,13,9);}
  vid(fbuf,128,128,256);
}
