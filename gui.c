#include "kernel.h"

#define COL_BORDER   0x1E2430
#define COL_TITLE_A  0x2D6CDF
#define COL_TITLE_I  0x6B7385
#define COL_TASKBAR  0x141A2B
#define COL_TASKLINE 0x2E3A5A

static Window wins[MAXWIN];
static Window *z[MAXWIN];
static int nz;
static Window *focus, *drag_win, *cap_win;
static int drag_dx, drag_dy;
static int dirty = 1, menu_open, sel_icon = -1, last_icon = -1;
static u32 last_click_ms;
static int prev_btn, spawn_n;
static char clock_str[8] = "--:--";
static int clock_min = -1;
static u32 clock_last;
static int mx_now, my_now;

/* ---------- dokumenty v RAM ---------- */
typedef struct { int used; char name[16]; int len; } FileEnt;
static FileEnt files[MAXFILES];
static int doc_counter;
char *file_data(int i){ return (char *)(0x1300000 + i * 0x10000); }
const char *file_name(int i){ return files[i].name; }
int file_len(int i){ return files[i].len; }
void file_set_len(int i, int len){ files[i].len = len; }
int file_count(void){ int n = 0; for (int i = 0; i < MAXFILES; i++) n += files[i].used; return n; }
int file_nth(int k){ for (int i = 0; i < MAXFILES; i++) if (files[i].used && k-- == 0) return i; return -1; }
int file_new(void){
    for (int i = 0; i < MAXFILES; i++) if (!files[i].used) {
        files[i].used = 1; files[i].len = 0; file_data(i)[0] = 0;
        ksnprintf(files[i].name, 16, "Dokument %d", ++doc_counter);
        return i;
    }
    return -1;
}

/* ---------- kontextova nabidka ---------- */
enum { ACT_NEWDOC = 1, ACT_TASKMGR, ACT_ABOUT, ACT_OPENFILE, ACT_DELFILE, ACT_OPENAPP };
typedef struct { const char *label; int act; } CtxItem;
static CtxItem ctx_items[4];
static int ctx_n, ctx_x, ctx_y, ctx_open, ctx_target;
#define CTX_W 180
#define CTX_IH 26
static int ctx_h(void){ return ctx_n * CTX_IH + 8; }
static void ctx_show(int x, int y){
    ctx_x = x; ctx_y = y;
    if (ctx_x + CTX_W > scr_w) ctx_x = scr_w - CTX_W;
    if (ctx_y + ctx_h() > scr_h - TASKBAR_H) ctx_y = scr_h - TASKBAR_H - ctx_h();
    if (ctx_y < 0) ctx_y = 0;
    ctx_open = 1; dirty = 1;
}
static void ctx_add(const char *l, int act){ ctx_items[ctx_n].label = l; ctx_items[ctx_n].act = act; ctx_n++; }
static int ctx_hit(int x, int y){
    if (x < ctx_x || x >= ctx_x + CTX_W || y < ctx_y + 4) return -1;
    int i = (y - ctx_y - 4) / CTX_IH;
    return i < ctx_n ? i : -1;
}
static int in_ctx(int x, int y){ return x >= ctx_x && x < ctx_x + CTX_W && y >= ctx_y && y < ctx_y + ctx_h(); }

void gui_dirty(void){ dirty = 1; }
int gui_need_redraw(void){ return dirty; }
int gui_is_focused(Window *w){ return w == focus; }
void *win_mem(Window *w){ return (void *)(0x1000000 + w->slot * WIN_MEM_SIZE); }

int app_find(const char *name){
    for (int i = 0; i < app_count; i++) if (strcmp(app_table[i].id, name) == 0) return i;
    return -1;
}

/* ---------- z-order ---------- */
static void z_remove(Window *w){
    int j = 0;
    for (int i = 0; i < nz; i++) if (z[i] != w) z[j++] = z[i];
    nz = j;
}
static void win_raise(Window *w){ z_remove(w); z[nz++] = w; focus = w; dirty = 1; }
static void refocus(void){
    focus = NULL;
    for (int i = nz - 1; i >= 0; i--) if (!z[i]->minimized) { focus = z[i]; break; }
}

