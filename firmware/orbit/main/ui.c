// The M5Dial screen. See ui.h.
//
// Layout on the 240 x 240 round panel: an outer ring whose colour is the
// state at a glance (green: everything bonded is connected, blue: waiting,
// yellow: pairing, red: a question for the user), two device cards in the
// middle, one line of state below them, and the small numbers (USB, LAT,
// heap) at the bottom. The dial moves the highlight between the cards and
// the menu items; the button opens the menu, confirms, or answers an
// approval question.

#include <stdio.h>
#include <string.h>
#include "driver/pulse_cnt.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ble.h"
#include "display.h"
#include "ledger.h"
#include "log.h"
#include "ui.h"

#define PIN_ENC_A 40 // M5Dial rotary encoder (docs.m5stack.com/en/core/M5Dial)
#define PIN_ENC_B 41
#define ENC_STEPS_PER_DETENT 4 // quadrature edges per click (to be checked on the device)

#define W 240
#define H 240
#define DRAW_LINES 30 // draw buffer: W x DRAW_LINES x 2 bytes, twice

// Colours
#define C_BG       0x101418
#define C_CARD     0x1e2630
#define C_CARD_SEL 0x2b3a4a
#define C_TEXT     0xe8ecf0
#define C_DIM      0x8a949e
#define C_GREEN    0x3ddc84
#define C_BLUE     0x4fc3f7
#define C_YELLOW   0xffd54f
#define C_RED      0xff5252
#define C_GREY     0x3a4450

static const char* TAG_PREFIX = "M1 EVT";

static lv_display_t* disp;
static pcnt_unit_handle_t encoder;
static int encoder_last;
static volatile int press_pending; // set by orbit_ui_press(), consumed by the LVGL timer

// Widgets
static lv_obj_t* ring;
static lv_obj_t* title;
static lv_obj_t* card[ORBIT_MAX_DEVS];
static lv_obj_t* card_name[ORBIT_MAX_DEVS];
static lv_obj_t* card_line[ORBIT_MAX_DEVS];
static lv_obj_t* card_dot[ORBIT_MAX_DEVS];
static lv_obj_t* state_line;
static lv_obj_t* footer;
static lv_obj_t* menu;
static lv_obj_t* menu_item[6];
static lv_obj_t* notice; // full-screen message

static orbit_ui_state_t last; // latest snapshot (LVGL lock held while used)
static int selected;          // card index, or -1 for the ring (nothing)
static int menu_open;         // 0: closed, 1: open
static int menu_sel;
static int menu_count;
static int confirm_forget;    // port awaiting a second press, 0: none
static int64_t menu_opened_us;

typedef enum { M_PAIR, M_STOP, M_APPROVE, M_FORGET, M_CLOSE } menu_kind_t;
static menu_kind_t menu_kind[6];
static int menu_port[6];

static void style_card(lv_obj_t* o, bool sel) {
    lv_obj_set_style_bg_color(o, lv_color_hex(sel ? C_CARD_SEL : C_CARD), 0);
    lv_obj_set_style_border_color(o, lv_color_hex(sel ? C_BLUE : C_GREY), 0);
    lv_obj_set_style_border_width(o, sel ? 2 : 1, 0);
}

static lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

