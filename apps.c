#include "kernel.h"
#include "sintab.h"

/* =====================================================================
 *  IKONY
 * ===================================================================== */
static void ic_notes(int x, int y){
    fill_rrect(x + 6, y + 2, 28, 36, 3, 0xFFFFFF);
    fill_rect(x + 6, y + 2, 28, 8, 0xF2B632);
    for (int i = 0; i < 5; i++) fill_rect(x + 11, y + 15 + i * 5, 18, 2, 0x9AA3B5);
}
static void ic_calc(int x, int y){
    fill_rrect(x + 5, y + 2, 30, 36, 4, 0x3A4256);
    fill_rect(x + 9, y + 6, 22, 9, 0x9BE3A1);
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
        fill_rect(x + 9 + c * 8, y + 19 + r * 6, 6, 4, c == 2 ? 0xF2994A : 0xD8DCE6);
}
static void ic_paint(int x, int y){
    fill_rrect(x + 3, y + 5, 34, 30, 12, 0xE8D3A8);
    fill_circle(x + 13, y + 15, 3, 0xE74C3C); fill_circle(x + 22, y + 12, 3, 0xF1C40F);
    fill_circle(x + 29, y + 19, 3, 0x2ECC71); fill_circle(x + 13, y + 26, 3, 0x3498DB);
    fill_circle(x + 27, y + 29, 3, 0x9B59B6);
}
static void ic_clock(int x, int y){
    fill_circle(x + 20, y + 20, 18, 0x2B3245); fill_circle(x + 20, y + 20, 15, 0xFFFFFF);
    draw_line(x + 20, y + 20, x + 20, y + 9, 0); draw_line(x + 20, y + 20, x + 28, y + 24, 0);
    fill_circle(x + 20, y + 20, 2, 0xD9534F);
}
static void ic_snake(int x, int y){
    fill_rrect(x + 2, y + 2, 36, 36, 5, 0x1E3A24);
    fill_rect(x + 6, y + 24, 20, 6, 0x4CD964); fill_rect(x + 20, y + 12, 6, 18, 0x4CD964);
    fill_rect(x + 20, y + 12, 14, 6, 0x7CFC7C); fill_circle(x + 9, y + 12, 3, 0xE74C3C);
}
static void ic_term(int x, int y){
    fill_rrect(x + 2, y + 5, 36, 30, 4, 0x101418);
    draw_text(x + 6, y + 12, ">_", 0x6BE675);
}
static void ic_about(int x, int y){
    fill_circle(x + 20, y + 20, 18, COL_ACCENT);
    draw_text_scaled(x + 12, y + 4, "i", COL_WHITE, 2);
}

/* =====================================================================
 *  POZNAMKY
 * ===================================================================== */
#define NP_MAX 60000
static char *np_t(Window *w){ return (char *)win_mem(w); }
static int np_ls(const char *t, int i){ while (i > 0 && t[i - 1] != '\n') i--; return i; }
static int np_le(const char *t, int len, int i){ while (i < len && t[i] != '\n') i++; return i; }

static void np_sync(Window *w){
    NpState *s = &w->d.np;
    if (!s->fileref) return;
    int f = s->fileref - 1; char *d = file_data(f);
    memcpy(d, np_t(w), s->len); d[s->len] = 0; file_set_len(f, s->len);
}
static void np_init(Window *w){
    NpState *s = &w->d.np; char *t = np_t(w);
    const char *txt = "Vitejte v SebaOS!\nToto je jednoduchy poznamkovy blok.\nPiste na klavesnici, klikejte mysi.\n";
    strcpy_s(t, txt, 4096);
    s->len = (int)strlen(t); s->cur = s->len; s->blink = 1;
}
static void np_draw(Window *w){
    NpState *s = &w->d.np; char *t = np_t(w);
    int X = CLX(w), Y = CLY(w), W = CLW(w), H = CLH(w);
    int rows = (H - 20) / 16, cols = (W - 8) / 8;
    fill_rect(X, Y, W, H - 20, COL_WHITE);
    int line = 0, col = 0;
    for (int i = 0; i < s->cur; i++) { if (t[i] == '\n') { line++; col = 0; } else col++; }
    if (line < s->top) s->top = line;
    if (line >= s->top + rows) s->top = line - rows + 1;
    if (col < s->left) s->left = col;
    if (col >= s->left + cols) s->left = col - cols + 1;
    int ln = 0, c = 0;
    for (int i = 0; i < s->len && ln < s->top + rows; i++) {
        if (t[i] == '\n') { ln++; c = 0; continue; }
        if (ln >= s->top && c >= s->left && c < s->left + cols)
            draw_char(X + 4 + (c - s->left) * 8, Y + 2 + (ln - s->top) * 16, (u8)t[i], 0x1A1A1A);
        c++;
    }
    if (gui_is_focused(w) && s->blink) fill_rect(X + 4 + (col - s->left) * 8, Y + 2 + (line - s->top) * 16, 2, 16, COL_ACCENT);
    fill_rect(X, Y + H - 20, W, 20, 0xDADDE4);
    char b[80];
    if (s->fileref) ksnprintf(b, 80, "Radek %d, Sloupec %d  |  Znaku: %d  |  ulozeno", line + 1, col + 1, s->len);
    else ksnprintf(b, 80, "Radek %d, Sloupec %d   |   Znaku: %d", line + 1, col + 1, s->len);
    draw_text(X + 6, Y + H - 18, b, 0x333844);
}
static void np_insert(Window *w, const char *str, int n){
    NpState *s = &w->d.np; char *t = np_t(w);
    if (s->len + n >= NP_MAX) return;
    memmove(t + s->cur + n, t + s->cur, s->len - s->cur);
    memcpy(t + s->cur, str, n); s->cur += n; s->len += n; t[s->len] = 0;
}
static void np_vert(Window *w, int dir, int n){
    NpState *s = &w->d.np; char *t = np_t(w);
    int col = s->cur - np_ls(t, s->cur);
    while (n--) {
        int ls = np_ls(t, s->cur);
        if (dir < 0) {
            if (ls == 0) { s->cur = 0; break; }
            int pls = np_ls(t, ls - 1), plen = ls - 1 - pls;
            s->cur = pls + (col < plen ? col : plen);
        } else {
            int le = np_le(t, s->len, s->cur);
            if (le >= s->len) { s->cur = s->len; break; }
            int nls = le + 1, nle = np_le(t, s->len, nls);
            s->cur = nls + (col < nle - nls ? col : nle - nls);
        }
    }
}
static void np_key(Window *w, int k){
    NpState *s = &w->d.np; char *t = np_t(w); s->blink = 1;
    int rows = (CLH(w) - 20) / 16;
    if (k == '\b') { if (s->cur > 0) { memmove(t + s->cur - 1, t + s->cur, s->len - s->cur); s->cur--; s->len--; t[s->len] = 0; } }
    else if (k == KEY_DEL) { if (s->cur < s->len) { memmove(t + s->cur, t + s->cur + 1, s->len - s->cur - 1); s->len--; t[s->len] = 0; } }
    else if (k == KEY_LEFT) { if (s->cur > 0) s->cur--; }
    else if (k == KEY_RIGHT) { if (s->cur < s->len) s->cur++; }
    else if (k == KEY_HOME) s->cur = np_ls(t, s->cur);
    else if (k == KEY_END) s->cur = np_le(t, s->len, s->cur);
    else if (k == KEY_UP) np_vert(w, -1, 1);
    else if (k == KEY_DOWN) np_vert(w, 1, 1);
    else if (k == KEY_PGUP) np_vert(w, -1, rows);
    else if (k == KEY_PGDN) np_vert(w, 1, rows);
    else if (k == '\t') np_insert(w, "    ", 4);
    else if (k == '\n' || (k >= 32 && k < 127)) { char c = (char)k; np_insert(w, &c, 1); }
    np_sync(w);
}
static void np_mouse(Window *w, int x, int y, int ev){
    if (ev != EV_DOWN) return;
    NpState *s = &w->d.np; char *t = np_t(w);
    if (y > CLH(w) - 20) return;
    int line = s->top + (y - 2) / 16; if (line < 0) line = 0;
    int col = s->left + (x - 4 + 4) / 8; if (col < 0) col = 0;
    int ls = 0;
    for (int l = 0; l < line; l++) {
        int le = np_le(t, s->len, ls);
        if (le >= s->len) { s->cur = s->len; return; }
        ls = le + 1;
    }
    int le = np_le(t, s->len, ls);
    s->cur = ls + (col < le - ls ? col : le - ls); s->blink = 1;
}
static void np_tick(Window *w, u32 now){
    int b = (now / 500) & 1;
    if (b != (w->d.np.blink & 1) && gui_is_focused(w)) { w->d.np.blink = b; gui_dirty(); }
}