Window *gui_open(int ai){
    if (ai < 0 || ai >= app_count) return NULL;
    Window *w = NULL;
    for (int i = 0; i < MAXWIN; i++) if (!wins[i].used) { w = &wins[i]; w->slot = i; break; }
    if (!w) return NULL;
    int slot = w->slot;
    memset(w, 0, sizeof(Window));
    w->slot = slot; w->used = 1; w->app = &app_table[ai];
    strcpy_s(w->title, w->app->title, 40);
    w->w = w->app->cw + 2; w->h = w->app->ch + TITLE_H + 2;
    int maxh = scr_h - TASKBAR_H;
    if (w->w > scr_w) w->w = scr_w;
    if (w->h > maxh) w->h = maxh;
    w->x = 70 + (spawn_n % 8) * 28; w->y = 30 + (spawn_n % 8) * 28; spawn_n++;
    if (w->x + w->w > scr_w) w->x = scr_w - w->w;
    if (w->y + w->h > maxh) w->y = maxh - w->h;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
    if (w->app->init) w->app->init(w);
    z[nz++] = w; focus = w; dirty = 1;
    return w;
}
void gui_close(Window *w){
    if (!w || !w->used) return;
    w->used = 0; z_remove(w);
    if (cap_win == w) cap_win = NULL;
    if (drag_win == w) drag_win = NULL;
    refocus(); dirty = 1;
}

static void toggle_max(Window *w){
    if (!w->app->resizable) return;
    if (!w->maximized) {
        w->rx = w->x; w->ry = w->y; w->rw = w->w; w->rh = w->h;
        w->x = 0; w->y = 0; w->w = scr_w; w->h = scr_h - TASKBAR_H; w->maximized = 1;
    } else {
        w->x = w->rx; w->y = w->ry; w->w = w->rw; w->h = w->rh; w->maximized = 0;
    }
    dirty = 1;
}

/* ---------- geometri ---------- */
static void btn_pos(Window *w, int which, int *bx, int *by){   /* 0 close, 1 max, 2 min */
    int idx = which;
    if (which == 2 && !w->app->resizable) idx = 1;
    *bx = w->x + w->w - 6 - 22 - idx * 26; *by = w->y + 4;
}
static int hit_btn(Window *w, int x, int y){
    for (int k = 0; k < 3; k++) {
        if (k == 1 && !w->app->resizable) continue;
        int bx, by; btn_pos(w, k, &bx, &by);
        if (x >= bx && x < bx + 22 && y >= by && y < by + 16) return k + 1;
    }
    return 0;
}
static int in_win(Window *w, int x, int y){ return x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h; }

static int icon_rows(void){ int r = (scr_h - TASKBAR_H - 12) / 84; return r < 1 ? 1 : r; }
static void icon_pos(int i, int *x, int *y){ int rows = icon_rows(); *x = 12 + (i / rows) * 100; *y = 12 + (i % rows) * 84; }
static int icon_total(void){ return app_count + file_count(); }
static int icon_hit(int x, int y){
    for (int i = 0; i < icon_total(); i++) {
        int ix, iy; icon_pos(i, &ix, &iy);
        if (x >= ix && x < ix + 96 && y >= iy && y < iy + 80) return i;
    }
    return -1;
}
static void draw_doc_icon(int x, int y){
    fill_rrect(x + 6, y + 2, 28, 36, 3, 0xFFFFFF);
    fill_rect(x + 26, y + 2, 8, 8, 0xC8D3E8);
    for (int i = 0; i < 5; i++) fill_rect(x + 11, y + 14 + i * 5, i == 4 ? 10 : 18, 2, 0x5C8DE8);
}

#define MENU_W 210
#define MENU_IH 28
static int menu_h(void){ return (app_count + 2) * MENU_IH + 18; }
static int menu_top(void){ return scr_h - TASKBAR_H - menu_h() - 4; }
static int menu_item_y(int i){
    int t = menu_top() + 4;
    if (i < app_count) return t + i * MENU_IH;
    return t + app_count * MENU_IH + 10 + (i - app_count) * MENU_IH;
}
static int menu_hit(int x, int y){
    if (x < 4 || x >= 4 + MENU_W) return -1;
    for (int i = 0; i < app_count + 2; i++) { int iy = menu_item_y(i); if (y >= iy && y < iy + MENU_IH) return i; }
    return -1;
}

