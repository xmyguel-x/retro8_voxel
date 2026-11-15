/*
 * Retro8 Voxel 3D  -  core "wrapper" para RetroArch
 *
 * Carrega o core original retro8_libretro_android.so (PICO-8) da mesma pasta,
 * roda o jogo normalmente (audio, input, save states ficam intactos) e em vez de
 * mostrar a tela 2D, desenha uma carcaça de videogame 3D com a tela embutida,
 * e dentro dela cada pixel do jogo vira uma coluna voxel.
 *
 * Renderizador 100% software (sem OpenGL), saida XRGB8888 640x480.
 *
 * Controles extras:  analogico direito = girar a camera | L3 = alterna voxel/plano
 *                    R3 = reseta a camera
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API __attribute__((visibility("default")))

/* ---------- tipos minimos do libretro ---------- */
struct retro_game_info { const char *path; const void *data; size_t size; const char *meta; };
struct retro_system_info { const char *library_name, *library_version, *valid_extensions; bool need_fullpath, block_extract; };
struct retro_game_geometry { unsigned base_width, base_height, max_width, max_height; float aspect_ratio; };
struct retro_system_timing { double fps, sample_rate; };
struct retro_system_av_info { struct retro_game_geometry geometry; struct retro_system_timing timing; };
struct retro_message { const char *msg; unsigned frames; };
typedef bool (*env_t)(unsigned, void *);
typedef void (*video_t)(const void *, unsigned, unsigned, size_t);
typedef void (*audio_t)(int16_t, int16_t);
typedef size_t (*audiob_t)(const int16_t *, size_t);
typedef void (*poll_t)(void);
typedef int16_t (*state_t)(unsigned, unsigned, unsigned, unsigned);

#define OUT_W 640
#define OUT_H 480
#define MAXG 256

/* ---------- core interno ---------- */
static void *g_lib;
static bool g_ok;
static struct {
    void (*init)(void); void (*deinit)(void); unsigned (*api_version)(void);
    void (*get_system_info)(struct retro_system_info *);
    void (*get_system_av_info)(struct retro_system_av_info *);
    void (*set_environment)(env_t); void (*set_video_refresh)(video_t);
    void (*set_audio_sample)(audio_t); void (*set_audio_sample_batch)(audiob_t);
    void (*set_input_poll)(poll_t); void (*set_input_state)(state_t);
    void (*set_controller_port_device)(unsigned, unsigned);
    void (*reset)(void); void (*run)(void);
    size_t (*serialize_size)(void); bool (*serialize)(void *, size_t); bool (*unserialize)(const void *, size_t);
    void (*cheat_reset)(void); void (*cheat_set)(unsigned, bool, const char *);
    bool (*load_game)(const struct retro_game_info *);
    bool (*load_game_special)(unsigned, const struct retro_game_info *, size_t);
    void (*unload_game)(void); unsigned (*get_region)(void);
    void *(*get_memory_data)(unsigned); size_t (*get_memory_size)(unsigned);
} in;

static env_t g_env; static video_t g_video; static state_t g_state;
static int g_infmt = 2; /* 0=0RGB1555 1=XRGB8888 2=RGB565 */

static bool load_inner(void)
{
    static const char *names[] = { "retro8_libretro_android.so", "retro8_inner_libretro_android.so", "retro8_libretro.so", NULL };
    char dir[1024] = "", path[1100];
    Dl_info di;
    int i, miss = 0;
    if (g_lib) return g_ok;
    if (dladdr((void *)&load_inner, &di) && di.dli_fname) {
        const char *s = strrchr(di.dli_fname, '/');
        if (s) { size_t n = (size_t)(s - di.dli_fname + 1); if (n < sizeof dir) { memcpy(dir, di.dli_fname, n); dir[n] = 0; } }
    }
    for (i = 0; names[i] && !g_lib; i++) {
        snprintf(path, sizeof path, "%s%s", dir, names[i]);
        g_lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    }
    if (!g_lib) return false;
#define L(n) do { *(void **)&in.n = dlsym(g_lib, "retro_" #n); if (!in.n) miss++; } while (0)
    L(init); L(deinit); L(api_version); L(get_system_info); L(get_system_av_info);
    L(set_environment); L(set_video_refresh); L(set_audio_sample); L(set_audio_sample_batch);
    L(set_input_poll); L(set_input_state); L(set_controller_port_device); L(reset); L(run);
    L(serialize_size); L(serialize); L(unserialize); L(cheat_reset); L(cheat_set);
    L(load_game); L(load_game_special); L(unload_game); L(get_region); L(get_memory_data); L(get_memory_size);
#undef L
    g_ok = (miss == 0);
    return g_ok;
}

