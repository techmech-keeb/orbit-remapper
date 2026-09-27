// TinyUSB configuration for Orbit: HID x2 (remapped output, config tool) + CDC (log).
// TinyUSB runs from the main loop (tud_task_ext() in main.cc), not from its own task.
#pragma once

#include "sdkconfig.h"

#define CFG_TUSB_OS            OPT_OS_FREERTOS
#define CFG_TUD_ENABLED        1
#define CFG_TUD_MAX_SPEED      OPT_MODE_FULL_SPEED
#define CFG_TUSB_DEBUG         0

#define CFG_TUD_DWC2_SLAVE_ENABLE 1
#define CFG_TUD_DWC2_DMA_ENABLE   1
#define CFG_TUD_MEM_DCACHE_ENABLE 0
#define CFG_TUSB_MEM_SECTION      TU_ATTR_ALIGNED(4) DRAM_ATTR
#define CFG_TUSB_MEM_ALIGN        TU_ATTR_ALIGNED(4)

#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_HID            2
#define CFG_TUD_HID_EP_BUFSIZE 64   // same as upstream

#define CFG_TUD_CDC            1
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 2048
#define CFG_TUD_CDC_EP_BUFSIZE 64

#define CFG_TUD_MSC    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0