static void build(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_scrollable(scr, false);

    ring = lv_arc_create(scr);
    lv_obj_set_size(ring, W, H);
    lv_obj_center(ring);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_range(ring, 0, 100);
    lv_arc_set_value(ring, 100);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(ring, false);
    lv_obj_set_style_arc_width(ring, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, lv_color_hex(C_GREY), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(C_GREY), LV_PART_INDICATOR);

    title = make_label(scr, &lv_font_montserrat_14, C_DIM);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        card[i] = lv_obj_create(scr);
        lv_obj_set_size(card[i], 168, 46);
        lv_obj_align(card[i], LV_ALIGN_CENTER, 0, -34 + i * 52);
        lv_obj_set_style_radius(card[i], 10, 0);
        lv_obj_set_style_pad_all(card[i], 6, 0);
        lv_obj_set_scrollable(card[i], false);
        style_card(card[i], false);
        card_dot[i] = lv_obj_create(card[i]);
        lv_obj_set_size(card_dot[i], 10, 10);
        lv_obj_set_style_radius(card_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(card_dot[i], 0, 0);
        lv_obj_set_style_bg_color(card_dot[i], lv_color_hex(C_GREY), 0);
        lv_obj_align(card_dot[i], LV_ALIGN_LEFT_MID, 0, 0);
        card_name[i] = make_label(card[i], &lv_font_montserrat_16, C_TEXT);
        lv_obj_set_width(card_name[i], 136);
        lv_label_set_long_mode(card_name[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(card_name[i], LV_ALIGN_TOP_LEFT, 16, -2);
        card_line[i] = make_label(card[i], &lv_font_montserrat_12, C_DIM);
        lv_obj_align(card_line[i], LV_ALIGN_BOTTOM_LEFT, 16, 2);
    }

    state_line = make_label(scr, &lv_font_montserrat_14, C_TEXT);
    lv_obj_align(state_line, LV_ALIGN_CENTER, 0, 46);

    footer = make_label(scr, &lv_font_montserrat_12, C_DIM);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -22);

    menu = lv_obj_create(scr);
    lv_obj_set_size(menu, 180, 150);
    lv_obj_center(menu);
    lv_obj_set_style_radius(menu, 12, 0);
    lv_obj_set_style_bg_color(menu, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_border_color(menu, lv_color_hex(C_BLUE), 0);
    lv_obj_set_style_border_width(menu, 2, 0);
    lv_obj_set_style_pad_all(menu, 8, 0);
    lv_obj_set_scrollable(menu, false);
    for (int i = 0; i < 6; i++) {
        menu_item[i] = make_label(menu, &lv_font_montserrat_16, C_TEXT);
        lv_obj_align(menu_item[i], LV_ALIGN_TOP_LEFT, 4, i * 26);
    }
    lv_obj_set_hidden(menu, true);

    notice = lv_obj_create(scr);
    lv_obj_set_size(notice, W, H);
    lv_obj_center(notice);
    lv_obj_set_style_bg_color(notice, lv_color_hex(C_BG), 0);
    lv_obj_set_style_border_width(notice, 0, 0);
    lv_obj_set_style_radius(notice, 0, 0);
    lv_obj_set_hidden(notice, true);
}

static void render_menu(void) {
    menu_count = 0;
    if (last.approval.wanted) {
        menu_kind[menu_count] = M_APPROVE;
        menu_count++;
    }
    menu_kind[menu_count++] = last.pairing ? M_STOP : M_PAIR;
    if (selected >= 0 && last.dev[selected].connected && last.dev[selected].port != 0) {
        menu_kind[menu_count] = M_FORGET;
        menu_port[menu_count] = last.dev[selected].port;
        menu_count++;
    }
    menu_kind[menu_count++] = M_CLOSE;
    if (menu_sel >= menu_count) {
        menu_sel = menu_count - 1;
    }
    for (int i = 0; i < 6; i++) {
        if (i >= menu_count) {
            lv_label_set_text(menu_item[i], "");
            continue;
        }
        char text[48];
        switch (menu_kind[i]) {
        case M_PAIR:
            snprintf(text, sizeof(text), LV_SYMBOL_PLUS "  Pair new device");
            break;
        case M_STOP:
            snprintf(text, sizeof(text), LV_SYMBOL_CLOSE "  Stop pairing");
            break;
        case M_APPROVE:
            snprintf(text, sizeof(text), LV_SYMBOL_OK "  Allow port %d", last.approval.port);
            break;
        case M_FORGET:
            snprintf(text, sizeof(text), confirm_forget == menu_port[i] ? LV_SYMBOL_WARNING "  Really forget?"
                                                                        : LV_SYMBOL_TRASH "  Forget port %d",
                     menu_port[i]);
            break;
        default:
            snprintf(text, sizeof(text), "Close");
            break;
        }
        lv_label_set_text(menu_item[i], text);
        lv_obj_set_style_text_color(menu_item[i], lv_color_hex(i == menu_sel ? C_BLUE : C_TEXT), 0);
    }
}

static void menu_act(void) {
    switch (menu_kind[menu_sel]) {
    case M_PAIR:
        olog("%s t=%.3f screen: pair new device\n", TAG_PREFIX, esp_timer_get_time() / 1e6);
        orbit_ble_pair_new_device();
        break;
    case M_STOP:
        olog("%s t=%.3f screen: stop pairing\n", TAG_PREFIX, esp_timer_get_time() / 1e6);
        orbit_ble_stop_pairing();
        break;
    case M_APPROVE:
        olog("%s t=%.3f screen: approve\n", TAG_PREFIX, esp_timer_get_time() / 1e6);
        orbit_ble_approve();
        break;
    case M_FORGET:
        if (confirm_forget != menu_port[menu_sel]) {
            confirm_forget = menu_port[menu_sel]; // ask once more
            render_menu();
            return;
        }
        olog("%s t=%.3f screen: forget port %d\n", TAG_PREFIX, esp_timer_get_time() / 1e6, menu_port[menu_sel]);
        orbit_ble_forget(menu_port[menu_sel]);
        break;
    default:
        break;
    }
    confirm_forget = 0;
    menu_open = 0;
    lv_obj_set_hidden(menu, true);
}

// Every 50 ms on the LVGL task: the dial and the button.
static void poll_input(lv_timer_t* t) {
    int count = 0;
    if (encoder != NULL) {
        pcnt_unit_get_count(encoder, &count);
    }
    int steps = (count - encoder_last) / ENC_STEPS_PER_DETENT;
    if (steps != 0) {
        encoder_last += steps * ENC_STEPS_PER_DETENT;
        if (menu_open) {
            menu_sel = (menu_sel + steps % menu_count + menu_count) % menu_count;
            confirm_forget = 0;
            render_menu();
        } else {
            int n = ORBIT_MAX_DEVS + 1; // the cards and "nothing"
            selected = ((selected + 1 + steps) % n + n) % n - 1;
            for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
                style_card(card[i], i == selected);
            }
        }
    }
    if (menu_open && esp_timer_get_time() - menu_opened_us > 15 * 1000000) {
        menu_open = 0; // left open: close it
        confirm_forget = 0;
        lv_obj_set_hidden(menu, true);
    }
    if (press_pending) {
        press_pending = 0;
        if (menu_open) {
            menu_act();
        } else if (last.approval.wanted) {
            // G-2: the short press answers the open question, as before the screen.
            olog("%s t=%.3f screen: approve (button)\n", TAG_PREFIX, esp_timer_get_time() / 1e6);
            orbit_ble_approve();
        } else {
            menu_open = 1;
            menu_sel = 0;
            confirm_forget = 0;
            menu_opened_us = esp_timer_get_time();
            render_menu();
            lv_obj_set_hidden(menu, false);
        }
    }
}

static bool encoder_init(void) {
    const pcnt_unit_config_t unit_cfg = {
        .low_limit = -1000,
        .high_limit = 1000,
        .flags.accum_count = true,
    };
    if (pcnt_new_unit(&unit_cfg, &encoder) != ESP_OK) {
        return false;
    }
    const pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 1000 };
    pcnt_unit_set_glitch_filter(encoder, &filter);
    pcnt_chan_config_t ch_cfg = { .edge_gpio_num = PIN_ENC_A, .level_gpio_num = PIN_ENC_B };
    pcnt_channel_handle_t ch_a, ch_b;
    if (pcnt_new_channel(encoder, &ch_cfg, &ch_a) != ESP_OK) {
        return false;
    }
    ch_cfg.edge_gpio_num = PIN_ENC_B;
    ch_cfg.level_gpio_num = PIN_ENC_A;
    if (pcnt_new_channel(encoder, &ch_cfg, &ch_b) != ESP_OK) {
        return false;
    }
    // Full quadrature decoding: both edges of both lines, direction by the other line.
    pcnt_channel_set_edge_action(ch_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(ch_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(ch_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(ch_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_unit_enable(encoder);
    pcnt_unit_clear_count(encoder);
    pcnt_unit_start(encoder);
    return true;
}

bool orbit_ui_init(void) {
    if (display_panel() == NULL) {
        return false;
    }
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;  // below the NimBLE host (CPU0) and the main loop (CPU1)
    port_cfg.task_affinity = 0;  // CPU0, away from the main loop
    port_cfg.task_stack = 8192;
    port_cfg.timer_period_ms = 10;
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        return false;
    }
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = display_io(),
        .panel_handle = display_panel(),
        .buffer_size = W * DRAW_LINES,
        .double_buffer = true,
        .hres = W,
        .vres = H,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_dma = true, .swap_bytes = true },
    };
    disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        return false;
    }
    if (!encoder_init()) {
        olog("%s t=%.3f screen: encoder init failed, dial disabled\n", TAG_PREFIX, esp_timer_get_time() / 1e6);
    }
    selected = -1;
    if (lvgl_port_lock(1000)) {
        build();
        lv_timer_create(poll_input, 50, NULL);
        lvgl_port_unlock();
    }
    return true;
}

