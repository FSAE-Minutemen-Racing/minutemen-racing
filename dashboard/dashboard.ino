#include <Arduino.h>
#include <esp_display_panel.hpp>

#include <lvgl.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl_v8_port.h"

#include "ui.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#define DASHBOARD_SERIAL_BAUD 115200

static const size_t DASH_RX_VALUE_MAX = 24;
static char dashRxValue[DASH_RX_VALUE_MAX];
static size_t dashRxValueLen = 0;
static char dashRxCommand = '\0';
static bool dashRxOverflow = false;

static bool killLightOn = false;
static bool stallLightOn = false;
static bool battLightOn = false;
static bool heatLightOn = false;

// Lit: bright pill with white text. Off: dark pill with dim text.
static void setWarningLight(lv_obj_t *light, bool on, uint32_t color)
{
    lv_obj_set_style_bg_color(light, lv_color_hex(on ? color : 0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(light, lv_color_hex(on ? 0xFFFFFF : 0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void setWarningLightIfChanged(lv_obj_t *light, bool *cached, bool on, uint32_t color)
{
    if (*cached == on) {
        return;
    }

    *cached = on;
    setWarningLight(light, on, color);
}

static void setLabelTextIfChanged(lv_obj_t *label, const char *value)
{
    const char *current = lv_label_get_text(label);
    if (current && strcmp(current, value) == 0) {
        return;
    }

    lv_label_set_text(label, value);
}

static void processDashCommand(char command, const char *value)
{
    lvgl_port_lock(-1);

    switch (command) {
        // Gear
        case 'G':
            setLabelTextIfChanged(ui_gear, value);
            break;

        // RPM drives the readout and the shift indicator
        case 'R':
            ui_set_rpm(atoi(value));
            break;

        case 'S':
            setLabelTextIfChanged(ui_speed, value);
            break;

        case 'C':
            setLabelTextIfChanged(ui_coolant, value);
            break;

        case 'V':
            setLabelTextIfChanged(ui_volts, value);
            break;

        // Warning Lights (off/on)
        case 'k':
            setWarningLightIfChanged(ui_kill, &killLightOn, false, 0xFF0000);
            break;
        case 'K':
            setWarningLightIfChanged(ui_kill, &killLightOn, true, 0xFF0000);
            break;

        case 'b':
            setWarningLightIfChanged(ui_batt, &battLightOn, false, 0xFF8A00);
            break;
        case 'B':
            setWarningLightIfChanged(ui_batt, &battLightOn, true, 0xFF8A00);
            break;

        case 'x':
            setWarningLightIfChanged(ui_stall, &stallLightOn, false, 0xFF0000);
            break;
        case 'X':
            setWarningLightIfChanged(ui_stall, &stallLightOn, true, 0xFF0000);
            break;

        case 'h':
            setWarningLightIfChanged(ui_heat, &heatLightOn, false, 0xFF8A00);
            break;
        case 'H':
            setWarningLightIfChanged(ui_heat, &heatLightOn, true, 0xFF8A00);
            break;

        default:
            break;
    }

    lvgl_port_unlock();
}

static void pollDashboardSerial()
{
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (dashRxCommand == '\0') {
            if (c != '\n' && c != '\r') {
                dashRxCommand = c;
                dashRxValueLen = 0;
                dashRxOverflow = false;
            }
            continue;
        }

        if (c == '\n') {
            if (!dashRxOverflow) {
                dashRxValue[dashRxValueLen] = '\0';
                processDashCommand(dashRxCommand, dashRxValue);
            }

            dashRxCommand = '\0';
            dashRxValueLen = 0;
            dashRxOverflow = false;
            continue;
        }

        if (c == '\r') {
            continue;
        }

        if (dashRxValueLen + 1 < DASH_RX_VALUE_MAX) {
            dashRxValue[dashRxValueLen++] = c;
        } else {
            dashRxOverflow = true;
        }
    }
}

void setup()
{
    Serial.begin(DASHBOARD_SERIAL_BAUD);

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
    pollDashboardSerial();
}
