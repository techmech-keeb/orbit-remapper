// The M5Dial screen. See ui.h.
//
// Layout on the 240 x 240 round panel: an outer ring whose colour is the
// state at a glance (green: everything bonded is connected, blue: waiting,
// yellow: pairing, red: a question for the user), two device cards in the
// middle, one line of state below them, and the small numbers (USB, LAT,
// heap) at the bottom. The dial moves the highlight between the cards and
// the menu items; the button opens the menu, confirms, or answers an
// approval question.
//
// Drawing costs input latency (9c63777 report: redrawing everything once a
// second put 12 seconds over 3 ms into 5 minutes), so every widget is only
// touched when its value changed; LVGL then redraws just that area.

#include <stdio.h>
#include <string.h>
#include "driver/pulse_cnt.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "nvs.h"
#include "ble.h"
#include "display.h"
#include "ledger.h"
#include "log.h"
#include "ui.h"

#define PIN_ENC_A 40 // M5Dial rotary encoder (docs.m5stack.com/en/core/M5Dial)
#define PIN_ENC_B 41
#define ENC_STEPS_PER_DETENT 4 // quadrature edges per click (confirmed on the device, 9c63777 report)

#define W 240
#define H 240
#define DRAW_LINES 20 // draw buffer: W x DRAW_LINES x 2 bytes, twice (19 KB)

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

// Settings in NVS (namespace "orbit")
#define NS "orbit"
#define KEY_ROTATION "ui_rot"   // quarter turns, 0..3
#define KEY_SCREEN_OFF "ui_off" // index into screen_off_s[]
static const int screen_off_s[] = { 0, 30, 120, 600 }; // 0: never
static const char* screen_off_text[] = { "never", "30 s", "2 min", "10 min" };
#define SCREEN_OFF_CHOICES 4

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
static lv_obj_t* menu_item[8];
static lv_obj_t* notice; // full-screen message

static orbit_ui_state_t last; // latest snapshot (LVGL lock held while used)
static int selected = -1;     // card index, or -1 for none
static int menu_open;
static int menu_sel;
static int menu_count;
static int confirm_forget;    // port awaiting a second press, 0: none
static int64_t menu_opened_us;
static uint8_t rotation;      // quarter turns
static uint8_t screen_off;    // index into screen_off_s[]
static int64_t last_activity_us;
static bool screen_dark;

typedef enum { M_PAIR, M_STOP, M_APPROVE, M_FORGET, M_ROTATE, M_OFF, M_CLOSE } menu_kind_t;
static menu_kind_t menu_kind[8];
static int menu_port[8];

// What is on the screen now, so that unchanged values are not redrawn.
static struct {
    uint32_t ring_color;
    char title[48];
    char state[64];
    uint32_t state_color;
    char footer[64];
    char name[ORBIT_MAX_DEVS][32];
    char line[ORBIT_MAX_DEVS][48];
    uint32_t dot[ORBIT_MAX_DEVS];
    int selected;
} shown;

static double now_s(void) {
    return esp_timer_get_time() / 1e6;
}