/* ---------- init / wallpaper ---------- */
void gui_init(void){
    u32 *old = gfx_set_target(gfx_wall());
    gfx_noclip();
    gfx_vgradient(0, 0, scr_w, scr_h, 0x0A1430, 0x2F6FB5);
    for (int i = 0; i < 6; i++) {
        int r = 60 + i * 55;
        fill_rect_alpha(scr_w - r - 20 + i * 8, scr_h - r - 60 + i * 10, r, 2, 0xFFFFFF, 10);
    }
    draw_text_scaled(scr_w - 8 * 6 * 5 - 24, scr_h - TASKBAR_H - 100, "SebaOS", 0x4F82C8, 5);
    draw_text(scr_w - 200, scr_h - TASKBAR_H - 26, "32-bit operacni system", 0x7FA6DC);
    gfx_set_target(old);
    
}

/* ---------- vykreslovani ---------- */
static void draw_window(Window *w){
    int act = (w == focus);
    gfx_noclip();
    fill_rect_alpha(w->x + w->w, w->y + 6, 6, w->h, 0, 90);
    fill_rect_alpha(w->x + 6, w->y + w->h, w->w - 6, 6, 0, 90);
    gfx_clip(w->x, w->y, w->w, w->h);
    fill_rect(w->x, w->y, w->w, w->h, COL_BORDER);
    fill_rect(w->x + 1, w->y + 1, w->w - 2, TITLE_H, act ? COL_TITLE_A : COL_TITLE_I);
    fill_rect(w->x + 8, w->y + 6, 12, 12, w->app->color);
    rect_outline(w->x + 8, w->y + 6, 12, 12, 0xFFFFFF);
    int bx, by; btn_pos(w, w->app->resizable ? 2 : 1, &bx, &by);
    gfx_clip(w->x + 1, w->y + 1, bx - w->x - 4, TITLE_H);
    draw_text(w->x + 28, w->y + 1 + (TITLE_H - 16) / 2, w->title, COL_WHITE);
    gfx_clip(w->x, w->y, w->w, w->h);
    /* tlacitka */
    btn_pos(w, 0, &bx, &by);
    fill_rrect(bx, by, 22, 16, 3, 0xD9534F);
    draw_line(bx + 7, by + 4, bx + 14, by + 11, COL_WHITE); draw_line(bx + 14, by + 4, bx + 7, by + 11, COL_WHITE);
    draw_line(bx + 8, by + 4, bx + 15, by + 11, COL_WHITE); draw_line(bx + 13, by + 4, bx + 6, by + 11, COL_WHITE);
    u32 bc = act ? 0x5C8DE8 : 0x808798;
    if (w->app->resizable) {
        btn_pos(w, 1, &bx, &by);
        fill_rrect(bx, by, 22, 16, 3, bc);
        if (w->maximized) { rect_outline(bx + 8, by + 3, 8, 8, COL_WHITE); rect_outline(bx + 6, by + 5, 8, 8, COL_WHITE); }
        else { rect_outline(bx + 6, by + 3, 10, 10, COL_WHITE); fill_rect(bx + 6, by + 3, 10, 2, COL_WHITE); }
    }
    btn_pos(w, 2, &bx, &by);
    fill_rrect(bx, by, 22, 16, 3, bc);
    fill_rect(bx + 6, by + 10, 10, 2, COL_WHITE);
    /* klientska oblast */
    gfx_clip(CLX(w), CLY(w), CLW(w), CLH(w));
    fill_rect(CLX(w), CLY(w), CLW(w), CLH(w), COL_PANEL);
    if (w->app->draw) w->app->draw(w);
    gfx_noclip();
}

static void draw_taskbar(void){
    int ty = scr_h - TASKBAR_H;
    gfx_noclip();
    fill_rect(0, ty, scr_w, TASKBAR_H, COL_TASKBAR);
    fill_rect(0, ty, scr_w, 1, COL_TASKLINE);
    fill_rrect(6, ty + 4, 96, 24, 5, menu_open ? 0x1F4FA8 : COL_ACCENT);
    fill_circle(22, ty + 16, 7, COL_WHITE);
    fill_circle(22, ty + 16, 4, menu_open ? 0x1F4FA8 : COL_ACCENT);
    draw_text(36, ty + 8, "SebaOS", COL_WHITE);
    int x = 112;
    for (int i = 0; i < MAXWIN; i++) {
        Window *w = &wins[i]; if (!w->used) continue;
        int act = (w == focus && !w->minimized);
        fill_rrect(x, ty + 4, 150, 24, 4, act ? 0x32415F : 0x1E2740);
        if (act) fill_rect(x + 6, ty + 26, 138, 2, COL_ACCENT);
        fill_rect(x + 8, ty + 10, 12, 12, w->app->color);
        char t[16]; strcpy_s(t, w->title, 14);
        gfx_clip(x, ty, 150, TASKBAR_H);
        draw_text(x + 26, ty + 8, t, w->minimized ? 0x8A93A8 : COL_WHITE);
        gfx_noclip();
        x += 154;
    }
    draw_text(scr_w - 8 * 5 - 12, ty + 8, clock_str, COL_WHITE);
}