/* =====================================================================
 *  KALKULACKA
 * ===================================================================== */
#define CBW 58
#define CBH 42
#define CGAP 6
static const char *calc_lbl[5][4] = { {"C","+/-","%","/"}, {"7","8","9","x"}, {"4","5","6","-"}, {"1","2","3","+"}, {"0",".","\xFB","="} };
static const char calc_code[5][4] = { {'C','N','%','/'}, {'7','8','9','*'}, {'4','5','6','-'}, {'1','2','3','+'}, {'0','.','R','='} };

static double parse_num(const char *s){
    int neg = 0; if (*s == '-') { neg = 1; s++; }
    double v = 0; while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    if (*s == '.') { s++; double sc = 0.1; while (*s >= '0' && *s <= '9') { v += (*s - '0') * sc; sc *= 0.1; s++; } }
    return neg ? -v : v;
}
static void fmt_num(double v, char *out, int n){
    if (v != v) { strcpy_s(out, "Chyba", n); return; }
    int neg = 0; if (v < 0) { neg = 1; v = -v; }
    if (v >= 1e9) { strcpy_s(out, "Preteceni", n); return; }
    u32 ip = (u32)v; double fp = v - (double)ip; u32 fr = (u32)(fp * 1000000.0 + 0.5);
    if (fr >= 1000000) { ip++; fr -= 1000000; }
    char tmp[28]; int l = 0;
    if (neg && (ip || fr)) tmp[l++] = '-';
    char d[12]; int dl = 0;
    if (!ip) d[dl++] = '0';
    while (ip) { d[dl++] = '0' + ip % 10; ip /= 10; }
    while (dl) tmp[l++] = d[--dl];
    if (fr) {
        tmp[l++] = '.'; char f[6];
        for (int i = 5; i >= 0; i--) { f[i] = '0' + fr % 10; fr /= 10; }
        int e = 6; while (e > 0 && f[e - 1] == '0') e--;
        for (int i = 0; i < e; i++) tmp[l++] = f[i];
    }
    tmp[l] = 0; strcpy_s(out, tmp, n);
}
static void calc_press(CalcState *s, char c){
    if (s->err && c != 'C') return;
    int l = (int)strlen(s->disp);
    if (c >= '0' && c <= '9') {
        if (!s->typing) { s->disp[0] = 0; s->typing = 1; l = 0; }
        if (l == 1 && s->disp[0] == '0') l = 0;
        if (l < 12) { s->disp[l] = c; s->disp[l + 1] = 0; }
    } else if (c == '.') {
        if (!s->typing) { strcpy_s(s->disp, "0", 32); s->typing = 1; l = 1; }
        int has = 0; for (int i = 0; i < l; i++) if (s->disp[i] == '.') has = 1;
        if (!has && l < 12) { s->disp[l] = '.'; s->disp[l + 1] = 0; }
    } else if (c == 'C') {
        memset(s, 0, sizeof *s); strcpy_s(s->disp, "0", 32);
    } else if (c == 'B') {
        if (s->typing) { if (l > 1) s->disp[l - 1] = 0; else strcpy_s(s->disp, "0", 32); if (!strcmp(s->disp, "-")) strcpy_s(s->disp, "0", 32); }
    } else if (c == 'N') {
        if (s->typing) {
            if (s->disp[0] == '-') memmove(s->disp, s->disp + 1, l);
            else if (l < 12 && strcmp(s->disp, "0")) { memmove(s->disp + 1, s->disp, l + 1); s->disp[0] = '-'; }
        } else { fmt_num(-parse_num(s->disp), s->disp, 32); }
    } else if (c == '%') {
        fmt_num(parse_num(s->disp) / 100.0, s->disp, 32); s->typing = 0;
    } else if (c == 'R') {
        double v = parse_num(s->disp);
        if (v < 0) { s->err = 1; strcpy_s(s->disp, "Chyba", 32); }
        else { __asm__("fsqrt" : "+t"(v)); fmt_num(v, s->disp, 32); }
        s->typing = 0;
    } else {
        double b = parse_num(s->disp);
        if (s->op && s->typing) {
            switch (s->op) {
            case '+': s->acc += b; break; case '-': s->acc -= b; break; case '*': s->acc *= b; break;
            case '/': if (b == 0) s->err = 1; else s->acc /= b; break;
            }
        } else if (!s->op) s->acc = b;
        if (s->err) { strcpy_s(s->disp, "Chyba", 32); s->typing = 0; s->op = 0; return; }
        fmt_num(s->acc, s->disp, 32); s->typing = 0;
        s->op = (c == '=') ? 0 : c;
    }
}
static void calc_init(Window *w){ strcpy_s(w->d.calc.disp, "0", 32); }
static void calc_draw(Window *w){
    CalcState *s = &w->d.calc;
    int X = CLX(w), Y = CLY(w);
    fill_rrect(X + CGAP, Y + 8, 4 * CBW + 3 * CGAP, 48, 6, 0x1C2230);
    if (s->op) { char o[2] = { s->op == '*' ? 'x' : s->op, 0 }; draw_text(X + CGAP + 8, Y + 12, o, 0xF2994A); }
    int tw = text_w(s->disp) * 2;
    draw_text_scaled(X + CGAP + 4 * CBW + 3 * CGAP - 10 - tw, Y + 16, s->disp, 0x9BE3A1, 2);
    for (int r = 0; r < 5; r++) for (int c = 0; c < 4; c++) {
        int bx = X + CGAP + c * (CBW + CGAP), by = Y + 64 + r * (CBH + CGAP);
        char code = calc_code[r][c];
        u32 col = 0xFFFFFF, fg = 0x20242E;
        if (code == '/' || code == '*' || code == '-' || code == '+') { col = 0xF2994A; fg = COL_WHITE; }
        else if (code == '=') { col = COL_ACCENT; fg = COL_WHITE; }
        else if (code == 'C') { col = 0xE8786F; fg = COL_WHITE; }
        else if (code == 'N' || code == '%' || code == 'R') col = 0xD3D8E4;
        if (s->pressed == r * 4 + c + 1) col = (col >> 1) & 0x7F7F7F;
        fill_rrect(bx, by, CBW, CBH, 6, col);
        draw_text_center(bx + CBW / 2, by + (CBH - 16) / 2, calc_lbl[r][c], fg);
    }
}
static int calc_hit(int x, int y){
    int c = (x - CGAP) / (CBW + CGAP), r = (y - 64) / (CBH + CGAP);
    if (x < CGAP || y < 64 || c > 3 || r > 4) return -1;
    if ((x - CGAP) % (CBW + CGAP) >= CBW || (y - 64) % (CBH + CGAP) >= CBH) return -1;
    return r * 4 + c;
}
static void calc_mouse(Window *w, int x, int y, int ev){
    CalcState *s = &w->d.calc; int h = calc_hit(x, y);
    if (ev == EV_DOWN) s->pressed = h + 1;
    else if (ev == EV_UP) {
        if (h >= 0 && s->pressed == h + 1) calc_press(s, calc_code[h / 4][h % 4]);
        s->pressed = 0;
    } else if (ev == EV_DRAG) { if (h + 1 != s->pressed && s->pressed) s->pressed = 0; }
}
static void calc_key(Window *w, int k){
    CalcState *s = &w->d.calc;
    if ((k >= '0' && k <= '9') || k == '+' || k == '-' || k == '*' || k == '/' || k == '.' || k == '%') calc_press(s, (char)k);
    else if (k == ',') calc_press(s, '.');
    else if (k == '\n' || k == '=') calc_press(s, '=');
    else if (k == 27 || k == 'c' || k == 'C') calc_press(s, 'C');
    else if (k == '\b') calc_press(s, 'B');
}

