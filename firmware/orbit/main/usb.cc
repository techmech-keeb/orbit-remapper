// USB device side: HID 0 (remapped output), HID 1 (config tool), CDC (log).
//
// Descriptors and HID callbacks follow upstream's USB build,
// firmware/hid-remapper/firmware/src/tinyusb_stuff.cc (MIT, Copyright (c)
// 2022 Jacek Fedorynski, Copyright (c) 2019 Ha Thach), with a CDC interface
// added. TinyUSB is driven from the main loop (tud_task_ext() in main.cc), so
// these callbacks run on the main loop's task and may call the core, as they
// do upstream.

#include <cstring>

#include "esp_log.h"
#include "esp_private/usb_phy.h"
#include "tusb.h"

#include "config.h"
#include "globals.h"
#include "our_descriptor.h"
#include "platform.h"
#include "remapper.h"

#include "log.h"
#include "orbit.h"

// Upstream's IDs, so that the web config tool finds the device (M1). Orbit's
// own IDs are an open question.
#define USB_VID 0xCAFE
#define USB_PID 0xBAF2

#define ITF_HID0     0
#define ITF_HID1     1
#define ITF_CDC      2 // + data interface 3
#define ITF_COUNT    4

// The ESP32-S3's USB controller has TX FIFOs for IN endpoints 0-4 only
// (TinyUSB's dwc2_esp32.h: ep_in_count = 5, and it gives IN endpoint N
// FIFO N). With the CDC data IN on 0x85 nothing was ever sent on it
// (728105c report), so keep every IN endpoint number at 4 or below.
#define EP_HID0_IN   0x81
#define EP_HID0_OUT  0x01
#define EP_HID1_IN   0x82
#define EP_CDC_NOTIF 0x83
#define EP_CDC_OUT   0x04
#define EP_CDC_IN    0x84

#define STR_CDC 4

static tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    // Interface Association Descriptor device class: needed for the CDC
    // interface pair in a composite device.
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

#define CONFIG_LEN(hid0_len) (TUD_CONFIG_DESC_LEN + (hid0_len) + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

#define HID1_AND_CDC                                                                                       \
    TUD_HID_DESCRIPTOR(ITF_HID1, 0, HID_ITF_PROTOCOL_NONE, config_report_descriptor_length, EP_HID1_IN,  \
                       CFG_TUD_HID_EP_BUFSIZE, 1),                                                       \
    TUD_CDC_DESCRIPTOR(ITF_CDC, STR_CDC, EP_CDC_NOTIF, 8, EP_CDC_OUT, EP_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE)

// One configuration per entry of upstream's our_descriptors[], as upstream.
static const uint8_t configuration_descriptor0[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN(TUD_HID_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_HID0, 0, HID_ITF_PROTOCOL_KEYBOARD, our_descriptors[0].descriptor_length, EP_HID0_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    HID1_AND_CDC,
};

static const uint8_t configuration_descriptor1[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN(TUD_HID_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_HID0, 0, HID_ITF_PROTOCOL_KEYBOARD, our_descriptors[1].descriptor_length, EP_HID0_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    HID1_AND_CDC,
};

static const uint8_t configuration_descriptor2[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN(TUD_HID_INOUT_DESC_LEN), 0, 100),
    TUD_HID_INOUT_DESCRIPTOR(ITF_HID0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[2].descriptor_length, EP_HID0_OUT, EP_HID0_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    HID1_AND_CDC,
};

static const uint8_t configuration_descriptor3[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN(TUD_HID_DESC_LEN), 0, 100),
    TUD_HID_DESCRIPTOR(ITF_HID0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[3].descriptor_length, EP_HID0_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    HID1_AND_CDC,
};

static const uint8_t configuration_descriptor4[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN(TUD_HID_INOUT_DESC_LEN), 0, 100),
    TUD_HID_INOUT_DESCRIPTOR(ITF_HID0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[4].descriptor_length, EP_HID0_OUT, EP_HID0_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    HID1_AND_CDC,
};

static const uint8_t configuration_descriptor5[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN(TUD_HID_DESC_LEN), 0, 100),
    TUD_HID_DESCRIPTOR(ITF_HID0, 0, HID_ITF_PROTOCOL_NONE, our_descriptors[5].descriptor_length, EP_HID0_IN, CFG_TUD_HID_EP_BUFSIZE, 1),
    HID1_AND_CDC,
};

static const uint8_t* configuration_descriptors[] = {
    configuration_descriptor0,
    configuration_descriptor1,
    configuration_descriptor2,
    configuration_descriptor3,
    configuration_descriptor4,
    configuration_descriptor5,
};

static const char* string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, // 0: English (0x0409)
    "Orbit (ESP32-S3)",           // 1: Manufacturer
    "HID Remapper XXXX",          // 2: Product; XXXX is filled from the unique ID, as upstream
    "123456789012",               // 3: Serial number, filled from the unique ID
    "Orbit log",                  // 4: CDC interface
};