/* ---------- frame capturado do jogo ---------- */
static uint32_t g_px[MAXG * MAXG];
static float g_h[MAXG * MAXG];
static int g_gw, g_gh;

static void video_proxy(const void *data, unsigned w, unsigned h, size_t pitch)
{
    unsigned x, y;
    if (!data || !w || !h || w > MAXG || h > MAXG) return;
    g_gw = (int)w; g_gh = (int)h;
    for (y = 0; y < h; y++) {
        const uint8_t *row = (const uint8_t *)data + y * pitch;
        for (x = 0; x < w; x++) {
            uint32_t c;
            if (g_infmt == 1) c = ((const uint32_t *)row)[x] & 0xFFFFFF;
            else if (g_infmt == 2) {
                uint32_t v = ((const uint16_t *)row)[x];
                uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
                c = (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
            } else {
                uint32_t v = ((const uint16_t *)row)[x];
                uint32_t r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
                c = (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
            }
            g_px[y * MAXG + x] = c;
        }
    }
}

static bool env_proxy(unsigned cmd, void *data)
{
    switch (cmd & 0xFFFF) {
    case 10: if (data) g_infmt = *(int *)data; return true;     /* formato de pixel do jogo: guardamos */
    case 32: case 37: return true;                              /* av_info/geometry: ignorados (saida fixa) */
    case 14: case 40: case 41: return false;                    /* sem hw render / framebuffer do frontend */
    }
    return g_env ? g_env(cmd, data) : false;
}

/* ---------- rasterizador software ---------- */
static uint32_t fb[OUT_W * OUT_H];
static float zb[OUT_W * OUT_H];
typedef struct { float x, y, w; } V;
static float g_cy = 1, g_sy = 0, g_cp = 1, g_sp = 0;
static float cam_x, cam_y, cam_z;
#define CAMD 340.0f
#define FOCAL 760.0f
#define YOFF -6.0f

static inline V proj(float x, float y, float z)
{
    float x1 = x * g_cy + z * g_sy, z1 = -x * g_sy + z * g_cy;
    float y2 = y * g_cp + z1 * g_sp, z2 = -y * g_sp + z1 * g_cp;
    float iw = 1.0f / (CAMD - z2);
    V v = { OUT_W * 0.5f + FOCAL * x1 * iw, OUT_H * 0.5f - FOCAL * y2 * iw + YOFF, iw };
    return v;
}

static void tri(V a, V b, V c, uint32_t col)
{
    float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    int x0, x1, y0, y1, x, y;
    float inv;
    if (area == 0) return;
    if (area > 0) { V t = b; b = c; c = t; area = -area; }
    x0 = (int)floorf(fminf(a.x, fminf(b.x, c.x))); x1 = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x)));
    y0 = (int)floorf(fminf(a.y, fminf(b.y, c.y))); y1 = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > OUT_W - 1) x1 = OUT_W - 1; if (y1 > OUT_H - 1) y1 = OUT_H - 1;
    inv = 1.0f / area;
    for (y = y0; y <= y1; y++) {
        float py = y + 0.5f;
        for (x = x0; x <= x1; x++) {
            float px = x + 0.5f;
            float w0 = (c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x);
            float w1 = (a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x);
            float w2 = (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
            float iw;
            if (w0 > 0 || w1 > 0 || w2 > 0) continue;
            iw = (w0 * a.w + w1 * b.w + w2 * c.w) * inv;
            if (iw > zb[y * OUT_W + x]) { zb[y * OUT_W + x] = iw; fb[y * OUT_W + x] = col; }
        }
    }
}

