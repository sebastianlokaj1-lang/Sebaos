#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed char    s8;
typedef short          s16;
typedef int            s32;
typedef u32            size_t;
#define NULL ((void*)0)

static inline void outb(u16 p, u8 v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void outw(u16 p, u16 v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline u8   inb(u16 p){ u8 v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }

/* ---------- sys.c: knihovna + ovladace ---------- */
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
void   strcpy_s(char *d, const char *s, int n);
int    ksnprintf(char *buf, int n, const char *fmt, ...);
void   copy32(u32 *d, const u32 *s, int n);
void   fill32(u32 *d, u32 v, int n);

enum { KEY_UP = 0x101, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_DEL, KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN };
typedef struct { int x, y, btn; } MouseEv;

void input_init(void);
void input_poll(void);
int  key_get(void);
int  mouse_get(MouseEv *e);
u32  millis(void);
typedef struct { int sec, min, hour, day, mon, year; } RTime;
void rtc_read(RTime *t);
void sys_reboot(void);
void sys_shutdown(void);
void cpu_vendor(char *out);
u32  rand32(void);
void cpu_brand(char *out);
static inline u32 rdtsc32(void){ u32 a; __asm__ volatile("rdtsc":"=a"(a)::"edx"); return a; }
#define PERF_N 60
extern int perf_load, perf_hist[PERF_N];
extern u32 perf_mhz, perf_seq;
void perf_work_add(u32 cycles);
void perf_update(u32 now);
void srand32(u32 s);

/* ---------- gfx.c ---------- */
extern int scr_w, scr_h;
extern u32 *back;
void gfx_init(void);
u32 *gfx_set_target(u32 *t);
u32 *gfx_wall(void);
void gfx_clip(int x, int y, int w, int h);
void gfx_noclip(void);
void put_pixel(int x, int y, u32 c);
void fill_rect(int x, int y, int w, int h, u32 c);
void rect_outline(int x, int y, int w, int h, u32 c);
void fill_rect_alpha(int x, int y, int w, int h, u32 c, int a);
void fill_rrect(int x, int y, int w, int h, int r, u32 c);
void draw_line(int x0, int y0, int x1, int y1, u32 c);
void fill_circle(int cx, int cy, int r, u32 c);
void draw_circle(int cx, int cy, int r, u32 c);
void draw_char(int x, int y, int ch, u32 fg);
void draw_text(int x, int y, const char *s, u32 fg);
void draw_text_scaled(int x, int y, const char *s, u32 fg, int sc);
void draw_text_center(int cx, int y, const char *s, u32 fg);
int  text_w(const char *s);
void gfx_vgradient(int x, int y, int w, int h, u32 c1, u32 c2);
void gfx_draw_indexed(int x, int y, int w, int h, const u8 *src, int pitch, const u32 *pal);
void gfx_present(int x, int y, int w, int h);
void gfx_present_all(void);
void cursor_draw(int x, int y);
void cursor_erase(int x, int y);
#define CUR_W 12
#define CUR_H 19

/* ---------- gui.c / apps.c ---------- */
#define MAXWIN   10
#define TITLE_H  24
#define TASKBAR_H 32
#define EV_DOWN 1
#define EV_UP   2
#define EV_DRAG 3

typedef struct { int len, cur, top, left, blink, fileref; } NpState;
typedef struct { char disp[32]; double acc; char op; int typing, err, pressed; } CalcState;
typedef struct { int tool, color, size, lx, ly, drawing; } PaintState;
typedef struct { int last_sec; u32 last; } ClockState;
typedef struct { u8 sx[432], sy[432]; int len, dx, dy, ndx, ndy, fx, fy, score, best, state; u32 last; } SnakeState;
typedef struct { int nlines, scroll, inlen, blink, started; char in[100]; char last[100]; } TermState;
typedef struct { u32 last; } AboutState;
typedef struct { int tab, sel; u32 seq; } TmState;
typedef union { NpState np; CalcState calc; PaintState paint; ClockState clock; SnakeState snake; TermState term; AboutState about; TmState tm; } AppData;

struct Window;
typedef struct App {
    const char *id;       /* kratky nazev pro prikaz "open" */
    const char *title;    /* nazev okna / ikony */
    int cw, ch;           /* vychozi velikost klientske oblasti */
    int resizable;
    u32 color;
    void (*init)(struct Window *);
    void (*draw)(struct Window *);
    void (*mouse)(struct Window *, int x, int y, int ev);
    void (*key)(struct Window *, int key);
    void (*tick)(struct Window *, u32 ms);
    void (*icon)(int x, int y);
} App;

typedef struct Window {
    int used, slot;
    int x, y, w, h;
    int minimized, maximized, rx, ry, rw, rh;
    const App *app;
    char title[40];
    AppData d;
} Window;

#define CLX(w) ((w)->x + 1)
#define CLY(w) ((w)->y + TITLE_H + 1)
#define CLW(w) ((w)->w - 2)
#define CLH(w) ((w)->h - TITLE_H - 2)

extern const App app_table[];
extern const int app_count;
void  gui_init(void);
void  gui_dirty(void);
int   gui_is_focused(Window *w);
Window *gui_open(int app);
void  gui_close(Window *w);
void  gui_mouse(int x, int y, int btn);
void  gui_key(int k);
void  gui_tick(u32 now);
int   gui_need_redraw(void);
void  gui_render(void);
void *win_mem(Window *w);
#define WIN_MEM_SIZE 0x40000
int   app_find(const char *name);
int   app_memory(Window *w);              /* KB */
Window *gui_win(int slot);
Window *gui_open_file(int idx);
extern char __bss_end[];
typedef struct { u32 total, reserved, kernel, gfx, apps, docs, used, freekb; } MemInfo;
void  mem_info(MemInfo *m);
/* dokumenty v RAM */
#define MAXFILES 16
#define FILE_MAX 60000
int   file_new(void);
void  file_delete(int i);
int   file_count(void);
int   file_nth(int k);
const char *file_name(int i);
char *file_data(int i);
int   file_len(int i);
void  file_set_len(int i, int len);

/* palety / barvy */
#define COL_WHITE 0xFFFFFF
#define COL_BLACK 0x000000
#define COL_PANEL 0xF0F0F2
#define COL_ACCENT 0x2D6CDF

#endif