uint8_t const* tud_descriptor_device_cb() {
    if ((our_descriptor->vid != 0) && (our_descriptor->pid != 0)) {
        desc_device.idVendor = our_descriptor->vid;
        desc_device.idProduct = our_descriptor->pid;
    }
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return configuration_descriptors[our_descriptor->idx];
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    if (itf == 0) {
        return our_descriptor->descriptor;
    } else if (itf == 1) {
        return config_report_descriptor;
    }
    return NULL;
}

static uint16_t _desc_str[32];

static const char id_chars[33] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }

        if (index == 2) {
            uint64_t unique_id = get_unique_id();
            for (uint8_t i = 0; i < 4; i++) {
                _desc_str[1 + chr_count - 4 + i] = id_chars[(unique_id >> (15 - i * 5)) & 0x1F];
            }
        }
        if (index == 3) {
            uint64_t unique_id = get_unique_id();
            for (uint8_t i = 0; i < 12; i++) {
                _desc_str[1 + i] = id_chars[(unique_id >> (55 - i * 5)) & 0x1F];
            }
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    if (itf == 0) {
        return handle_get_report0(report_id, buffer, reqlen);
    } else {
        return handle_get_report1(report_id, buffer, reqlen);
    }
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (itf == 0) {
        if ((report_id == 0) && (report_type == 0) && (bufsize > 0)) {
            report_id = buffer[0];
            buffer++;
        }
        handle_set_report0(report_id, buffer, bufsize);
    } else {
        handle_set_report1(report_id, buffer, bufsize);
    }
}

void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    olog("M1 EVT t=%.3f usb set_protocol itf=%u %s\n", orbit_now_s(), instance,
         protocol == HID_PROTOCOL_BOOT ? "boot" : "report");
    boot_protocol_keyboard = (protocol == HID_PROTOCOL_BOOT);
    boot_protocol_updated = true;
}

void tud_mount_cb() {
    olog("M1 EVT t=%.3f usb mounted\n", orbit_now_s());
    reset_resolution_multiplier();
    if (boot_protocol_keyboard) {
        boot_protocol_keyboard = false;
        boot_protocol_updated = true;
    }
}

void tud_umount_cb() {
    olog("M1 EVT t=%.3f usb unmounted\n", orbit_now_s());
}

void tud_suspend_cb(bool remote_wakeup_en) {
    olog("M1 EVT t=%.3f usb suspended remote_wakeup_enabled=%d\n", orbit_now_s(), remote_wakeup_en);
}

void tud_resume_cb() {
    olog("M1 EVT t=%.3f usb resumed\n", orbit_now_s());
}

// Third way back to flashing, needing neither the config tool nor the
// button: open the log port at 1200 bps (the Arduino "1200 bps touch").
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding) {
    if (p_line_coding->bit_rate == 1200) {
        orbit_request_download_mode("log port opened at 1200 bps");
    }
}

// Same as upstream's USB build (firmware/src/main.cc), plus the send time
// for the M1 LAT line.
bool do_send_report(uint8_t interface, const uint8_t* report_with_id, uint8_t len) {
    if (tud_suspended() &&
        (our_descriptor->should_cause_wakeup != nullptr) &&
        our_descriptor->should_cause_wakeup(report_with_id[0], report_with_id + 1, len - 1)) {
        bool ok = tud_remote_wakeup();
        olog("M1 EVT t=%.3f usb remote_wakeup %s\n", orbit_now_s(), ok ? "sent" : "refused");
    } else {
        tud_hid_n_report(interface, report_with_id[0], report_with_id + 1, len - 1);
        if (interface == 0) {
            orbit_report_sent();
        }
    }
    return true;  // XXX? (as upstream)
}

static usb_phy_handle_t phy;

void orbit_usb_init() {
    usb_phy_config_t phy_conf = {};
    phy_conf.controller = USB_PHY_CTRL_OTG;
    phy_conf.target = USB_PHY_TARGET_INT;
    phy_conf.otg_mode = USB_OTG_MODE_DEVICE;
    phy_conf.otg_speed = USB_PHY_SPEED_FULL;
    ESP_ERROR_CHECK(usb_new_phy(&phy_conf, &phy));

    const tusb_rhport_init_t init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    if (!tusb_rhport_init(0, &init)) {
        olog("M1 EVT t=%.3f usb init failed\n", orbit_now_s());
    }
}