static inline uint32_t shade(uint32_t c, float k)
{
    int r = (int)(((c >> 16) & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)((c & 255) * k);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void quad(float ax, float ay, float az, float bx, float by, float bz,
                 float cx, float cy, float cz, float dx, float dy, float dz, uint32_t col)
{
    V a = proj(ax, ay, az), b = proj(bx, by, bz), c = proj(cx, cy, cz), d = proj(dx, dy, dz);
    tri(a, b, c, col); tri(a, c, d, col);
}

/* f: 0=+z 1=+x 2=-x 3=+y 4=-y  (vertices anti-horario visto de fora) */
static void face(int f, float x0, float y0, float z0, float x1, float y1, float z1, uint32_t c)
{
    switch (f) {
    case 0: quad(x0,y0,z1, x1,y0,z1, x1,y1,z1, x0,y1,z1, shade(c, 1.00f)); break;
    case 1: quad(x1,y0,z1, x1,y0,z0, x1,y1,z0, x1,y1,z1, shade(c, 0.60f)); break;
    case 2: quad(x0,y0,z0, x0,y0,z1, x0,y1,z1, x0,y1,z0, shade(c, 0.80f)); break;
    case 3: quad(x0,y1,z1, x1,y1,z1, x1,y1,z0, x0,y1,z0, shade(c, 0.90f)); break;
    case 4: quad(x0,y0,z0, x1,y0,z0, x1,y0,z1, x0,y0,z1, shade(c, 0.50f)); break;
    }
}

static void box(float x0, float y0, float z0, float x1, float y1, float z1, uint32_t c)
{
    if (cam_z > z1) face(0, x0,y0,z0, x1,y1,z1, c);
    if (cam_x > x1) face(1, x0,y0,z0, x1,y1,z1, c);
    if (cam_x < x0) face(2, x0,y0,z0, x1,y1,z1, c);
    if (cam_y > y1) face(3, x0,y0,z0, x1,y1,z1, c);
    if (cam_y < y0) face(4, x0,y0,z0, x1,y1,z1, c);
}

/* ---------- estado da cena / input ---------- */
static float g_yaw, g_pitch = 30.0f;   /* graus */
static bool g_flat;
static float g_press[16];
static bool g_l3_prev, g_r3_prev;
static unsigned g_frame;

#define ZF (-3.0f)          /* piso da tela (rebaixado na carcaca) */
#define SCREEN 90.0f        /* tamanho da tela em unidades do mundo */

static void update_input(void)
{
    int i;
    float ax = 0, ay = 0;
    bool l3 = false, r3 = false;
    if (g_state) {
        for (i = 0; i < 16; i++) {
            float t = g_state(0, 1, 0, (unsigned)i) ? 1.0f : 0.0f;
            g_press[i] += (t - g_press[i]) * 0.6f;
        }
        ax = g_state(0, 5, 1, 0) / 32767.0f;
        ay = g_state(0, 5, 1, 1) / 32767.0f;
        l3 = g_state(0, 1, 0, 14) != 0; r3 = g_state(0, 1, 0, 15) != 0;
    }
    if (fabsf(ax) < 0.18f) ax = 0;
    if (fabsf(ay) < 0.18f) ay = 0;
    g_yaw += ax * 1.8f; g_pitch -= ay * 1.2f;
    if (g_yaw > 55) g_yaw = 55; if (g_yaw < -55) g_yaw = -55;
    if (g_pitch > 68) g_pitch = 68; if (g_pitch < -8) g_pitch = -8;
    if (l3 && !g_l3_prev) g_flat = !g_flat;
    if (r3 && !g_r3_prev) { g_yaw = 0; g_pitch = 30.0f; }
    g_l3_prev = l3; g_r3_prev = r3;
}

static void draw_shell(void)
{
    const uint32_t BODY = 0x6f68a8, DARK = 0x1c1c26, PAD = 0x2e2b45;
    float p;
    int i;
    /* corpo */
    box(-120, -66, -16, 120, 66, ZF - 0.6f, BODY);            /* placa de tras (fundo da tela) */
    box(-120, -66, ZF, -44.8f, 66, 0, BODY);           /* laterais com recorte da tela */
    box(44.8f, -66, ZF, 120, 66, 0, BODY);
    box(-44.8f, 44.8f, ZF, 44.8f, 66, 0, BODY);
    box(-44.8f, -66, ZF, 44.8f, -44.8f, 0, BODY);
    /* moldura da tela */
    box(-52, -52, ZF, -44.8f, 52, 1.5f, DARK);
    box(44.8f, -52, ZF, 52, 52, 1.5f, DARK);
    box(-52, 44.8f, ZF, 52, 52, 1.5f, DARK);
    box(-52, -52, ZF, 52, -44.8f, 1.5f, DARK);
    /* direcional */
    box(-89, -5, 0, -79, 5, 4, PAD);
    p = g_press[4]; box(-89, 5, 0, -79, 15, 4 - 1.6f * p, PAD);
    p = g_press[5]; box(-89, -15, 0, -79, -5, 4 - 1.6f * p, PAD);
    p = g_press[6]; box(-99, -5, 0, -89, 5, 4 - 1.6f * p, PAD);
    p = g_press[7]; box(-79, -5, 0, -69, 5, 4 - 1.6f * p, PAD);
    /* botoes A / B  (retro8: O/X) */
    p = g_press[8];  box(92, -2, 0, 106, 12, 4 - 1.6f * p, 0xd0305a);
    p = g_press[0];  box(74, -14, 0, 88, 0, 4 - 1.6f * p, 0xe0a030);
    /* start / select */
    p = g_press[2]; box(-22, -61, 0, -6, -57, 2 - 0.8f * p, PAD);
    p = g_press[3]; box(6, -61, 0, 22, -57, 2 - 0.8f * p, PAD);
    /* ombros */
    p = g_press[10]; box(-112, 66, -12, -72, 70 - 1.0f * p, -3, PAD);
    p = g_press[11]; box(72, 66, -12, 112, 70 - 1.0f * p, -3, PAD);
    /* grade do alto-falante e LED */
    for (i = 0; i < 6; i++) box(70.0f + i * 6.0f, 26, 0, 72.0f + i * 6.0f, 50, 0.7f, DARK);
    box(-112, 54, 0, -108, 58, 1.5f, 0x2ee06a);
}

static uint32_t mode_color(void)
{
    uint32_t key[64] = {0}; int cnt[64] = {0}; int used[64] = {0};
    int x, y, best = 0;
    for (y = 0; y < g_gh; y++)
        for (x = 0; x < g_gw; x++) {
            uint32_t c = g_px[y * MAXG + x];
            unsigned h = (c * 2654435761u) >> 26;
            int tries = 0;
            while (used[h] && key[h] != c && tries++ < 64) h = (h + 1) & 63;
            if (tries >= 64) continue;
            used[h] = 1; key[h] = c; cnt[h]++;
        }
    for (x = 0; x < 64; x++) if (cnt[x] > cnt[best]) best = x;
    return key[best];
}

static void draw_voxels(void)
{
    int i, j;
    float s, xl0, yt0;
    uint32_t bg;
    if (g_gw <= 0 || g_gh <= 0) return;
    s = SCREEN / (float)(g_gw > g_gh ? g_gw : g_gh);
    xl0 = -s * g_gw * 0.5f; yt0 = s * g_gh * 0.5f;
    bg = mode_color();
    for (j = 0; j < g_gh; j++)
        for (i = 0; i < g_gw; i++) {
            uint32_t c = g_px[j * MAXG + i];
            float t, lum;
            if (g_flat || c == bg) t = 0.0f;
            else {
                lum = (0.30f * ((c >> 16) & 255) + 0.59f * ((c >> 8) & 255) + 0.11f * (c & 255)) / 255.0f;
                t = 2.0f + lum * 6.5f;
            }
            g_h[j * MAXG + i] += (t - g_h[j * MAXG + i]) * 0.8f;
        }
    for (j = 0; j < g_gh; j++)
        for (i = 0; i < g_gw; i++) {
            float xl = xl0 + i * s, xr = xl + s, yt = yt0 - j * s, yb = yt - s;
            float h = g_h[j * MAXG + i], ztop = ZF + h;
            uint32_t c = g_px[j * MAXG + i];
            float nl = i > 0 ? g_h[j * MAXG + i - 1] : 0, nr = i < g_gw - 1 ? g_h[j * MAXG + i + 1] : 0;
            float nu = j > 0 ? g_h[(j - 1) * MAXG + i] : 0, nd = j < g_gh - 1 ? g_h[(j + 1) * MAXG + i] : 0;
            if (cam_z > ztop) face(0, xl, yb, ZF, xr, yt, ztop, c);
            if (cam_x < xl && nl < h) face(2, xl, yb, ZF + nl, xr, yt, ztop, c);
            if (cam_x > xr && nr < h) face(1, xl, yb, ZF + nr, xr, yt, ztop, c);
            if (cam_y > yt && nu < h) face(3, xl, yb, ZF + nu, xr, yt, ztop, c);
            if (cam_y < yb && nd < h) face(4, xl, yb, ZF + nd, xr, yt, ztop, c);
        }
}

static void render_scene(void)
{
    int x, y;
    float yaw = (g_yaw + sinf(g_frame * 0.015f) * 2.5f) * 0.0174532925f;
    float pit = g_pitch * 0.0174532925f;
    g_cy = cosf(yaw); g_sy = sinf(yaw); g_cp = cosf(pit); g_sp = sinf(pit);
    cam_x = -CAMD * g_cp * g_sy; cam_y = -CAMD * g_sp; cam_z = CAMD * g_cp * g_cy;
    for (y = 0; y < OUT_H; y++) {
        int k = 14 + (y * 22) / OUT_H;
        uint32_t c = ((uint32_t)k << 16) | ((uint32_t)(k + 2) << 8) | (uint32_t)(k + 12);
        for (x = 0; x < OUT_W; x++) { fb[y * OUT_W + x] = c; zb[y * OUT_W + x] = 0.0f; }
    }
    draw_shell();
    draw_voxels();
    g_frame++;
}

/* ---------- API libretro ---------- */
API unsigned retro_api_version(void) { return 1; }

API void retro_set_environment(env_t cb)
{
    int fmt = 1; /* XRGB8888 para o frontend */
    g_env = cb;
    if (load_inner()) in.set_environment(env_proxy);
    if (cb) cb(10, &fmt);
}
API void retro_set_video_refresh(video_t cb) { g_video = cb; if (load_inner()) in.set_video_refresh(video_proxy); }
API void retro_set_audio_sample(audio_t cb) { if (load_inner()) in.set_audio_sample(cb); }
API void retro_set_audio_sample_batch(audiob_t cb) { if (load_inner()) in.set_audio_sample_batch(cb); }
API void retro_set_input_poll(poll_t cb) { if (load_inner()) in.set_input_poll(cb); }
API void retro_set_input_state(state_t cb) { g_state = cb; if (load_inner()) in.set_input_state(cb); }

API void retro_init(void) { if (load_inner()) in.init(); }
API void retro_deinit(void) { if (g_ok) in.deinit(); }

API void retro_get_system_info(struct retro_system_info *info)
{
    static char ver[96], ext[64];
    struct retro_system_info t;
    memset(&t, 0, sizeof t);
    if (load_inner()) in.get_system_info(&t);
    snprintf(ver, sizeof ver, "%s voxel", t.library_version ? t.library_version : "1.0");
    snprintf(ext, sizeof ext, "%s", t.valid_extensions ? t.valid_extensions : "p8|png");
    info->library_name = "Retro8 Voxel 3D"; info->library_version = ver; info->valid_extensions = ext;
    info->need_fullpath = t.need_fullpath; info->block_extract = t.block_extract;
}

API void retro_get_system_av_info(struct retro_system_av_info *info)
{
    struct retro_system_av_info a;
    memset(&a, 0, sizeof a); a.timing.fps = 60.0; a.timing.sample_rate = 44100.0;
    if (g_ok) in.get_system_av_info(&a);
    info->timing = a.timing;
    info->geometry.base_width = OUT_W; info->geometry.base_height = OUT_H;
    info->geometry.max_width = OUT_W; info->geometry.max_height = OUT_H;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
}

API bool retro_load_game(const struct retro_game_info *game)
{
    if (!g_ok) {
        struct retro_message m = { "Retro8 Voxel: coloque retro8_libretro_android.so na mesma pasta de cores", 360 };
        if (g_env) g_env(6, &m);
        return false;
    }
    g_yaw = 0; g_pitch = 30.0f; g_frame = 0; g_gw = g_gh = 0;
    memset(g_h, 0, sizeof g_h);
    return in.load_game(game);
}

API void retro_run(void)
{
    if (!g_ok) return;
    in.run();
    update_input();
    render_scene();
    if (g_video) g_video(fb, OUT_W, OUT_H, OUT_W * 4);
}

API bool retro_load_game_special(unsigned t, const struct retro_game_info *g, size_t n) { return g_ok && in.load_game_special(t, g, n); }
API void retro_unload_game(void) { if (g_ok) in.unload_game(); }
API void retro_reset(void) { if (g_ok) in.reset(); }
API void retro_set_controller_port_device(unsigned p, unsigned d) { if (g_ok) in.set_controller_port_device(p, d); }
API size_t retro_serialize_size(void) { return g_ok ? in.serialize_size() : 0; }
API bool retro_serialize(void *d, size_t n) { return g_ok && in.serialize(d, n); }
API bool retro_unserialize(const void *d, size_t n) { return g_ok && in.unserialize(d, n); }
API void retro_cheat_reset(void) { if (g_ok) in.cheat_reset(); }
API void retro_cheat_set(unsigned i, bool e, const char *c) { if (g_ok) in.cheat_set(i, e, c); }
API unsigned retro_get_region(void) { return g_ok ? in.get_region() : 0; }
API void *retro_get_memory_data(unsigned id) { return g_ok ? in.get_memory_data(id) : NULL; }
API size_t retro_get_memory_size(unsigned id) { return g_ok ? in.get_memory_size(id) : 0; }
