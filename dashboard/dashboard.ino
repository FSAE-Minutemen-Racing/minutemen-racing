#include <Arduino.h>
#include <esp_display_panel.hpp>

#include <lvgl.h>
#include "lvgl_v8_port.h"

#include "ui.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

void setup()
{
    Serial.begin(9600);

    Serial.println("Initializing board");
    Board *board = new Board();
    board->init();

    #if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    // When avoid tearing function is enabled, the frame buffer number should be set in the board driver
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    /**
     * As the anti-tearing feature typically consumes more PSRAM bandwidth, for the ESP32-S3, we need to utilize the
     * "bounce buffer" functionality to enhance the RGB data bandwidth.
     * This feature will consume `bounce_buffer_size * bytes_per_pixel * 2` of SRAM memory.
     */
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
    assert(board->begin());

    Serial.println("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    Serial.println("Creating UI");
    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    lvgl_port_lock(-1);

    ui_init();

    /* Release the mutex */
    lvgl_port_unlock();
}

void loop()
{
    if (Serial.available()) {

        char command = Serial.read();
        String value = Serial.readStringUntil('\n');

        switch (command) {
            // Screen changes
            case '1':
                _ui_screen_change(&ui_Start_Mode, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_Start_Mode_screen_init);
                break;
            case '2':
                _ui_screen_change(&ui_Driver_Mode, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_Driver_Mode_screen_init);
                break;
            case '3':
                _ui_screen_change(&ui_Engineer_Mode, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, &ui_Engineer_Mode_screen_init);
                break;

            // Gauges
            case 'G':
                lv_label_set_text_fmt(ui_gear, value.c_str());
                break;
            case 'R': {
                lv_label_set_text_fmt(ui_rpm, value.c_str());
                lv_slider_set_value(ui_rpmbar, (value.toInt() * 100) / 16000, LV_ANIM_ON);
                lv_event_send(ui_rpmbar, LV_EVENT_VALUE_CHANGED, NULL);
                break;
            }
            case 'S':
                lv_label_set_text_fmt(ui_speed, value.c_str());
                break;
            case 'C':
                lv_label_set_text_fmt(ui_coolant, value.c_str());
                break;
            case 'L':
                lv_label_set_text(ui_laptimer, value.c_str());
                break;

            // Warning Lights
            case 'k':
                lv_obj_set_style_bg_color(ui_kill, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'K':
                lv_obj_set_style_bg_color(ui_kill, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'b':
                lv_obj_set_style_bg_color(ui_batt, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'B':
                lv_obj_set_style_bg_color(ui_batt, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'x':
                lv_obj_set_style_bg_color(ui_stall, lv_color_hex(0x00000), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'X':
                lv_obj_set_style_bg_color(ui_stall, lv_color_hex(0xFF8A00), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'h':
                lv_obj_set_style_bg_color(ui_heat, lv_color_hex(0x00000), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case 'H':
                lv_obj_set_style_bg_color(ui_heat, lv_color_hex(0xFF8A00), LV_PART_MAIN | LV_STATE_DEFAULT);
                break;

            // Sensors
            case 'V':
                lv_label_set_text_fmt(ui_volts, value.c_str());
                break;
            case 'F':
                lv_label_set_text_fmt(ui_gforce, value.c_str());
                break;
            case 'g':
                lv_label_set_text_fmt(ui_gps, value.c_str());
                break;
            case 'T': {
                lv_label_set_text(ui_tps, value.c_str());
                lv_slider_set_value(ui_battbar, (int)((value.toFloat() - 0) * 100) / (100 - 0), LV_ANIM_ON);
                lv_event_send(ui_battbar, LV_EVENT_VALUE_CHANGED, NULL);
                break;
            }

            default:
                // Handle unknown command if needed.  Could be a log message or error handling.
                break;
            }
    }
}