static uint32_t ring_color(const orbit_ui_state_t* s, const char** text, char* buf, int len) {
    if (s->approval.wanted) {
        snprintf(buf, len, LV_SYMBOL_WARNING " Port %d wants to pair: press to allow (%ds)", s->approval.port,
                 s->approval.remaining_s);
        *text = buf;
        return C_RED;
    }
    if (s->full) {
        snprintf(buf, len, "All %d ports used: forget one", s->max_devices);
        *text = buf;
        return C_RED;
    }
    if (s->pairing) {
        if (s->pairing_remaining_s < 0) {
            snprintf(buf, len, "Pairing: turn on a device");
        } else {
            snprintf(buf, len, "Pairing (%ds): turn on a device", s->pairing_remaining_s);
        }
        *text = buf;
        return C_YELLOW;
    }
    if (s->approval.granted_port != 0) {
        snprintf(buf, len, "Port %d may pair again (%ds)", s->approval.granted_port, s->approval.granted_remaining_s);
        *text = buf;
        return C_YELLOW;
    }
    int connected = 0;
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        connected += s->dev[i].connected;
    }
    if (s->duplicates) {
        snprintf(buf, len, "Two rows look alike: check the ledger");
        *text = buf;
        return C_YELLOW;
    }
    if (connected == 0) {
        snprintf(buf, len, s->devices == 0 ? "No device paired yet" : "Waiting for %d device(s)", s->devices);
        *text = buf;
        return s->devices == 0 ? C_GREY : C_BLUE;
    }
    if (connected < s->devices && connected < ORBIT_MAX_DEVS) {
        snprintf(buf, len, "%d connected, waiting for more", connected);
        *text = buf;
        return C_BLUE;
    }
    snprintf(buf, len, "%d device(s) connected", connected);
    *text = buf;
    return C_GREEN;
}