static void draw_menu(void){
    int t = menu_top();
    gfx_noclip();
    fill_rect_alpha(10, t + 6, MENU_W, menu_h(), 0, 90);
    fill_rect(4, t, MENU_W, menu_h(), 0x1B2236);
    rect_outline(4, t, MENU_W, menu_h(), COL_TASKLINE);
    int hover = menu_hit(mx_now, my_now);
    for (int i = 0; i < app_count + 2; i++) {
        int iy = menu_item_y(i);
        if (i == hover) fill_rect(6, iy, MENU_W - 4, MENU_IH, COL_ACCENT);
        if (i < app_count) {
            fill_rrect(14, iy + 6, 16, 16, 3, app_table[i].color);
            draw_text(40, iy + 6, app_table[i].title, COL_WHITE);
        } else {
            draw_text(40, iy + 6, i == app_count ? "Restartovat" : "Vypnout", i == app_count ? COL_WHITE : 0xFF8A80);
        }
    }
    fill_rect(14, menu_item_y(app_count) - 6, MENU_W - 20, 1, COL_TASKLINE);
}

static void draw_ctx(void){
    int h = ctx_h();
    gfx_noclip();
    fill_rect_alpha(ctx_x + 5, ctx_y + 5, CTX_W, h, 0, 90);
    fill_rect(ctx_x, ctx_y, CTX_W, h, 0x1B2236);
    rect_outline(ctx_x, ctx_y, CTX_W, h, COL_TASKLINE);
    int hv = ctx_hit(mx_now, my_now);
    for (int i = 0; i < ctx_n; i++) {
        int iy = ctx_y + 4 + i * CTX_IH;
        if (i == hv) fill_rect(ctx_x + 3, iy, CTX_W - 6, CTX_IH, COL_ACCENT);
        draw_text(ctx_x + 14, iy + 5, ctx_items[i].label, ctx_items[i].act == ACT_DELFILE ? 0xFF8A80 : COL_WHITE);
    }
}

void gui_render(void){
    copy32(back, gfx_wall(), scr_w * scr_h);
    gfx_noclip();
    for (int i = 0; i < icon_total(); i++) {
        int x, y; icon_pos(i, &x, &y);
        const char *label;
        if (i == sel_icon) { fill_rect_alpha(x, y, 96, 80, COL_WHITE, 60); rect_outline(x, y, 96, 80, 0x9CC2FF); }
        if (i < app_count) { app_table[i].icon(x + 28, y + 6); label = app_table[i].title; }
        else { draw_doc_icon(x + 28, y + 6); label = files[file_nth(i - app_count)].name; }
        int tx = x + 48 - text_w(label) / 2;
        draw_text(tx + 1, y + 53, label, COL_BLACK);
        draw_text(tx, y + 52, label, COL_WHITE);
    }
    for (int i = 0; i < nz; i++) if (!z[i]->minimized) draw_window(z[i]);
    draw_taskbar();
    if (menu_open) draw_menu();
    if (ctx_open) draw_ctx();
    dirty = 0;
}

/* ---------- vstup ---------- */
static void do_shutdown(void){
    gfx_noclip();
    fill_rect(0, 0, scr_w, scr_h, COL_BLACK);
    draw_text_center(scr_w / 2, scr_h / 2 - 8, "Pocitac se vypina...", 0xFFB74D);
    gfx_present_all();
    sys_shutdown();
}
static void menu_action(int i){
    if (i < app_count) gui_open(i);
    else if (i == app_count) sys_reboot();
    else do_shutdown();
}