/* =====================================================================
 *  MALOVANI
 * ===================================================================== */
#define PW 600
#define PH 400
#define PTB 52
static const u32 ppal[16] = { 0x000000,0xFFFFFF,0x7F7F7F,0xC3C3C3,0xED1C24,0xFF7F27,0xFFF200,0x22B14C,
                              0x00A2E8,0x3F48CC,0xA349A4,0xB97A57,0xFFAEC9,0x880015,0x99D9EA,0x7092BE };
static const int psizes[4] = { 1, 3, 6, 10 };
static const char *ptools[4] = { "Tuzka", "Guma", "Vypln", "Smazat" };
static int fstack[PW * PH];

static void paint_init(Window *w){
    PaintState *s = &w->d.paint; memset((u8 *)win_mem(w), 1, PW * PH);
    s->color = 0; s->size = 1; s->tool = 0;
}
static void pbtn(int i, int *x, int *y, int *bw, int *bh){
    if (i < 4) { *x = 204 + i * 64; *y = 6; *bw = 60; *bh = 20; }
    else { *x = 204 + 44 + (i - 4) * 34; *y = 30; *bw = 30; *bh = 18; }
}
static void paint_draw(Window *w){
    PaintState *s = &w->d.paint;
    int X = CLX(w), Y = CLY(w);
    fill_rect(X, Y, CLW(w), CLH(w), 0x8E95A5);
    gfx_draw_indexed(X, Y + PTB, PW, PH, (u8 *)win_mem(w), PW, ppal);
    fill_rect(X, Y, CLW(w), PTB, 0xDADDE4);
    for (int i = 0; i < 16; i++) {
        int sx = X + 6 + (i % 8) * 24, sy = Y + 6 + (i / 8) * 22;
        fill_rect(sx, sy, 20, 18, ppal[i]);
        rect_outline(sx, sy, 20, 18, i == s->color ? 0xFF3B30 : 0x555B6B);
        if (i == s->color) rect_outline(sx + 1, sy + 1, 18, 16, 0xFF3B30);
    }
    for (int i = 0; i < 8; i++) {
        int bx, by, bw, bh; pbtn(i, &bx, &by, &bw, &bh); bx += X; by += Y;
        int sel = (i < 3 && s->tool == i) || (i >= 4 && s->size == i - 4);
        fill_rrect(bx, by, bw, bh, 3, sel ? COL_ACCENT : 0xF4F5F8);
        rect_outline(bx, by, bw, bh, 0x8A91A3);
        char b[8];
        if (i < 4) strcpy_s(b, ptools[i], 8); else ksnprintf(b, 8, "%d", psizes[i - 4]);
        draw_text_center(bx + bw / 2, by + (bh - 16) / 2, b, sel ? COL_WHITE : 0x20242E);
    }
    draw_text(X + 204, Y + 32, "Sila", 0x333844);
}
static void paint_stamp(Window *w, int x, int y){
    PaintState *s = &w->d.paint; u8 *cv = (u8 *)win_mem(w);
    int r = s->tool == 1 ? (psizes[s->size] < 2 ? 2 : psizes[s->size]) : psizes[s->size] / 2;
    u8 col = s->tool == 1 ? 1 : (u8)s->color;
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy > r * r + r) continue;
        int px = x + dx, py = y + dy;
        if (px >= 0 && px < PW && py >= 0 && py < PH) cv[py * PW + px] = col;
    }
}
static void paint_fill(u8 *cv, int x, int y, u8 nc){
    u8 old = cv[y * PW + x]; if (old == nc) return;
    int sp = 0; cv[y * PW + x] = nc; fstack[sp++] = y * PW + x;
    while (sp) {
        int p = fstack[--sp], px = p % PW, py = p / PW;
        if (px > 0      && cv[p - 1]  == old) { cv[p - 1]  = nc; fstack[sp++] = p - 1; }
        if (px < PW - 1 && cv[p + 1]  == old) { cv[p + 1]  = nc; fstack[sp++] = p + 1; }
        if (py > 0      && cv[p - PW] == old) { cv[p - PW] = nc; fstack[sp++] = p - PW; }
        if (py < PH - 1 && cv[p + PW] == old) { cv[p + PW] = nc; fstack[sp++] = p + PW; }
    }
}
static void paint_mouse(Window *w, int x, int y, int ev){
    PaintState *s = &w->d.paint;
    if (ev == EV_DOWN && y < PTB) {
        for (int i = 0; i < 16; i++) {
            int sx = 6 + (i % 8) * 24, sy = 6 + (i / 8) * 22;
            if (x >= sx && x < sx + 20 && y >= sy && y < sy + 18) { s->color = i; if (s->tool == 1) s->tool = 0; }
        }
        for (int i = 0; i < 8; i++) {
            int bx, by, bw, bh; pbtn(i, &bx, &by, &bw, &bh);
            if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
                if (i < 3) s->tool = i;
                else if (i == 3) memset((u8 *)win_mem(w), 1, PW * PH);
                else s->size = i - 4;
            }
        }
        return;
    }
    int cx = x, cy = y - PTB;
    if (ev == EV_DOWN) {
        if (cx < 0 || cy < 0 || cx >= PW || cy >= PH) return;
        if (s->tool == 2) { paint_fill((u8 *)win_mem(w), cx, cy, (u8)s->color); return; }
        s->drawing = 1; s->lx = cx; s->ly = cy; paint_stamp(w, cx, cy);
    } else if (ev == EV_DRAG && s->drawing) {
        int x0 = s->lx, y0 = s->ly;
        int dx = cx > x0 ? cx - x0 : x0 - cx, sx = x0 < cx ? 1 : -1;
        int dy = cy > y0 ? y0 - cy : cy - y0, sy = y0 < cy ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            paint_stamp(w, x0, y0);
            if (x0 == cx && y0 == cy) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        s->lx = cx; s->ly = cy;
    } else if (ev == EV_UP) s->drawing = 0;
}

