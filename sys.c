#include "kernel.h"

/* ---------------- libc-like ---------------- */
void *memset(void *d, int c, size_t n){ u8 *p = d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, size_t n){ u8 *a = d; const u8 *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n){
    u8 *a = d; const u8 *b = s;
    if (a < b) { while (n--) *a++ = *b++; }
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
int memcmp(const void *a, const void *b, size_t n){
    const u8 *x = a, *y = b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}
size_t strlen(const char *s){ size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b){ while (*a && *a == *b) { a++; b++; } return (u8)*a - (u8)*b; }
int strncmp(const char *a, const char *b, size_t n){
    for (; n; n--, a++, b++) { if (*a != *b) return (u8)*a - (u8)*b; if (!*a) return 0; }
    return 0;
}
void strcpy_s(char *d, const char *s, int n){ int i = 0; while (s[i] && i < n - 1) { d[i] = s[i]; i++; } d[i] = 0; }

void copy32(u32 *d, const u32 *s, int n){ __asm__ volatile("cld; rep movsl":"+D"(d),"+S"(s),"+c"(n)::"memory"); }
void fill32(u32 *d, u32 v, int n){ __asm__ volatile("cld; rep stosl":"+D"(d),"+c"(n):"a"(v):"memory"); }

int ksnprintf(char *buf, int n, const char *fmt, ...){
    __builtin_va_list ap; __builtin_va_start(ap, fmt);
    int len = 0;
#define PUT(ch) do { if (len < n - 1) buf[len] = (ch); len++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        fmt++;
        int zero = 0, width = 0;
        if (*fmt == '0') { zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        char tmp[16]; int tl = 0; const char *str = NULL;
        switch (*fmt) {
        case 'd': case 'u': case 'x': {
            u32 u; int neg = 0;
            if (*fmt == 'd') { int v = __builtin_va_arg(ap, int); if (v < 0) { neg = 1; u = (u32)(-v); } else u = (u32)v; }
            else u = __builtin_va_arg(ap, u32);
            u32 base = (*fmt == 'x') ? 16 : 10;
            if (u == 0) tmp[tl++] = '0';
            while (u) { tmp[tl++] = "0123456789abcdef"[u % base]; u /= base; }
            if (neg) tmp[tl++] = '-';
            for (int p = tl; p < width; p++) PUT(zero ? '0' : ' ');
            while (tl) PUT(tmp[--tl]);
            break; }
        case 's': str = __builtin_va_arg(ap, const char *); if (!str) str = "(null)";
            { int sl = (int)strlen(str); for (int p = sl; p < width; p++) PUT(' '); while (*str) { PUT(*str); str++; } }
            break;
        case 'c': PUT((char)__builtin_va_arg(ap, int)); break;
        case '%': PUT('%'); break;
        default: PUT('%'); if (*fmt) PUT(*fmt); else fmt--; break;
        }
    }
    if (n > 0) buf[len < n ? len : n - 1] = 0;
    __builtin_va_end(ap);
    return len;
}

/* ---------------- 8042: klavesnice + mys ---------------- */
#define QN 64
static int kq[QN], kqh, kqt;
static MouseEv mq[QN]; static int mqh, mqt;
static int kshift, kcaps, kext;
static int mouse_x, mouse_y, mouse_btn;
static u8 mpk[3]; static int mi;

static void kpush(int k){ int n = (kqt + 1) % QN; if (n != kqh) { kq[kqt] = k; kqt = n; } }
static void mpush(void){ int n = (mqt + 1) % QN; if (n != mqh) { mq[mqt].x = mouse_x; mq[mqt].y = mouse_y; mq[mqt].btn = mouse_btn; mqt = n; } }

static const char sc_norm[64] = {
 0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
 'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
 'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
 'b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0 };
static const char sc_shift[64] = {
 0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
 'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
 'B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0 };
/* numpad 0x47..0x53 */
static const char sc_pad[13] = { '7','8','9','-','4','5','6','+','1','2','3','0','.' };

static void kbd_byte(u8 sc){
    if (sc == 0xE0) { kext = 1; return; }
    int rel = sc & 0x80; sc &= 0x7F;
    int ext = kext; kext = 0;
    if (sc == 0x2A || sc == 0x36) { kshift = !rel; return; }
    if (sc == 0x1D || sc == 0x38) return;
    if (rel) return;
    if (ext) {
        switch (sc) {
        case 0x48: kpush(KEY_UP); break;   case 0x50: kpush(KEY_DOWN); break;
        case 0x4B: kpush(KEY_LEFT); break; case 0x4D: kpush(KEY_RIGHT); break;
        case 0x47: kpush(KEY_HOME); break; case 0x4F: kpush(KEY_END); break;
        case 0x53: kpush(KEY_DEL); break;  case 0x49: kpush(KEY_PGUP); break;
        case 0x51: kpush(KEY_PGDN); break; case 0x1C: kpush('\n'); break;
        case 0x35: kpush('/'); break;
        }
        return;
    }
    if (sc == 0x3A) { kcaps ^= 1; return; }
    if (sc >= 0x47 && sc <= 0x53) { kpush(sc_pad[sc - 0x47]); return; }
    if (sc >= 64) return;
    char c = kshift ? sc_shift[sc] : sc_norm[sc];
    if (kcaps) { if (c >= 'a' && c <= 'z') c -= 32; else if (c >= 'A' && c <= 'Z') c += 32; }
    if (c) kpush((u8)c);
}

static void mouse_byte(u8 d){
    if (mi == 0 && !(d & 8)) return;
    mpk[mi++] = d;
    if (mi < 3) return;
    mi = 0;
    if (mpk[0] & 0xC0) return;
    int dx = mpk[1], dy = mpk[2];
    if (mpk[0] & 0x10) dx -= 256;
    if (mpk[0] & 0x20) dy -= 256;
    mouse_x += dx; mouse_y -= dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= scr_w) mouse_x = scr_w - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= scr_h) mouse_y = scr_h - 1;
    mouse_btn = mpk[0] & 7;
    mpush();
}

static void kbc_wait_w(void){ int t = 200000; while ((inb(0x64) & 2) && t--) ; }
static void kbc_wait_r(void){ int t = 200000; while (!(inb(0x64) & 1) && t--) ; }
static void mouse_write(u8 v){ kbc_wait_w(); outb(0x64, 0xD4); kbc_wait_w(); outb(0x60, v); }
static u8 mouse_ack(void){ kbc_wait_r(); return inb(0x60); }

void input_init(void){
    while (inb(0x64) & 1) inb(0x60);
    kbc_wait_w(); outb(0x64, 0xAE);          /* povolit klavesnici */
    kbc_wait_w(); outb(0x64, 0xA8);          /* povolit mys */
    kbc_wait_w(); outb(0x64, 0x20);
    kbc_wait_r(); u8 cfg = inb(0x60);
    cfg &= ~0x30;                            /* hodiny klavesnice i mysi povoleny */
    kbc_wait_w(); outb(0x64, 0x60);
    kbc_wait_w(); outb(0x60, cfg);
    mouse_write(0xF6); mouse_ack();          /* vychozi nastaveni */
    mouse_write(0xF4); mouse_ack();          /* zapnout hlaseni */
    mouse_x = scr_w / 2; mouse_y = scr_h / 2;
    /* PIT kanal 0, mod 2, reload 65536 */
    outb(0x43, 0x34); outb(0x40, 0); outb(0x40, 0);
}

void input_poll(void){
    for (int g = 0; g < 64; g++) {
        u8 st = inb(0x64);
        if (!(st & 1)) break;
        u8 d = inb(0x60);
        if (st & 0x20) mouse_byte(d); else kbd_byte(d);
    }
}
int key_get(void){ if (kqh == kqt) return -1; int k = kq[kqh]; kqh = (kqh + 1) % QN; return k; }
int mouse_get(MouseEv *e){ if (mqh == mqt) return 0; *e = mq[mqh]; mqh = (mqh + 1) % QN; return 1; }

/* ---------------- cas ---------------- */
u32 millis(void){
    static u32 last, frac, ms;
    outb(0x43, 0x00);
    u32 lo = inb(0x40), hi = inb(0x40);
    u32 c = lo | (hi << 8);
    u32 delta = (last - c) & 0xFFFF;
    last = c;
    frac += delta;
    ms += frac / 1193; frac %= 1193;
    return ms;
}
static int cmos(int r){ outb(0x70, r); return inb(0x71); }
static int bcd(int v){ return (v & 15) + (v >> 4) * 10; }
void rtc_read(RTime *t){
    int tries = 0;
    while ((cmos(0x0A) & 0x80) && tries++ < 100000) ;
    int s = cmos(0), m = cmos(2), h = cmos(4), d = cmos(7), mo = cmos(8), y = cmos(9), b = cmos(0x0B);
    if (!(b & 4)) { s = bcd(s); m = bcd(m); h = bcd(h & 0x7F) | (h & 0x80); d = bcd(d); mo = bcd(mo); y = bcd(y); }
    if (!(b & 2) && (h & 0x80)) h = ((h & 0x7F) % 12) + 12;
    t->sec = s; t->min = m; t->hour = h; t->day = d; t->mon = mo; t->year = 2000 + y;
}

/* ---------------- system ---------------- */
void sys_reboot(void){
    kbc_wait_w(); outb(0x64, 0xFE);
    for (;;) __asm__ volatile("cli; hlt");
}
void sys_shutdown(void){
    outw(0x604, 0x2000);    /* QEMU */
    outw(0xB004, 0x2000);   /* stare QEMU / Bochs */
    outw(0x4004, 0x3400);   /* VirtualBox */
    for (;;) __asm__ volatile("cli; hlt");
}
void cpu_vendor(char *out){
    u32 a, b, c, d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0));
    memcpy(out, &b, 4); memcpy(out + 4, &d, 4); memcpy(out + 8, &c, 4); out[12] = 0;
}
static u32 rng = 2463534242u;
void srand32(u32 s){ rng ^= s * 2654435761u; if (!rng) rng = 1; }
u32 rand32(void){ rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* ---------------- vykon CPU ---------------- */
int perf_load; int perf_hist[PERF_N]; u32 perf_mhz, perf_seq;
static u32 pf_busy, pf_tsc, pf_ms; static int pf_init;
void perf_work_add(u32 c){ pf_busy += c; }
void perf_update(u32 now){
    if (!pf_init) { pf_init = 1; pf_tsc = rdtsc32(); pf_ms = now; return; }
    if (now - pf_ms < 500) return;
    u32 t = rdtsc32(), total = t - pf_tsc, ms = now - pf_ms;
    pf_tsc = t; pf_ms = now;
    if (!total) return;
    u32 ld = pf_busy / (total / 100 + 1); if (ld > 100) ld = 100;
    pf_busy = 0; perf_load = (int)ld; perf_mhz = total / (ms * 1000);
    memmove(perf_hist, perf_hist + 1, (PERF_N - 1) * sizeof(int));
    perf_hist[PERF_N - 1] = (int)ld; perf_seq++;
}
void cpu_brand(char *out){
    u32 a, b, c, d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000000));
    if (a < 0x80000004) { cpu_vendor(out); return; }
    char tmp[49];
    for (int i = 0; i < 3; i++) {
        __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x80000002 + i));
        memcpy(tmp + i * 16, &a, 4); memcpy(tmp + i * 16 + 4, &b, 4);
        memcpy(tmp + i * 16 + 8, &c, 4); memcpy(tmp + i * 16 + 12, &d, 4);
    }
    tmp[48] = 0;
    char *p = tmp; while (*p == ' ') p++;
    strcpy_s(out, p, 49);
}
