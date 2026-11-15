/* "Frontend" de mentira: carrega o wrapper, roda N quadros e salva o ultimo em PPM. */
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint32_t *last; static unsigned lw, lh; static int stick_x, stick_y, press_a, press_r;
static bool env(unsigned c, void *d){ (void)d; return (c & 0xFFFF) == 10; }
static void vid(const void *d, unsigned w, unsigned h, size_t p){ (void)p; if (d) { last = d; lw = w; lh = h; } }
static void poll(void){}
static int16_t st(unsigned port, unsigned dev, unsigned idx, unsigned id){ (void)port;
  if (dev == 1) return (id == 8 && press_a) || (id == 7 && press_r);
  if (dev == 5 && idx == 1) return id == 0 ? stick_x : stick_y; return 0; }
int main(int argc, char **argv){
  void *h = dlopen("./retro8_voxel_libretro_android.so", RTLD_NOW); if (!h){ puts(dlerror()); return 1; }
  int frames = argc > 1 ? atoi(argv[1]) : 40; stick_x = argc > 2 ? atoi(argv[2]) : 0; stick_y = argc > 3 ? atoi(argv[3]) : 0;
  press_a = argc > 4 ? atoi(argv[4]) : 0; press_r = press_a;
  void (*se)(void*) = dlsym(h,"retro_set_environment"), (*sv)(void*) = dlsym(h,"retro_set_video_refresh");
  void (*sa)(void*) = dlsym(h,"retro_set_audio_sample"), (*sb)(void*) = dlsym(h,"retro_set_audio_sample_batch");
  void (*sp)(void*) = dlsym(h,"retro_set_input_poll"), (*ss)(void*) = dlsym(h,"retro_set_input_state");
  void (*init)(void) = dlsym(h,"retro_init"), (*run)(void) = dlsym(h,"retro_run");
  bool (*lg)(void*) = dlsym(h,"retro_load_game");
  se(env); sv(vid); sa(NULL); sb(NULL); sp(poll); ss(st); init();
  struct { const char *a,*b,*c; bool d,e; } si; void (*gi)(void*) = dlsym(h,"retro_get_system_info"); gi(&si);
  printf("core: %s %s (%s)\n", si.a, si.b, si.c);
  if (!lg(NULL)) { puts("load_game falhou"); return 1; }
  for (int i = 0; i < frames; i++) run();
  FILE *f = fopen("out.ppm","wb"); fprintf(f,"P6\n%u %u\n255\n", lw, lh);
  for (unsigned i = 0; i < lw*lh; i++){ uint32_t c = last[i]; fputc(c>>16,f); fputc(c>>8,f); fputc(c,f);} fclose(f);
  printf("ok %ux%u\n", lw, lh); return 0;
}