/* =====================================================================
 *  HODINY
 * ===================================================================== */
static int cosi(int i, int r){ return SN[(i + 15) % 60] * r / 1000; }
static int sini(int i, int r){ return SN[i % 60] * r / 1000; }
static void clock_draw(Window *w){
    (void)w;
    RTime t; rtc_read(&t);
    int X = CLX(w), Y = CLY(w), cx = X + 150, cy = Y + 150;
    fill_circle(cx, cy, 130, 0x2B3245);
    fill_circle(cx, cy, 124, 0xFAFBFD);
    for (int i = 0; i < 60; i++) {
        int len = (i % 5 == 0) ? 12 : 5, R = 118;
        draw_line(cx + sini(i, R - len), cy - cosi(i, R - len), cx + sini(i, R), cy - cosi(i, R), i % 5 == 0 ? 0x20242E : 0x8A91A3);
    }
    for (int k = 1; k <= 12; k++) {
        char b[4]; ksnprintf(b, 4, "%d", k);
        draw_text_center(cx + sini(k * 5, 92), cy - cosi(k * 5, 92) - 8, b, 0x333844);
    }
    int hi = (t.hour % 12) * 5 + t.min / 12;
    for (int d = -1; d <= 1; d++) {
        draw_line(cx + d, cy, cx + d + sini(hi, 62), cy - cosi(hi, 62), 0x20242E);
        draw_line(cx, cy + d, cx + sini(hi, 62), cy + d - cosi(hi, 62), 0x20242E);
    }
    draw_line(cx, cy, cx + sini(t.min, 92), cy - cosi(t.min, 92), 0x20242E);
    draw_line(cx + 1, cy, cx + 1 + sini(t.min, 92), cy - cosi(t.min, 92), 0x20242E);
    draw_line(cx - sini(t.sec, 18), cy + cosi(t.sec, 18), cx + sini(t.sec, 104), cy - cosi(t.sec, 104), 0xD9534F);
    fill_circle(cx, cy, 5, 0xD9534F);
    char b[24];
    ksnprintf(b, 24, "%02d:%02d:%02d", t.hour, t.min, t.sec);
    draw_text_scaled(X + 150 - text_w(b), Y + 284, b, 0x20242E, 2);
    ksnprintf(b, 24, "%d. %d. %d", t.day, t.mon, t.year);
    draw_text_center(X + 150, Y + 320, b, 0x555B6B);
}
static void clock_tick(Window *w, u32 now){
    if (now - w->d.clock.last < 200) return;
    w->d.clock.last = now;
    RTime t; rtc_read(&t);
    if (t.sec != w->d.clock.last_sec) { w->d.clock.last_sec = t.sec; gui_dirty(); }
}

/* =====================================================================
 *  HAD
 * ===================================================================== */
