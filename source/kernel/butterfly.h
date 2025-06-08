#ifndef _KERNEL_BUTTERFLY_H
#define _KERNEL_BUTTERFLY_H 1

#include "../boot/binaries.h"
#include "../boot/bootmain.h"

#include "../common/common.h"
#include "../common/random.h"

#include "BFS/filesystem.h"
#include "BGL/demo.h"

#include "CPU//GDT/GDT.h"
#include "CPU//ISR/ISR.h"
#include "CPU//PIT/timer.h"
#include "CPU/RTC/clock.h"
#include "CPU/BIOS.h"
#include "CPU/CPU.h"
#include "CPU/HAL.h"

#include "drivers/ATA/ata.h"
#include "drivers/COM/serial.h"
#include "drivers/FBC/graphics.h"
#include "drivers/TTY/console.h"
#include "drivers/VGA/video.h"
#include "drivers/keyboard.h"
#include "drivers/speaker.h"
#include "drivers/power.h"

#include "memory/heap.h"
#include "memory/memory.h"
#include "memory/paging.h"

#include "modules/arithmetic.h"
#include "modules/calendar.h"
#include "modules/terminal.h"

#include "bugfault.h"
#include "version.h"

void dumpMultiboot(void);

#endif /* _KERNEL_BUTTERFLY_H */
