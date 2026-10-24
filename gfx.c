#include "kernel.h"

int scr_w, scr_h;
u32 *back;
static u32 *target;
static u32 *lfb;
static int lfb_pitch;            /* v bajtech */
static const u8 *font;
static int cx0, cy0, cx1, cy1;

void gfx_init(void){
    u8 *mi = (u8 *)0x600;
    lfb_pitch = *(u16 *)(mi + 0x10);
    scr_w = *(u16 *)(mi + 0x12);
    scr_h = *(u16 *)(mi + 0x14);
    lfb = (u32 *)(*(u32 *)(mi + 0x28));
    back = (u32 *)0x400000;
    target = back;
    font = (const u8 *)0x9000;
    gfx_noclip();
}
u32 *gfx_wall(void){ return (u32 *)0x800000; }
u32 *gfx_set_target(u32 *t){ u32 *o = target; target = t; back = t; return o; }

void gfx_clip(int x, int y, int w, int h){
    cx0 = x < 0 ? 0 : x; cy0 = y < 0 ? 0 : y;
    cx1 = x + w > scr_w ? scr_w : x + w; cy1 = y + h > scr_h ? scr_h : y + h;
}
void gfx_noclip(void){ cx0 = 0; cy0 = 0; cx1 = scr_w; cy1 = scr_h; }

void put_pixel(int x, int y, u32 c){
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) target[y * scr_w + x] = c;
}
void fill_rect(int x, int y, int w, int h, u32 c){
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) fill32(target + yy * scr_w + x0, c, x1 - x0);
}
void rect_outline(int x, int y, int w, int h, u32 c){
    fill_rect(x, y, w, 1, c); fill_rect(x, y + h - 1, w, 1, c);
    fill_rect(x, y, 1, h, c); fill_rect(x + w - 1, y, 1, h, c);
}
static inline u32 blend(u32 d, u32 s, u32 a){
    u32 rb = (((s & 0xFF00FF) * a + (d & 0xFF00FF) * (256 - a)) >> 8) & 0xFF00FF;
    u32 g  = (((s & 0x00FF00) * a + (d & 0x00FF00) * (256 - a)) >> 8) & 0x00FF00;
    return rb | g;
}
void fill_rect_alpha(int x, int y, int w, int h, u32 c, int a){
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = target + yy * scr_w;
        for (int xx = x0; xx < x1; xx++) p[xx] = blend(p[xx], c, (u32)a);
    }
}
void fill_rrect(int x, int y, int w, int h, int r, u32 c){
    fill_rect(x + r, y, w - 2 * r, h, c);
    fill_rect(x, y + r, r, h - 2 * r, c);
    fill_rect(x + w - r, y + r, r, h - 2 * r, c);
    for (int dy = 0; dy < r; dy++) for (int dx = 0; dx < r; dx++) {
        int ex = r - 1 - dx, ey = r - 1 - dy;
        if (ex * ex + ey * ey <= r * r - r / 2) {
            put_pixel(x + dx, y + dy, c); put_pixel(x + w - 1 - dx, y + dy, c);
            put_pixel(x + dx, y + h - 1 - dy, c); put_pixel(x + w - 1 - dx, y + h - 1 - dy, c);
        }
    }
}
void draw_line(int x0, int y0, int x1, int y1, u32 c){
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        put_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static int isqrt(int v){ int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
void fill_circle(int cx, int cy, int r, u32 c){
    for (int dy = -r; dy <= r; dy++) { int dx = isqrt(r * r - dy * dy); fill_rect(cx - dx, cy + dy, 2 * dx + 1, 1, c); }
}
void draw_circle(int cx, int cy, int r, u32 c){
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
        put_pixel(cx + x, cy + y, c); put_pixel(cx - x, cy + y, c); put_pixel(cx + x, cy - y, c); put_pixel(cx - x, cy - y, c);
        put_pixel(cx + y, cy + x, c); put_pixel(cx - y, cy + x, c); put_pixel(cx + y, cy - x, c); put_pixel(cx - y, cy - x, c);
        y++;
        if (e < 0) e += 2 * y + 1; else { x--; e += 2 * (y - x) + 1; }
    }
}

void draw_char(int x, int y, int ch, u32 fg){
    if (x + 8 <= cx0 || x >= cx1 || y + 16 <= cy0 || y >= cy1) return;
    const u8 *g = font + (ch & 255) * 16;
    for (int r = 0; r < 16; r++) {
        u8 bits = g[r]; int yy = y + r;
        if (!bits || yy < cy0 || yy >= cy1) continue;
        for (int b = 0; b < 8; b++) if (bits & (0x80 >> b)) {
            int xx = x + b;
            if (xx >= cx0 && xx < cx1) target[yy * scr_w + xx] = fg;
        }
    }
}
void draw_text(int x, int y, const char *s, u32 fg){ for (; *s; s++, x += 8) draw_char(x, y, (u8)*s, fg); }
int text_w(const char *s){ return (int)strlen(s) * 8; }
void draw_text_center(int cx, int y, const char *s, u32 fg){ draw_text(cx - text_w(s) / 2, y, s, fg); }
void draw_text_scaled(int x, int y, const char *s, u32 fg, int sc){
    for (; *s; s++, x += 8 * sc) {
        const u8 *g = font + ((u8)*s) * 16;
        for (int r = 0; r < 16; r++) for (int b = 0; b < 8; b++)
            if (g[r] & (0x80 >> b)) fill_rect(x + b * sc, y + r * sc, sc, sc, fg);
    }
}
void gfx_vgradient(int x, int y, int w, int h, u32 c1, u32 c2){
    for (int i = 0; i < h; i++) {
        int a = h > 1 ? (i * 256) / (h - 1) : 0;
        fill_rect(x, y + i, w, 1, blend(c1, c2, (u32)a));
    }
}
void gfx_draw_indexed(int x, int y, int w, int h, const u8 *src, int pitch, const u32 *pal){
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        const u8 *s = src + (yy - y) * pitch + (x0 - x);
        u32 *d = target + yy * scr_w + x0;
        for (int xx = x0; xx < x1; xx++) *d++ = pal[*s++];
    }
}

void gfx_present(int x, int y, int w, int h){
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > scr_w ? scr_w : x + w, y1 = y + h > scr_h ? scr_h : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++)
        copy32((u32 *)((u8 *)lfb + yy * lfb_pitch) + x0, back + yy * scr_w + x0, x1 - x0);
}
void gfx_present_all(void){ gfx_present(0, 0, scr_w, scr_h); }

static const char *cur_art[CUR_H] = {
"X           ","XX          ","X.X         ","X..X        ","X...X       ","X....X      ",
"X.....X     ","X......X    ","X.......X   ","X........X  ","X.....XXXXX ","X..X..X     ",
"X.X X..X    ","XX  X..X    ","X    X..X   ","     X..X   ","      X..X  ","      X..X  ","       XX   " };

void cursor_draw(int x, int y){
    for (int r = 0; r < CUR_H; r++) {
        int yy = y + r; if (yy < 0 || yy >= scr_h) continue;
        u32 *row = (u32 *)((u8 *)lfb + yy * lfb_pitch);
        for (int c = 0; cur_art[r][c]; c++) {
            char ch = cur_art[r][c]; int xx = x + c;
            if (ch == ' ' || xx < 0 || xx >= scr_w) continue;
            row[xx] = ch == 'X' ? 0x000000 : 0xFFFFFF;
        }
    }
}
void cursor_erase(int x, int y){ gfx_present(x, y, CUR_W, CUR_H); }