#define GW 24
#define GH 18
#define CELL 16
static void snake_food(SnakeState *s){
    for (int tries = 0; tries < 2000; tries++) {
        int x = rand32() % GW, y = rand32() % GH, ok = 1;
        for (int i = 0; i < s->len; i++) if (s->sx[i] == x && s->sy[i] == y) { ok = 0; break; }
        if (ok) { s->fx = x; s->fy = y; return; }
    }
}
static void snake_reset(SnakeState *s){
    int best = s->best;
    memset(s, 0, sizeof *s); s->best = best;
    s->len = 3;
    for (int i = 0; i < 3; i++) { s->sx[i] = 12 - i; s->sy[i] = 9; }
    s->dx = s->ndx = 1; s->dy = s->ndy = 0;
    snake_food(s);
}
static void snake_init(Window *w){ srand32(millis()); snake_reset(&w->d.snake); }
static void snake_step(SnakeState *s){
    s->dx = s->ndx; s->dy = s->ndy;
    int nx = s->sx[0] + s->dx, ny = s->sy[0] + s->dy;
    int ate = (nx == s->fx && ny == s->fy);
    int lim = ate ? s->len : s->len - 1;
    int dead = (nx < 0 || ny < 0 || nx >= GW || ny >= GH);
    for (int i = 0; !dead && i < lim; i++) if (s->sx[i] == nx && s->sy[i] == ny) dead = 1;
    if (dead) { s->state = 2; if (s->score > s->best) s->best = s->score; return; }
    if (ate && s->len < GW * GH) s->len++;
    for (int i = s->len - 1; i > 0; i--) { s->sx[i] = s->sx[i - 1]; s->sy[i] = s->sy[i - 1]; }
    s->sx[0] = nx; s->sy[0] = ny;
    if (ate) { s->score++; snake_food(s); }
}
static void snake_key(Window *w, int k){
    SnakeState *s = &w->d.snake;
    int dx = 0, dy = 0, dir = 0;
    if (k == KEY_UP || k == 'w' || k == 'W') { dy = -1; dir = 1; }
    else if (k == KEY_DOWN || k == 's' || k == 'S') { dy = 1; dir = 1; }
    else if (k == KEY_LEFT || k == 'a' || k == 'A') { dx = -1; dir = 1; }
    else if (k == KEY_RIGHT || k == 'd' || k == 'D') { dx = 1; dir = 1; }
    if (dir) {
        if (s->state == 2) return;
        if (!(dx == -s->dx && dy == -s->dy)) { s->ndx = dx; s->ndy = dy; }
        if (s->state == 0) { s->state = 1; s->last = millis(); }
    } else if (k == ' ' || k == '\n') {
        if (s->state == 2) { snake_reset(s); s->state = 1; s->last = millis(); }
        else if (s->state == 0) { s->state = 1; s->last = millis(); }
    } else if (k == 'p' || k == 'P') {
        if (s->state == 1) s->state = 3; else if (s->state == 3) { s->state = 1; s->last = millis(); }
    }
}
static void snake_tick(Window *w, u32 now){
    SnakeState *s = &w->d.snake;
    if (s->state != 1) return;
    int iv = 150 - s->score * 4; if (iv < 60) iv = 60;
    if (now - s->last >= (u32)iv) { s->last = now; snake_step(s); gui_dirty(); }
}
static void snake_draw(Window *w){
    SnakeState *s = &w->d.snake;
    int X = CLX(w), Y = CLY(w) + 24;
    fill_rect(CLX(w), CLY(w), CLW(w), 24, 0x20242E);
    char b[48]; ksnprintf(b, 48, "Skore: %d     Rekord: %d", s->score, s->best);
    draw_text(CLX(w) + 10, CLY(w) + 4, b, 0xFFFFFF);
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        fill_rect(X + x * CELL, Y + y * CELL, CELL, CELL, ((x + y) & 1) ? 0x1E3A24 : 0x234529);
    fill_circle(X + s->fx * CELL + 8, Y + s->fy * CELL + 8, 6, 0xE74C3C);
    for (int i = s->len - 1; i >= 0; i--)
        fill_rrect(X + s->sx[i] * CELL + 1, Y + s->sy[i] * CELL + 1, CELL - 2, CELL - 2, 4, i == 0 ? 0x9CFF9C : 0x4CD964);
    const char *msg = NULL;
    if (s->state == 0) msg = "Sipky / WASD = start";
    else if (s->state == 2) msg = "Konec hry! Mezernik = znovu";
    else if (s->state == 3) msg = "Pauza (P)";
    if (msg) {
        int mw = text_w(msg) + 24;
        fill_rrect(X + GW * CELL / 2 - mw / 2, Y + GH * CELL / 2 - 20, mw, 40, 8, 0x101418);
        draw_text_center(X + GW * CELL / 2, Y + GH * CELL / 2 - 8, msg, 0xFFFFFF);
    }
}

/* =====================================================================
 *  TERMINAL
 * ===================================================================== */
