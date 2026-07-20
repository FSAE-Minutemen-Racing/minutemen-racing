// Hand-written driver screen (replaces the SquareLine Studio export).
// LVGL version: 8.3.11

#ifndef UI_DRIVER_MODE_H
#define UI_DRIVER_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_Driver_Mode
extern void ui_Driver_Mode_screen_init(void);
extern void ui_Driver_Mode_screen_destroy(void);
extern lv_obj_t * ui_Driver_Mode;

// Shift indicator (top strip)
extern lv_obj_t * ui_shiftbar_left;
extern lv_obj_t * ui_shiftbar_right;
extern lv_obj_t * ui_shiftmark;
extern lv_obj_t * ui_shiftflash;

// Primary values
extern lv_obj_t * ui_gear;
extern lv_obj_t * ui_speed;
extern lv_obj_t * ui_rpm;
extern lv_obj_t * ui_coolant;
extern lv_obj_t * ui_volts;

// Warning lights
extern lv_obj_t * ui_kill;
extern lv_obj_t * ui_stall;
extern lv_obj_t * ui_batt;
extern lv_obj_t * ui_heat;

// Updates the gear readout and selects the matching shift-light window.
// Gear 0 is neutral; neutral and 6th gear disable the shift indicator.
void ui_set_gear(int gear);

// Updates the RPM readout and the shift indicator (bar fill, zone color,
// and the flashing bar once past the current gear's shift point).
void ui_set_rpm(int rpm);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
