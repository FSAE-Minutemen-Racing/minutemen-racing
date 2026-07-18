// Hand-written driver screen (replaces the SquareLine Studio export).
// LVGL version: 8.3.11
//
// 800x480 layout:
//   - Top strip: shift indicator. Two bars fill from the outer edges and
//     meet in the middle exactly at DASH_RPM_SHIFT; past that the whole
//     strip flashes red with a SHIFT banner.
//   - Middle row: speed (left), gear (center, large), rpm (right).
//   - Bottom row: coolant (left), warning lights (center), battery (right).

#include "ui.h"

// Shift thresholds are placeholders until calibrated on the dyno (the bars
// reach the middle at DASH_RPM_SHIFT, so only these two need tuning).
#define DASH_RPM_SHIFT  12000   // bars meet + flash: time to shift
#define DASH_RPM_YELLOW 10000   // bar turns yellow: shift point approaching

#define SHIFT_STRIP_HEIGHT 64
#define SHIFT_FLASH_PERIOD_MS 90

lv_obj_t * ui_Driver_Mode = NULL;
lv_obj_t * ui_shiftbar_left = NULL;
lv_obj_t * ui_shiftbar_right = NULL;
lv_obj_t * ui_shiftmark = NULL;
lv_obj_t * ui_shiftflash = NULL;
lv_obj_t * ui_gear = NULL;
lv_obj_t * ui_speed = NULL;
lv_obj_t * ui_rpm = NULL;
lv_obj_t * ui_coolant = NULL;
lv_obj_t * ui_volts = NULL;
lv_obj_t * ui_kill = NULL;
lv_obj_t * ui_stall = NULL;
lv_obj_t * ui_batt = NULL;
lv_obj_t * ui_heat = NULL;

static lv_timer_t * shift_flash_timer = NULL;
static bool shift_active = false;

static void shift_flash_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if (shift_active) {
        if (lv_obj_has_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (!lv_obj_has_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t * make_shift_bar(lv_align_t align, lv_base_dir_t dir)
{
    lv_obj_t * bar = lv_bar_create(ui_Driver_Mode);
    lv_bar_set_range(bar, 0, DASH_RPM_SHIFT);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_size(bar, 397, SHIFT_STRIP_HEIGHT);
    lv_obj_set_align(bar, align);
    lv_obj_set_style_base_dir(bar, dir, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A1A1A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x21C400), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    return bar;
}

static lv_obj_t * make_value(lv_coord_t x, lv_coord_t y, const char * text)
{
    lv_obj_t * label = lv_label_create(ui_Driver_Mode);
    lv_obj_set_width(label, 180);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

static lv_obj_t * make_caption(lv_coord_t x, lv_coord_t y, const char * text)
{
    lv_obj_t * label = lv_label_create(ui_Driver_Mode);
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

static lv_obj_t * make_light(lv_coord_t x, lv_coord_t y, const char * text)
{
    lv_obj_t * label = lv_label_create(ui_Driver_Mode);
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_hor(label, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

void ui_Driver_Mode_screen_init(void)
{
    ui_Driver_Mode = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Driver_Mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Driver_Mode, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Driver_Mode, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Shift indicator strip: left bar fills rightward, right bar fills
    // leftward, converging on the center mark at the shift point.
    ui_shiftbar_left = make_shift_bar(LV_ALIGN_TOP_LEFT, LV_BASE_DIR_LTR);
    ui_shiftbar_right = make_shift_bar(LV_ALIGN_TOP_RIGHT, LV_BASE_DIR_RTL);

    ui_shiftmark = lv_obj_create(ui_Driver_Mode);
    lv_obj_remove_style_all(ui_shiftmark);
    lv_obj_set_size(ui_shiftmark, 6, SHIFT_STRIP_HEIGHT);
    lv_obj_set_align(ui_shiftmark, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_color(ui_shiftmark, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_shiftmark, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_shiftflash = lv_obj_create(ui_Driver_Mode);
    lv_obj_remove_style_all(ui_shiftflash);
    lv_obj_set_size(ui_shiftflash, 800, SHIFT_STRIP_HEIGHT);
    lv_obj_set_align(ui_shiftflash, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_color(ui_shiftflash, lv_color_hex(0xFF2000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_shiftflash, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * shift_text = lv_label_create(ui_shiftflash);
    lv_obj_set_align(shift_text, LV_ALIGN_CENTER);
    lv_label_set_text(shift_text, "SHIFT");
    lv_obj_set_style_text_color(shift_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(shift_text, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Center: gear
    ui_gear = lv_label_create(ui_Driver_Mode);
    lv_obj_set_size(ui_gear, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(ui_gear, 0, -10);
    lv_obj_set_align(ui_gear, LV_ALIGN_CENTER);
    lv_label_set_text(ui_gear, "N");
    lv_obj_set_style_text_color(ui_gear, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_gear, &ui_font_GEAR_FONT, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Middle row: speed left, rpm right
    ui_speed = make_value(-280, -60, "0");
    make_caption(-280, 0, "MPH");

    ui_rpm = make_value(280, -60, "0");
    make_caption(280, 0, "RPM");

    // Bottom row: coolant left, battery right
    ui_coolant = make_value(-280, 130, "-");
    make_caption(-280, 190, "COOLANT");

    ui_volts = make_value(280, 130, "-");
    make_caption(280, 190, "VOLTS");

    // Warning lights, dark until lit by the serial handler
    ui_kill = make_light(-85, 130, "KILL");
    ui_stall = make_light(-85, 190, "STALL");
    ui_batt = make_light(85, 130, "BATT");
    ui_heat = make_light(85, 190, "HEAT");

    shift_active = false;
    shift_flash_timer = lv_timer_create(shift_flash_cb, SHIFT_FLASH_PERIOD_MS, NULL);
}

void ui_set_rpm(int rpm)
{
    if (rpm < 0) rpm = 0;
    lv_label_set_text_fmt(ui_rpm, "%d", rpm);

    int fill = rpm > DASH_RPM_SHIFT ? DASH_RPM_SHIFT : rpm;
    lv_bar_set_value(ui_shiftbar_left, fill, LV_ANIM_OFF);
    lv_bar_set_value(ui_shiftbar_right, fill, LV_ANIM_OFF);

    lv_color_t zone = lv_color_hex(0x21C400);
    if (rpm >= DASH_RPM_SHIFT) {
        zone = lv_color_hex(0xFF2000);
    } else if (rpm >= DASH_RPM_YELLOW) {
        zone = lv_color_hex(0xFFC400);
    }
    lv_obj_set_style_bg_color(ui_shiftbar_left, zone, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_shiftbar_right, zone, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    shift_active = rpm >= DASH_RPM_SHIFT;
    if (!shift_active) {
        lv_obj_add_flag(ui_shiftflash, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_Driver_Mode_screen_destroy(void)
{
    if (shift_flash_timer) {
        lv_timer_del(shift_flash_timer);
        shift_flash_timer = NULL;
    }
    shift_active = false;

    if (ui_Driver_Mode) lv_obj_del(ui_Driver_Mode);

    ui_Driver_Mode = NULL;
    ui_shiftbar_left = NULL;
    ui_shiftbar_right = NULL;
    ui_shiftmark = NULL;
    ui_shiftflash = NULL;
    ui_gear = NULL;
    ui_speed = NULL;
    ui_rpm = NULL;
    ui_coolant = NULL;
    ui_volts = NULL;
    ui_kill = NULL;
    ui_stall = NULL;
    ui_batt = NULL;
    ui_heat = NULL;
}