#define TW 100
#define TL 300
static char (*tlines(Window *w))[TW]{ return (char (*)[TW])win_mem(w); }
static void t_newline(Window *w){
    TermState *s = &w->d.term; char (*L)[TW] = tlines(w);
    if (s->nlines >= TL) { memmove(L[0], L[1], (TL - 1) * TW); s->nlines = TL - 1; }
    L[s->nlines][0] = 0; s->nlines++;
}
static void t_puts(Window *w, const char *str){
    TermState *s = &w->d.term; char (*L)[TW] = tlines(w);
    for (; *str; str++) {
        if (*str == '\n') { t_newline(w); continue; }
        int l = (int)strlen(L[s->nlines - 1]);
        if (l >= TW - 2) { t_newline(w); l = 0; }
        L[s->nlines - 1][l] = *str; L[s->nlines - 1][l + 1] = 0;
    }
}
static void term_init(Window *w){
    TermState *s = &w->d.term; char (*L)[TW] = tlines(w);
    s->nlines = 1; L[0][0] = 0; s->blink = 1;
    t_puts(w, "SebaOS Terminal 1.0\nNapiste 'help' pro seznam prikazu.\n");
}
static int tstreq(const char *a, const char *b){ return strcmp(a, b) == 0; }
static void term_exec(Window *w, char *cmd){
    char b[100];
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;
    char *arg = cmd; while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg++ = 0; while (*arg == ' ') arg++; }
    if (tstreq(cmd, "help")) t_puts(w, "Prikazy: help clear echo ver time date uptime cpu res apps ls\n         open <app> rand reboot shutdown\n");
    else if (tstreq(cmd, "clear") || tstreq(cmd, "cls")) { w->d.term.nlines = 1; tlines(w)[0][0] = 0; }
    else if (tstreq(cmd, "echo")) { t_puts(w, arg); t_puts(w, "\n"); }
    else if (tstreq(cmd, "ver")) t_puts(w, "SebaOS 1.0 (32-bit protected mode, VESA, PS/2)\n");
    else if (tstreq(cmd, "time")) { RTime t; rtc_read(&t); ksnprintf(b, 100, "%02d:%02d:%02d\n", t.hour, t.min, t.sec); t_puts(w, b); }
    else if (tstreq(cmd, "date")) { RTime t; rtc_read(&t); ksnprintf(b, 100, "%d. %d. %d\n", t.day, t.mon, t.year); t_puts(w, b); }
    else if (tstreq(cmd, "uptime")) { u32 s = millis() / 1000; ksnprintf(b, 100, "Beh systemu: %d min %d s\n", s / 60, s % 60); t_puts(w, b); }
    else if (tstreq(cmd, "cpu")) { char v[16]; cpu_vendor(v); ksnprintf(b, 100, "CPU: %s\n", v); t_puts(w, b); }
    else if (tstreq(cmd, "res")) { ksnprintf(b, 100, "Rozliseni: %dx%d, 32 bpp\n", scr_w, scr_h); t_puts(w, b); }
    else if (tstreq(cmd, "ls")) {
        int n = file_count();
        if (!n) t_puts(w, "Zadne dokumenty. Pravy klik na plochu -> Novy dokument.\n");
        for (int k = 0; k < n; k++) { int f = file_nth(k); ksnprintf(b, 100, "  %s  (%d B)\n", file_name(f), file_len(f)); t_puts(w, b); }
    }
    else if (tstreq(cmd, "rand")) { ksnprintf(b, 100, "%d\n", rand32() % 1000); t_puts(w, b); }
    else if (tstreq(cmd, "apps")) { for (int i = 0; i < app_count; i++) { ksnprintf(b, 100, "  %s  (%s)\n", app_table[i].id, app_table[i].title); t_puts(w, b); } }
    else if (tstreq(cmd, "open")) {
        int a = app_find(arg);
        if (a < 0) t_puts(w, "Neznama aplikace. Zkuste 'apps'.\n"); else gui_open(a);
    }
    else if (tstreq(cmd, "reboot")) sys_reboot();
    else if (tstreq(cmd, "shutdown")) sys_shutdown();
    else { ksnprintf(b, 100, "Neznamy prikaz: %s\n", cmd); t_puts(w, b); }
}
static void term_key(Window *w, int k){
    TermState *s = &w->d.term; s->blink = 1;
    int rows = CLH(w) / 16;
    if (k == KEY_PGUP) { s->scroll += rows / 2; if (s->scroll > s->nlines) s->scroll = s->nlines; return; }
    if (k == KEY_PGDN) { s->scroll -= rows / 2; if (s->scroll < 0) s->scroll = 0; return; }
    s->scroll = 0;
    if (k == '\n') {
        char b[120]; ksnprintf(b, 120, "seba> %s\n", s->in); t_puts(w, b);
        char cmd[100]; strcpy_s(cmd, s->in, 100);
        if (s->in[0]) strcpy_s(s->last, s->in, 100);
        s->in[0] = 0; s->inlen = 0;
        term_exec(w, cmd);
    } else if (k == '\b') { if (s->inlen > 0) s->in[--s->inlen] = 0; }
    else if (k == KEY_UP) { strcpy_s(s->in, s->last, 100); s->inlen = (int)strlen(s->in); }
    else if (k >= 32 && k < 127 && s->inlen < 90) { s->in[s->inlen++] = (char)k; s->in[s->inlen] = 0; }
}
static void term_draw(Window *w){
    TermState *s = &w->d.term; char (*L)[TW] = tlines(w);
    int X = CLX(w), Y = CLY(w), rows = CLH(w) / 16, cols = (CLW(w) - 8) / 8;
    fill_rect(X, Y, CLW(w), CLH(w), 0x0C0F14);
    int total = s->nlines;                 /* posledni radek = prompt */
    int first = total - rows - s->scroll; if (first < 0) first = 0;
    for (int i = 0; i < rows; i++) {
        int idx = first + i; if (idx >= total) break;
        int ty = Y + 2 + i * 16;
        if (idx < total - 1) {
            char tmp[TW]; strcpy_s(tmp, L[idx], cols + 1 < TW ? cols + 1 : TW);
            draw_text(X + 4, ty, tmp, 0xC8D0DC);
        } else {
            draw_text(X + 4, ty, "seba> ", 0x6BE675);
            draw_text(X + 4 + 48, ty, s->in, COL_WHITE);
            if (gui_is_focused(w) && s->blink) fill_rect(X + 4 + 48 + s->inlen * 8, ty + 13, 8, 2, 0x6BE675);
        }
    }
}
static void term_tick(Window *w, u32 now){
    int b = (now / 500) & 1;
    if (b != (w->d.term.blink & 1) && gui_is_focused(w)) { w->d.term.blink = b; gui_dirty(); }
}

/* =====================================================================
 *  O SYSTEMU
 * ===================================================================== */
static void about_draw(Window *w){
    int X = CLX(w), Y = CLY(w);
    fill_rect(X, Y, CLW(w), 70, 0x16213E);
    draw_text_scaled(X + 20, Y + 14, "SebaOS", COL_WHITE, 3);
    draw_text(X + 24, Y + 56, "32bitovy operacni system", 0x9CC2FF);
    char b[80], v[16]; cpu_vendor(v);
    u32 s = millis() / 1000;
    int y = Y + 86;
    ksnprintf(b, 80, "Procesor:   %s", v); draw_text(X + 20, y, b, 0x20242E); y += 20;
    ksnprintf(b, 80, "Rozliseni:  %dx%d, 32 bpp", scr_w, scr_h); draw_text(X + 20, y, b, 0x20242E); y += 20;
    ksnprintf(b, 80, "Beh:        %d min %d s", s / 60, s % 60); draw_text(X + 20, y, b, 0x20242E); y += 28;
    draw_text(X + 20, y, "Vlastnosti:", COL_ACCENT); y += 20;
    const char *f[] = { "- vlastni bootloader (BIOS, LBA)", "- 32bitovy protected mode", "- VESA framebuffer + okna",
                        "- ovladace PS/2 mysi a klavesnice", "- PIT casovac, RTC, mereni CPU", "- 8 aplikaci, dokumenty v RAM" };
    for (int i = 0; i < 6; i++) { draw_text(X + 20, y, f[i], 0x333844); y += 18; }
}
static void about_tick(Window *w, u32 now){
    if (now - w->d.about.last >= 1000) { w->d.about.last = now; if (gui_is_focused(w)) gui_dirty(); }
}