static void set_text_if_changed(lv_obj_t* l, char* cache, int cache_len, const char* text) {
    if (strncmp(cache, text, cache_len) != 0) {
        snprintf(cache, cache_len, "%s", text);
        lv_label_set_text(l, text);
    }
}

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
    shown.ring_color = C_GREY;

    // Near the top the circle is only ~130 px wide: keep the title short and a little lower.
    title = make_label(scr, &lv_font_montserrat_14, C_DIM);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        card[i] = lv_obj_create(scr);
        lv_obj_set_size(card[i], 168, 46);
        lv_obj_align(card[i], LV_ALIGN_CENTER, 0, -40 + i * 52);
        lv_obj_set_style_radius(card[i], 10, 0);
        lv_obj_set_style_pad_all(card[i], 6, 0);
        lv_obj_set_scrollable(card[i], false);
        style_card(card[i], false);
        card_dot[i] = lv_obj_create(card[i]);
        lv_obj_set_size(card_dot[i], 10, 10);
        lv_obj_set_style_radius(card_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(card_dot[i], 0, 0);
        lv_obj_set_style_bg_color(card_dot[i], lv_color_hex(C_GREY), 0);
        shown.dot[i] = C_GREY;
        lv_obj_align(card_dot[i], LV_ALIGN_LEFT_MID, 0, 0);
        card_name[i] = make_label(card[i], &lv_font_montserrat_16, C_TEXT);
        lv_obj_set_width(card_name[i], 136);
        lv_label_set_long_mode(card_name[i], LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(card_name[i], LV_ALIGN_TOP_LEFT, 16, -2);
        card_line[i] = make_label(card[i], &lv_font_montserrat_12, C_DIM);
        lv_obj_align(card_line[i], LV_ALIGN_BOTTOM_LEFT, 16, 2);
    }
    shown.selected = -1;

    // Kept inside the circle: 184 px wide at 46 px below the centre, cut with dots.
    // One line only (fixed height, dots when too long), below the cards.
    state_line = make_label(scr, &lv_font_montserrat_14, C_TEXT);
    lv_obj_set_size(state_line, 190, 18);
    lv_obj_set_style_text_align(state_line, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(state_line, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(state_line, LV_ALIGN_CENTER, 0, 54);

    footer = make_label(scr, &lv_font_montserrat_12, C_DIM);
    lv_obj_set_size(footer, 150, 16);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(footer, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -26);

    menu = lv_obj_create(scr);
    // Round, so that nothing of it leaves the panel; the rest of the screen
    // hides while it is open (9c63777 report: the title peeked out above it).
    lv_obj_set_size(menu, 200, 200);
    lv_obj_center(menu);
    lv_obj_set_style_radius(menu, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(menu, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_border_color(menu, lv_color_hex(C_BLUE), 0);
    lv_obj_set_style_border_width(menu, 2, 0);
    lv_obj_set_style_pad_all(menu, 10, 0);
    lv_obj_set_scrollable(menu, false);
    for (int i = 0; i < 8; i++) {
        menu_item[i] = make_label(menu, &lv_font_montserrat_14, C_TEXT);
        lv_obj_set_size(menu_item[i], 150, 18);
        lv_obj_set_style_text_align(menu_item[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(menu_item[i], LV_LABEL_LONG_MODE_DOTS);
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

// ---- settings ----

static void settings_load(void) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, KEY_ROTATION, &rotation);
        nvs_get_u8(h, KEY_SCREEN_OFF, &screen_off);
        nvs_close(h);
    }
    rotation %= 4;
    if (screen_off >= SCREEN_OFF_CHOICES) {
        screen_off = 0;
    }
}

static void settings_save(void) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, KEY_ROTATION, rotation);
        nvs_set_u8(h, KEY_SCREEN_OFF, screen_off);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void apply_rotation(void) {
    // The port turns this into the panel's swap/mirror commands (no software rotation).
    lv_display_set_rotation(disp, (lv_display_rotation_t) rotation);
    lv_obj_invalidate(lv_screen_active());
}

static void screen_wake(void) {
    last_activity_us = esp_timer_get_time();
    if (screen_dark) {
        screen_dark = false;
        display_backlight(true);
    }
}

// ---- menu ----

static void render_menu(void) {
    menu_count = 0;
    if (last.approval.wanted) {
        menu_kind[menu_count++] = M_APPROVE;
    }
    menu_kind[menu_count++] = last.pairing ? M_STOP : M_PAIR;
    if (selected >= 0 && last.dev[selected].connected && last.dev[selected].port != 0) {
        menu_kind[menu_count] = M_FORGET;
        menu_port[menu_count] = last.dev[selected].port;
        menu_count++;
    }
    menu_kind[menu_count++] = M_ROTATE;
    menu_kind[menu_count++] = M_OFF;
    menu_kind[menu_count++] = M_CLOSE;
    if (menu_sel >= menu_count) {
        menu_sel = menu_count - 1;
    }
    for (int i = 0; i < 8; i++) {
        if (i >= menu_count) {
            lv_label_set_text(menu_item[i], "");
            continue;
        }
        lv_obj_align(menu_item[i], LV_ALIGN_CENTER, 0, (i - (menu_count - 1) / 2.0) * 24);
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
        case M_ROTATE:
            snprintf(text, sizeof(text), LV_SYMBOL_REFRESH "  Rotate: %d deg", rotation * 90);
            break;
        case M_OFF:
            snprintf(text, sizeof(text), LV_SYMBOL_POWER "  Screen off: %s", screen_off_text[screen_off]);
            break;
        default:
            snprintf(text, sizeof(text), "Close");
            break;
        }
        lv_label_set_text(menu_item[i], text);
        lv_obj_set_style_text_color(menu_item[i], lv_color_hex(i == menu_sel ? C_BLUE : C_TEXT), 0);
    }
}

static void show_main(bool on) {
    lv_obj_set_hidden(title, !on);
    lv_obj_set_hidden(state_line, !on);
    lv_obj_set_hidden(footer, !on);
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        lv_obj_set_hidden(card[i], !on);
    }
}

static void menu_close(void) {
    confirm_forget = 0;
    menu_open = 0;
    lv_obj_set_hidden(menu, true);
    show_main(true);
}

static void menu_act(void) {
    switch (menu_kind[menu_sel]) {
    case M_PAIR:
        olog("M1 EVT t=%.3f screen: pair new device\n", now_s());
        orbit_ble_pair_new_device();
        break;
    case M_STOP:
        olog("M1 EVT t=%.3f screen: stop pairing\n", now_s());
        orbit_ble_stop_pairing();
        break;
    case M_APPROVE:
        olog("M1 EVT t=%.3f screen: approve\n", now_s());
        orbit_ble_approve();
        break;
    case M_FORGET:
        if (confirm_forget != menu_port[menu_sel]) {
            confirm_forget = menu_port[menu_sel]; // ask once more
            render_menu();
            return;
        }
        olog("M1 EVT t=%.3f screen: forget port %d\n", now_s(), menu_port[menu_sel]);
        orbit_ble_forget(menu_port[menu_sel]);
        break;
    case M_ROTATE:
        rotation = (rotation + 1) % 4;
        settings_save();
        apply_rotation();
        olog("M1 EVT t=%.3f screen: rotation %d\n", now_s(), rotation * 90);
        menu_opened_us = esp_timer_get_time();
        render_menu();
        return; // stays open to see the result
    case M_OFF:
        screen_off = (screen_off + 1) % SCREEN_OFF_CHOICES;
        settings_save();
        olog("M1 EVT t=%.3f screen: off after %s\n", now_s(), screen_off_text[screen_off]);
        menu_opened_us = esp_timer_get_time();
        render_menu();
        return;
    default:
        break;
    }
    menu_close();
}

// Every 50 ms on the LVGL task: the dial, the button and the screen-off timer.
static void poll_input(lv_timer_t* t) {
    int64_t now = esp_timer_get_time();
    int count = 0;
    if (encoder != NULL) {
        pcnt_unit_get_count(encoder, &count);
    }
    int steps = (count - encoder_last) / ENC_STEPS_PER_DETENT;
    int pressed = press_pending;
    press_pending = 0;

    if (steps != 0 || pressed) {
        bool was_dark = screen_dark;
        screen_wake();
        if (was_dark) {
            // The first touch only wakes the screen.
            encoder_last = count;
            return;
        }
    }
    if (steps != 0) {
        encoder_last += steps * ENC_STEPS_PER_DETENT;
        if (menu_open) {
            menu_sel = ((menu_sel + steps) % menu_count + menu_count) % menu_count;
            confirm_forget = 0;
            menu_opened_us = now;
            render_menu();
        } else {
            int n = ORBIT_MAX_DEVS + 1; // the cards and "nothing"
            selected = ((selected + 1 + steps) % n + n) % n - 1;
        }
    }
    if (selected != shown.selected) {
        for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
            style_card(card[i], i == selected);
        }
        shown.selected = selected;
    }
    if (menu_open && now - menu_opened_us > 15 * 1000000) {
        menu_close(); // left open
    }
    if (pressed) {
        if (menu_open) {
            menu_act();
        } else if (last.approval.wanted) {
            // G-2: the short press answers the open question, as before the screen.
            olog("M1 EVT t=%.3f screen: approve (button)\n", now_s());
            orbit_ble_approve();
        } else {
            menu_open = 1;
            menu_sel = 0;
            confirm_forget = 0;
            menu_opened_us = now;
            render_menu();
            show_main(false);
            lv_obj_set_hidden(menu, false);
        }
    }
    if (!screen_dark && screen_off_s[screen_off] != 0 && !menu_open && !last.approval.wanted &&
        now - last_activity_us > (int64_t) screen_off_s[screen_off] * 1000000) {
        screen_dark = true;
        display_backlight(false);
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
    settings_load();
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;  // with the status task; far below the NimBLE host (CPU0)
    port_cfg.task_affinity = 0;  // CPU0, away from the main loop
    port_cfg.task_stack = 8192;
    port_cfg.timer_period_ms = 20;
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        return false;
    }
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = display_io(),
        .panel_handle = display_panel(),
        .control_handle = display_panel(),
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
        olog("M1 EVT t=%.3f screen: encoder init failed, dial disabled\n", now_s());
    }
    last_activity_us = esp_timer_get_time();
    if (lvgl_port_lock(1000)) {
        build();
        if (rotation != 0) {
            apply_rotation();
        }
        lv_timer_create(poll_input, 50, NULL);
        lvgl_port_unlock();
    }
    olog("M1 EVT t=%.3f screen: lvgl rotation=%d screen_off=%s\n", now_s(), rotation * 90,
         screen_off_text[screen_off]);
    return true;
}

