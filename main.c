#include "kernel.h"

void kernel_main(void){
    gfx_init();
    input_init();
    gui_init();
    RTime t; rtc_read(&t);
    srand32(millis() + t.sec * 1000 + t.min * 60000);
    gui_open(app_find("about"));

    int mcx = scr_w / 2, mcy = scr_h / 2, curx = mcx, cury = mcy;
    gui_render();
    gfx_present_all();
    cursor_draw(curx, cury);

    for (;;) {
        u32 t0 = rdtsc32();
        int work = 0;
        input_poll();
        int k;
        while ((k = key_get()) >= 0) { gui_key(k); work = 1; }
        MouseEv e;
        while (mouse_get(&e)) { gui_mouse(e.x, e.y, e.btn); mcx = e.x; mcy = e.y; work = 1; }
        u32 now = millis();
        gui_tick(now);
        if (gui_need_redraw()) {
            gui_render();
            gfx_present_all();
            cursor_draw(mcx, mcy);
            curx = mcx; cury = mcy; work = 1;
        } else if (mcx != curx || mcy != cury) {
            cursor_erase(curx, cury);
            cursor_draw(mcx, mcy);
            curx = mcx; cury = mcy; work = 1;
        }
        if (work) perf_work_add(rdtsc32() - t0);
        perf_update(now);
        __asm__ volatile("pause");
    }
}