/* =====================================================================
 *  SPRAVCE ULOH
 * ===================================================================== */
static void ic_taskmgr(int x, int y){
    fill_rrect(x + 2, y + 4, 36, 32, 4, 0x101820);
    int px = x + 6, py = y + 28;
    static const int v[8] = { 4, 8, 6, 14, 10, 18, 12, 20 };
    for (int i = 0; i < 7; i++) draw_line(px + i * 4, py - v[i], px + (i + 1) * 4, py - v[i + 1], 0x4CD964);
    rect_outline(x + 2, y + 4, 36, 32, 0x4C9BE8);
}

int app_memory(Window *w){
    int kb = 4;
    if (!strcmp(w->app->id, "notepad")) kb += w->d.np.len / 1024 + 1;
    else if (!strcmp(w->app->id, "paint")) kb += PW * PH / 1024;
    else if (!strcmp(w->app->id, "terminal")) kb += w->d.term.nlines * TW / 1024 + 1;
    return kb;
}

typedef struct { char name[24]; const char *state; u32 kb; int slot; } PRow;
static int tm_rows(PRow *r){
    MemInfo m; mem_info(&m); int n = 0;
    strcpy_s(r[n].name, "Jadro SebaOS", 24); r[n].state = "System"; r[n].kb = m.kernel; r[n].slot = -1; n++;
    strcpy_s(r[n].name, "Graficky podsystem", 24); r[n].state = "System"; r[n].kb = m.gfx; r[n].slot = -1; n++;
    for (int i = 0; i < MAXWIN; i++) {
        Window *w = gui_win(i); if (!w) continue;
        strcpy_s(r[n].name, w->app->title, 24);
        r[n].state = w->minimized ? "Minimalizovano" : "Bezi"; r[n].kb = (u32)app_memory(w); r[n].slot = i; n++;
    }
    return n;
}
static void fmt_kb(char *b, u32 kb){
    if (kb >= 10240) ksnprintf(b, 16, "%d MB", kb / 1024); else ksnprintf(b, 16, "%d KB", kb);
}
static u32 load_col(int p){ return p < 60 ? 0x4CD964 : (p < 85 ? 0xF2A23A : 0xE8504A); }
static void tm_bar(int x, int y, int w, int h, int pct, u32 col, const char *label){
    fill_rect(x, y, w, h, 0xC9CEDA);
    fill_rect(x, y, w * pct / 100, h, col);
    rect_outline(x, y, w, h, 0x8A91A3);
    draw_text(x + 6, y + (h - 16) / 2, label, 0x101418);
}
static void tm_draw(Window *w){
    TmState *s = &w->d.tm;
    int X = CLX(w), Y = CLY(w), W = CLW(w), H = CLH(w);
    MemInfo m; mem_info(&m);
    int ramp = (int)(m.used * 100 / m.total);
    PRow rows[MAXWIN + 2]; int nr = tm_rows(rows);
    char b[96], c[16];
    fill_rect(X, Y, W, 30, 0xDADDE4);
    const char *tabs[2] = { "Procesy", "Vykon" };
    for (int t = 0; t < 2; t++) {
        fill_rrect(X + 8 + t * 108, Y + 4, 100, 24, 4, s->tab == t ? COL_ACCENT : 0xF4F5F8);
        draw_text_center(X + 8 + t * 108 + 50, Y + 8, tabs[t], s->tab == t ? COL_WHITE : 0x20242E);
    }
    if (s->tab == 0) {
        ksnprintf(b, 32, "CPU  %d %%", perf_load); tm_bar(X + 10, Y + 38, 220, 20, perf_load, load_col(perf_load), b);
        ksnprintf(b, 32, "RAM  %d %%", ramp);       tm_bar(X + 250, Y + 38, 220, 20, ramp, load_col(ramp), b);
        fill_rect(X + 10, Y + 68, W - 20, 20, 0xCDD2DD);
        draw_text(X + 18, Y + 70, "Nazev", 0x20242E); draw_text(X + 250, Y + 70, "Stav", 0x20242E);
        draw_text(X + W - 18 - 5 * 8, Y + 70, "Pamet", 0x20242E);
        if (!gui_win(s->sel)) s->sel = -1;
        for (int i = 0; i < nr; i++) {
            int ry = Y + 90 + i * 20, sel = (rows[i].slot >= 0 && rows[i].slot == s->sel);
            fill_rect(X + 10, ry, W - 20, 20, sel ? COL_ACCENT : ((i & 1) ? 0xEBEEF3 : 0xF7F8FA));
            u32 fg = sel ? COL_WHITE : (rows[i].slot < 0 ? 0x6B7385 : 0x20242E);
            draw_text(X + 18, ry + 2, rows[i].name, fg);
            draw_text(X + 250, ry + 2, rows[i].state, fg);
            fmt_kb(c, rows[i].kb); draw_text(X + W - 18 - text_w(c), ry + 2, c, fg);
        }
        int can = (s->sel >= 0);
        fill_rrect(X + W - 150, Y + H - 62, 140, 28, 5, can ? 0xD9534F : 0xB9BDC8);
        draw_text_center(X + W - 80, Y + H - 56, "Ukoncit ulohu", COL_WHITE);
    } else {
        ksnprintf(b, 40, "Vyuziti CPU: %d %%", perf_load); draw_text(X + 10, Y + 36, b, 0x20242E);
        int gx = X + 10, gy = Y + 56, gw = W - 20, gh = 100;
        fill_rect(gx, gy, gw, gh, 0x101820);
        for (int q = 1; q < 4; q++) fill_rect(gx, gy + gh * q / 4, gw, 1, 0x26323F);
        int lastx = -1, lasty = 0;
        for (int px = 0; px < gw; px++) {
            int idx = px * (PERF_N - 1) / (gw - 1), v = perf_hist[idx];
            int py = gy + gh - 2 - v * (gh - 4) / 100;
            fill_rect(gx + px, py, 1, gy + gh - py, 0x1B5E3F);
            if (lastx >= 0) draw_line(gx + lastx, lasty, gx + px, py, 0x6BE69B);
            lastx = px; lasty = py;
        }
        rect_outline(gx, gy, gw, gh, 0x8A91A3);
        char br[49]; cpu_brand(br);
        int y = Y + 164;
        ksnprintf(b, 96, "Procesor:  %s", br); draw_text(X + 10, y, b, 0x20242E); y += 18;
        if (perf_mhz) ksnprintf(b, 96, "Frekvence: %d MHz (zmerena)", perf_mhz); else ksnprintf(b, 96, "Frekvence: mereni...");
        draw_text(X + 10, y, b, 0x20242E); y += 18;
        int watts10 = 60 + perf_load * 390 / 100;
        ksnprintf(b, 96, "Prikon CPU: ~%d.%d W  (odhad z vytizeni, ne mereni)", watts10 / 10, watts10 % 10);
        draw_text(X + 10, y, b, 0x20242E); y += 18;
        u32 sec = millis() / 1000;
        ksnprintf(b, 96, "Beh: %d min %d s    Procesy: %d", sec / 60, sec % 60, nr); draw_text(X + 10, y, b, 0x20242E);
        ksnprintf(b, 96, "Pamet RAM: %d / %d MB  (%d %%)", m.used / 1024, m.total / 1024, ramp);
        draw_text(X + 10, Y + 248, b, 0x20242E);
        struct { const char *n; u32 kb; u32 col; } seg[6] = {
            { "Hardware/BIOS", m.reserved, 0x7F8798 }, { "Jadro", m.kernel, 0x2D6CDF }, { "Grafika", m.gfx, 0xA349A4 },
            { "Aplikace", m.apps, 0xF2994A }, { "Dokumenty", m.docs, 0xF2D13A }, { "Volno", m.freekb, 0x2B3245 } };
        int bx = X + 10, bw = W - 20, cur = 0;
        fill_rect(bx, Y + 268, bw, 22, 0x2B3245);
        for (int i = 0; i < 6; i++) {
            int w1 = (int)(seg[i].kb * (u32)bw / m.total);
            if (seg[i].kb && w1 < 1) w1 = 1;
            if (cur + w1 > bw) w1 = bw - cur;
            fill_rect(bx + cur, Y + 268, w1, 22, seg[i].col); cur += w1;
        }
        rect_outline(bx, Y + 268, bw, 22, 0x8A91A3);
        for (int i = 0; i < 6; i++) {
            int lx = X + 10 + (i / 3) * 230, ly = Y + 298 + (i % 3) * 20;
            fill_rect(lx, ly + 2, 12, 12, seg[i].col); rect_outline(lx, ly + 2, 12, 12, 0x555B6B);
            fmt_kb(c, seg[i].kb); ksnprintf(b, 40, "%s: %s", seg[i].n, c); draw_text(lx + 18, ly, b, 0x333844);
        }
    }
    fill_rect(X, Y + H - 24, W, 24, 0xDADDE4);
    ksnprintf(b, 96, "Procesy: %d    CPU: %d %%    RAM: %d %%", nr, perf_load, ramp);
    draw_text(X + 8, Y + H - 20, b, 0x333844);
}
static void tm_mouse(Window *w, int x, int y, int ev){
    TmState *s = &w->d.tm;
    if (ev != EV_DOWN) return;
    if (y < 30) {
        for (int t = 0; t < 2; t++) if (x >= 8 + t * 108 && x < 108 + t * 108) s->tab = t;
        return;
    }
    if (s->tab != 0) return;
    PRow rows[MAXWIN + 2]; int nr = tm_rows(rows);
    int W = CLW(w), H = CLH(w);
    if (x >= W - 150 && x < W - 10 && y >= H - 62 && y < H - 34 && s->sel >= 0) {
        Window *t = gui_win(s->sel); s->sel = -1;
        if (t) gui_close(t);
        return;
    }
    if (y >= 90 && y < 90 + nr * 20 && x >= 10 && x < W - 10) {
        int i = (y - 90) / 20;
        s->sel = rows[i].slot;
    }
}
static void tm_tick(Window *w, u32 now){
    (void)now;
    if (w->d.tm.seq != perf_seq) { w->d.tm.seq = perf_seq; if (!w->minimized) gui_dirty(); }
}
static void tm_init(Window *w){ w->d.tm.sel = -1; w->d.tm.seq = perf_seq; }