static uint32_t ring_color(const orbit_ui_state_t* s, char* buf, int len) {
    if (s->approval.wanted) {
        snprintf(buf, len, LV_SYMBOL_WARNING " Port %d asks to pair: press (%ds)", s->approval.port,
                 s->approval.remaining_s);
        return C_RED;
    }
    if (s->full) {
        snprintf(buf, len, "All %d ports used: forget one", s->max_devices);
        return C_RED;
    }
    if (s->pairing) {
        if (s->pairing_remaining_s < 0) {
            snprintf(buf, len, "Pairing: turn on a device");
        } else {
            snprintf(buf, len, "Pairing %ds: turn on device", s->pairing_remaining_s);
        }
        return C_YELLOW;
    }
    if (s->approval.granted_port != 0) {
        snprintf(buf, len, "Port %d may pair again (%ds)", s->approval.granted_port, s->approval.granted_remaining_s);
        return C_YELLOW;
    }
    int connected = 0;
    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        connected += s->dev[i].connected;
    }
    if (s->duplicates) {
        snprintf(buf, len, "Two rows look alike: check them");
        return C_YELLOW;
    }
    if (connected == 0) {
        snprintf(buf, len, s->devices == 0 ? "No device paired yet" : "Waiting for %d device(s)", s->devices);
        return s->devices == 0 ? C_GREY : C_BLUE;
    }
    if (connected < s->devices && connected < ORBIT_MAX_DEVS) {
        snprintf(buf, len, "%d connected, waiting", connected);
        return C_BLUE;
    }
    snprintf(buf, len, connected == 1 ? "1 device connected" : "%d devices connected", connected);
    return C_GREEN;
}

