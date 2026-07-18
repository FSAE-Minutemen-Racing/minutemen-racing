#include <Arduino.h>
#include <esp_display_panel.hpp>

#include <lvgl.h>
#include "lvgl_v8_port.h"

#include "ui.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// Lit: bright pill with white text. Off: dark pill with dim text.
static void setWarningLight(lv_obj_t *light, bool on, uint32_t color)
{
    lv_obj_set_style_bg_color(light, lv_color_hex(on ? color : 0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(light, lv_color_hex(on ? 0xFFFFFF : 0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
}

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
        value.trim();

        lvgl_port_lock(-1);

        switch (command) {
            // Gear
            case 'G':
                lv_label_set_text(ui_gear, value.c_str());
                break;

            // RPM drives the readout and the shift indicator
            case 'R':
                ui_set_rpm(value.toInt());
                break;

            case 'S':
                lv_label_set_text(ui_speed, value.c_str());
                break;

            case 'C':
                lv_label_set_text(ui_coolant, value.c_str());
                break;

            case 'V':
                lv_label_set_text(ui_volts, value.c_str());
                break;

            // Warning Lights (off/on)
            case 'k':
                setWarningLight(ui_kill, false, 0xFF0000);
                break;
            case 'K':
                setWarningLight(ui_kill, true, 0xFF0000);
                break;

            case 'b':
                setWarningLight(ui_batt, false, 0xFF8A00);
                break;
            case 'B':
                setWarningLight(ui_batt, true, 0xFF8A00);
                break;

            case 'x':
                setWarningLight(ui_stall, false, 0xFF0000);
                break;
            case 'X':
                setWarningLight(ui_stall, true, 0xFF0000);
                break;

            case 'h':
                setWarningLight(ui_heat, false, 0xFF8A00);
                break;
            case 'H':
                setWarningLight(ui_heat, true, 0xFF8A00);
                break;

            default:
                break;
        }

        lvgl_port_unlock();
    }
}