/* =====================================================================
 *  TABULKA APLIKACI
 * ===================================================================== */
const App app_table[] = {
    { "notepad",  "Poznamky",   520, 360, 1, 0xF2B632, np_init,    np_draw,    np_mouse,    np_key,    np_tick,    ic_notes },
    { "calc",     "Kalkulacka", 262, 310, 0, 0xF2994A, calc_init,  calc_draw,  calc_mouse,  calc_key,  NULL,       ic_calc  },
    { "paint",    "Malovani",   520, 380, 1, 0xE74C3C, paint_init, paint_draw, paint_mouse, NULL,      NULL,       ic_paint },
    { "clock",    "Hodiny",     300, 340, 0, 0x4C9BE8, NULL,       clock_draw, NULL,        NULL,      clock_tick, ic_clock },
    { "snake",    "Had",        384, 312, 0, 0x4CD964, snake_init, snake_draw, NULL,        snake_key, snake_tick, ic_snake },
    { "terminal", "Terminal",   560, 340, 1, 0x3A4256, term_init,  term_draw,  NULL,        term_key,  term_tick,  ic_term  },
    { "taskmgr",  "Spravce uloh", 480, 400, 0, 0x2EB87A, tm_init, tm_draw, tm_mouse, NULL, tm_tick, ic_taskmgr },
    { "about",    "O systemu",  340, 300, 0, COL_ACCENT, NULL,     about_draw, NULL,        NULL,      about_tick, ic_about },
};
const int app_count = sizeof(app_table) / sizeof(app_table[0]);