void orbit_ui_update(const orbit_ui_state_t* s) {
    if (disp == NULL || !lvgl_port_lock(50)) {
        return; // never hold up the status task
    }
    // Something that needs the user wakes a dark screen.
    if (screen_dark && (s->approval.wanted || (s->pairing && !last.pairing) || s->full)) {
        screen_wake();
    }
    last = *s;

    char buf[64];
    uint32_t color = ring_color(s, buf, sizeof(buf));
    if (color != shown.ring_color) {
        lv_obj_set_style_arc_color(ring, lv_color_hex(color), LV_PART_INDICATOR);
        shown.ring_color = color;
    }
    uint32_t text_color = color == C_GREY ? C_DIM : color;
    if (text_color != shown.state_color) {
        lv_obj_set_style_text_color(state_line, lv_color_hex(text_color), 0);
        shown.state_color = text_color;
    }
    set_text_if_changed(state_line, shown.state, sizeof(shown.state), buf);

    char t[48];
    snprintf(t, sizeof(t), "ORBIT  %d/%d", s->devices, s->max_devices);
    set_text_if_changed(title, shown.title, sizeof(shown.title), t);

    for (int i = 0; i < ORBIT_MAX_DEVS; i++) {
        const orbit_ui_dev_t* d = &s->dev[i];
        uint32_t dot;
        char line[48];
        if (!d->connected) {
            set_text_if_changed(card_name[i], shown.name[i], sizeof(shown.name[i]), "--");
            line[0] = '\0';
            dot = C_GREY;
        } else {
            set_text_if_changed(card_name[i], shown.name[i], sizeof(shown.name[i]), d->name);
            // The report count changes every second while the device is in
            // use; it is the one value worth that redraw (a small area).
            snprintf(line, sizeof(line), "P%d  %.2f ms  L%u  %u/s", d->port, d->itvl_ms, d->latency, d->reports);
            dot = !d->encrypted ? C_YELLOW : d->subscribed == 0 ? C_YELLOW : d->reports > 0 ? C_GREEN : C_BLUE;
        }
        set_text_if_changed(card_line[i], shown.line[i], sizeof(shown.line[i]), line);
        if (dot != shown.dot[i]) {
            lv_obj_set_style_bg_color(card_dot[i], lv_color_hex(dot), 0);
            shown.dot[i] = dot;
        }
    }

    // Only what a user can act on: the USB state and the version. Latency,
    // heap and the last event stay in the log (015df03 report).
    char f[64];
    snprintf(f, sizeof(f), "USB %s  %s", s->usb, s->version);
    set_text_if_changed(footer, shown.footer, sizeof(shown.footer), f);

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
    screen_wake();
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