static void taskbar_press(int x, int y){
    int ty = scr_h - TASKBAR_H;
    if (x >= 6 && x < 102 && y >= ty + 4 && y < ty + 28) { menu_open = !menu_open; dirty = 1; return; }
    int bx = 112;
    for (int i = 0; i < MAXWIN; i++) {
        Window *w = &wins[i]; if (!w->used) continue;
        if (x >= bx && x < bx + 150) {
            if (w->minimized) { w->minimized = 0; win_raise(w); }
            else if (w == focus) { w->minimized = 1; refocus(); }
            else win_raise(w);
            dirty = 1; return;
        }
        bx += 154;
    }
}

static void open_icon(int hit){
    if (hit < 0) return;
    if (hit < app_count) gui_open(hit);
    else gui_open_file(file_nth(hit - app_count));
}
static void ctx_action(int act, int tg){
    switch (act) {
    case ACT_NEWDOC: { int f = file_new(); if (f >= 0) gui_open_file(f); break; }
    case ACT_TASKMGR: gui_open(app_find("taskmgr")); break;
    case ACT_ABOUT: gui_open(app_find("about")); break;
    case ACT_OPENFILE: gui_open_file(tg); break;
    case ACT_DELFILE: file_delete(tg); break;
    case ACT_OPENAPP: gui_open(tg); break;
    }
}

static void handle_press(int x, int y){
    if (ctx_open) {
        int it = ctx_hit(x, y);
        int act = it >= 0 ? ctx_items[it].act : 0, tg = ctx_target;
        ctx_open = 0; dirty = 1;
        if (it >= 0) { ctx_action(act, tg); return; }
        if (in_ctx(x, y)) return;
    }
    int sb = (y >= scr_h - TASKBAR_H && x >= 6 && x < 102);
    if (menu_open) {
        int it = menu_hit(x, y);
        menu_open = 0; dirty = 1;
        if (it >= 0) { menu_action(it); return; }
        if (sb) return;
    }
    if (y >= scr_h - TASKBAR_H) { taskbar_press(x, y); return; }
    for (int i = nz - 1; i >= 0; i--) {
        Window *w = z[i];
        if (w->minimized || !in_win(w, x, y)) continue;
        win_raise(w);
        if (y < w->y + TITLE_H + 1) {
            int b = hit_btn(w, x, y);
            if (b == 1) { gui_close(w); return; }
            if (b == 2) { toggle_max(w); return; }
            if (b == 3) { w->minimized = 1; refocus(); dirty = 1; return; }
            if (!w->maximized) { drag_win = w; drag_dx = x - w->x; drag_dy = y - w->y; }
        } else if (x >= CLX(w) && y >= CLY(w) && x < CLX(w) + CLW(w) && y < CLY(w) + CLH(w)) {
            cap_win = w;
            if (w->app->mouse) w->app->mouse(w, x - CLX(w), y - CLY(w), EV_DOWN);
        }
        return;
    }
    /* plocha */
    focus = NULL; dirty = 1;
    int hit = icon_hit(x, y);
    u32 now = millis();
    if (hit >= 0 && hit == last_icon && now - last_click_ms < 500) { open_icon(hit); sel_icon = -1; last_icon = -1; return; }
    sel_icon = hit; last_icon = hit; last_click_ms = now;
}

static void handle_rpress(int x, int y){
    menu_open = 0; ctx_open = 0; ctx_n = 0; dirty = 1;
    if (y >= scr_h - TASKBAR_H) {
        if (x >= 108) { ctx_add("Spravce uloh", ACT_TASKMGR); ctx_show(x, y - ctx_h()); }
        return;
    }
    for (int i = nz - 1; i >= 0; i--) if (!z[i]->minimized && in_win(z[i], x, y)) { win_raise(z[i]); return; }
    focus = NULL;
    int hit = icon_hit(x, y);
    sel_icon = hit; last_icon = -1;
    if (hit >= app_count) {
        ctx_target = file_nth(hit - app_count);
        ctx_add("Otevrit", ACT_OPENFILE); ctx_add("Smazat", ACT_DELFILE);
    } else if (hit >= 0) {
        ctx_target = hit; ctx_add("Otevrit", ACT_OPENAPP);
    } else {
        ctx_add("Novy dokument", ACT_NEWDOC); ctx_add("Spravce uloh", ACT_TASKMGR); ctx_add("O systemu", ACT_ABOUT);
    }
    ctx_show(x, y);
}