void orbit_ui_update(const orbit_ui_state_t* s) {
    if (disp == NULL || !lvgl_port_lock(50)) {
        return; // never hold up the status task
    }
    last = *s;
    char buf[64];
    const char* text = "";
    uint32_t color = ring_color(s, &text, buf, sizeof(buf));
    lv_obj_set_style_arc_color(ring, lv_color_hex(color), LV_PART_INDICATOR);
    lv_label_set_text(state_line, text);
    lv_obj_set_style_text_color(state_line, lv_color_hex(color == C_GREY ? C_DIM : color), 0);

    char t[48];
    snprintf(t, sizeof(t), "ORBIT  %s  %d/%d", s->version, s->devices, s->max_devices);
    lv_label_set_text(title, t);

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        const orbit_ui_dev_t* d = &s->dev[i];
        if (!d->connected) {
            lv_label_set_text(card_name[i], "--");
            lv_label_set_text(card_line[i], "");
            lv_obj_set_style_bg_color(card_dot[i], lv_color_hex(C_GREY), 0);
            continue;
        }
        lv_label_set_text(card_name[i], d->name);
        char line[48];
        snprintf(line, sizeof(line), "P%d  %.2f ms  L%u  %u/s", d->port, d->itvl_ms, d->latency, d->reports);
        lv_label_set_text(card_line[i], line);
        uint32_t dot = !d->encrypted ? C_YELLOW : d->subscribed == 0 ? C_YELLOW : d->reports > 0 ? C_GREEN : C_BLUE;
        lv_obj_set_style_bg_color(card_dot[i], lv_color_hex(dot), 0);
    }

    char f[64];
    if (s->lat_max_ms > 0) {
        snprintf(f, sizeof(f), "USB %s  LAT %.1f/%.1f ms  %uK", s->usb, s->lat_avg_ms, s->lat_max_ms,
                 s->heap_free / 1024);
    } else {
        snprintf(f, sizeof(f), "USB %s  %uK  %s", s->usb, s->heap_free / 1024, s->last_event);
    }
    lv_label_set_text(footer, f);
    if (menu_open) {
        render_menu();
    }
    lvgl_port_unlock();
}

void orbit_ui_press(void) {
    press_pending = 1;
}

void orbit_ui_message(const char* line1, const char* line2) {
    if (disp == NULL || !lvgl_port_lock(500)) {
        return;
    }
    lv_obj_t* l1 = make_label(notice, &lv_font_montserrat_20, C_YELLOW);
    lv_label_set_text(l1, line1);
    lv_obj_align(l1, LV_ALIGN_CENTER, 0, -14);
    lv_obj_t* l2 = make_label(notice, &lv_font_montserrat_14, C_TEXT);
    lv_label_set_text(l2, line2);
    lv_obj_align(l2, LV_ALIGN_CENTER, 0, 14);
    lv_obj_set_hidden(notice, false);
    lv_obj_move_foreground(notice);
    lv_refr_now(disp);
    // Keep the lock: nothing else draws before the reboot.
}
