# SebaOS – vlastní 32bitový operační systém

Bootovatelný x86 systém napsaný od nuly (asembler + C, bez libc a bez cizího jádra).

## Co umí
- vlastní bootloader (BIOS, 2 stupně, načítání přes LBA)
- 32bitový protected mode, plochý model paměti
- VESA grafika 800x600x32 (záložně 1024x768 / 640x480), dvojitý buffer
- ovladače PS/2 klávesnice (US rozložení) a PS/2 myši, PIT časovač, RTC hodiny
- plocha s ikonami (dvojklik), hlavní panel, nabídka Start, hodiny, vypnutí / restart
- okna: přesouvání, zaostřování, minimalizace, maximalizace, zavření
- pravé tlačítko myši: na ploše Nový dokument / Správce úloh / O systému, na ikoně dokumentu Otevřít / Smazat
- dokumenty (uložené v RAM, po restartu zmizí) se objevují jako ikony na ploše a Poznámky je ukládají průběžně
- Správce úloh: zátěž CPU (graf), frekvence, název procesoru, odhad příkonu, rozpad RAM, seznam procesů, ukončení úlohy
- aplikace: Poznámky, Kalkulačka, Malování, Hodiny, Had, Terminál, Správce úloh, O systému

## Sestavení (Linux)
    sudo apt install build-essential nasm qemu-system-x86
    make            # vytvoří sebaos.img
    make run        # spustí v QEMU

## Spuštění
- QEMU:       qemu-system-i386 -drive file=sebaos.img,format=raw,if=ide -m 64
- VirtualBox: vytvořte VM (Other/Unknown, 64 MB RAM, BEZ EFI) a připojte sebaos.vdi jako disk
- Flash disk: sudo dd if=sebaos.img of=/dev/sdX bs=1M   (jen starý BIOS / CSM režim!)

## Struktura
    boot/boot.asm    boot sektor
    boot/stage2.asm  font z BIOSu, VESA, A20, přechod do protected mode
    kernel/sys.c     knihovna, klávesnice, myš, časovač, RTC
    kernel/gfx.c     framebuffer, text, kreslení, kurzor
    kernel/gui.c     správce oken, plocha, panel
    kernel/apps.c    všechny aplikace