void gui_mouse(int x, int y, int btn){
    int pressed = (btn & 1) && !(prev_btn & 1);
    int rpressed = (btn & 2) && !(prev_btn & 2);
    prev_btn = btn;
    if (x != mx_now || y != my_now) { mx_now = x; my_now = y; if (menu_open || ctx_open) dirty = 1; }
    if (drag_win) {
        if (btn & 1) {
            int nx = x - drag_dx, ny = y - drag_dy;
            if (nx < 80 - drag_win->w) nx = 80 - drag_win->w;
            if (nx > scr_w - 80) nx = scr_w - 80;
            if (ny < 0) ny = 0;
            if (ny > scr_h - TASKBAR_H - TITLE_H) ny = scr_h - TASKBAR_H - TITLE_H;
            drag_win->x = nx; drag_win->y = ny; dirty = 1;
        } else drag_win = NULL;
        return;
    }
    if (cap_win) {
        Window *w = cap_win;
        if (btn & 1) { if (w->app->mouse) w->app->mouse(w, x - CLX(w), y - CLY(w), EV_DRAG); }
        else { if (w->app->mouse) w->app->mouse(w, x - CLX(w), y - CLY(w), EV_UP); cap_win = NULL; }
        dirty = 1;
        return;
    }
    if (pressed) handle_press(x, y);
    else if (rpressed) handle_rpress(x, y);
}

void gui_key(int k){
    if (focus && !focus->minimized && focus->app->key) { focus->app->key(focus, k); dirty = 1; }
}

void gui_tick(u32 now){
    if (now - clock_last > 500) {
        clock_last = now;
        RTime t; rtc_read(&t);
        if (t.min != clock_min) {
            clock_min = t.min;
            ksnprintf(clock_str, sizeof clock_str, "%02d:%02d", t.hour, t.min);
            dirty = 1;
        }
    }
    for (int i = 0; i < MAXWIN; i++) if (wins[i].used && wins[i].app->tick) wins[i].app->tick(&wins[i], now);
}

Window *gui_win(int slot){ return (slot >= 0 && slot < MAXWIN && wins[slot].used) ? &wins[slot] : NULL; }

void file_delete(int i){
    if (i < 0 || i >= MAXFILES || !files[i].used) return;
    for (int k = 0; k < MAXWIN; k++)
        if (wins[k].used && !strcmp(wins[k].app->id, "notepad") && wins[k].d.np.fileref == i + 1) gui_close(&wins[k]);
    files[i].used = 0; sel_icon = -1; last_icon = -1; dirty = 1;
}

Window *gui_open_file(int idx){
    if (idx < 0 || idx >= MAXFILES || !files[idx].used) return NULL;
    for (int k = 0; k < MAXWIN; k++)
        if (wins[k].used && !strcmp(wins[k].app->id, "notepad") && wins[k].d.np.fileref == idx + 1) {
            wins[k].minimized = 0; win_raise(&wins[k]); return &wins[k];
        }
    Window *w = gui_open(app_find("notepad"));
    if (!w) return NULL;
    NpState *np = &w->d.np; char *t = (char *)win_mem(w);
    int len = files[idx].len;
    memcpy(t, file_data(idx), len); t[len] = 0;
    np->len = len; np->cur = len; np->top = 0; np->left = 0; np->fileref = idx + 1; np->blink = 1;
    ksnprintf(w->title, 40, "%s - Poznamky", files[idx].name);
    return w;
}

void mem_info(MemInfo *m){
    u32 tot = *(volatile u32 *)0x5F0;
    if (tot < 4096) tot = 16384;
    m->total = tot; m->reserved = 1024;
    m->kernel = ((u32)__bss_end - 0x10000 + 1023) / 1024;
    m->gfx = (u32)scr_w * (u32)scr_h * 8 / 1024;
    m->apps = 0; m->docs = 0;
    for (int i = 0; i < MAXWIN; i++) if (wins[i].used) m->apps += (u32)app_memory(&wins[i]);
    for (int i = 0; i < MAXFILES; i++) if (files[i].used) m->docs += ((u32)files[i].len + 1023) / 1024;
    m->used = m->reserved + m->kernel + m->gfx + m->apps + m->docs;
    m->freekb = tot > m->used ? tot - m->used : 0;
}
