#pragma once
#ifndef SCREEN_CONTROL_HPP
#define SCREEN_CONTROL_HPP

#include <stdarg.h>
#include <stdio.h>

#define DASHBOARD_SERIAL_BAUD 115200
#define DASH_FAST_UPDATE_INTERVAL_MS 50
#define DASH_SLOW_UPDATE_INTERVAL_MS 500
#define DASH_RESEND_INTERVAL_MS 1000

// Battery warning hysteresis band: on below 12.0 V, off above 12.4 V,
// so noise around a single threshold can't chatter the BATT light.
// Values provisional pending confirmation against the pack chemistry (issue #11).
#define BATT_WARN_ON_VOLTAGE 12.0
#define BATT_WARN_OFF_VOLTAGE 12.4

enum Warnings
{
    KILL,
    HEAT,
    STALL,
    BATT
};

bool warning_state[4] = {false, false, false, false};

static size_t appendToDashboardTx(char *tx, size_t pos, size_t txSize, const char *fmt, ...)
{
    if (pos >= txSize)
        return pos;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(tx + pos, txSize - pos, fmt, args);
    va_end(args);

    if (written <= 0)
        return pos;

    size_t available = txSize - pos;
    if ((size_t)written >= available)
        return txSize;

    return pos + (size_t)written;
}

static char warningCommand(int warning, bool state)
{
    switch (warning)
    {
    case KILL:
        return state ? 'K' : 'k';
    case HEAT:
        return state ? 'H' : 'h';
    case STALL:
        return state ? 'X' : 'x';
    case BATT:
        return state ? 'B' : 'b';
    default:
        return '\0';
    }
}

void warning_lights(int warning, bool state)
{
    char command = warningCommand(warning, state);
    if (command)
        Serial1.println(command);
}

static size_t appendGear(char *tx, size_t pos, size_t txSize, int gear)
{
    if (gear > 0 and gear <= 6)
    {
        return appendToDashboardTx(tx, pos, txSize, "G%d\n", gear);
    }
    else if (gear == 0)
    {
        return appendToDashboardTx(tx, pos, txSize, "GN\n");
    }

    return pos;
}

void changeGear(int gear)
{
    char tx[8];
    size_t pos = appendGear(tx, 0, sizeof(tx), gear);
    if (pos > 0 && pos < sizeof(tx))
        Serial1.write((const uint8_t *)tx, pos);
}

void updateDashboard(int gear, double speed, int rpm)
{
    static unsigned long lastFastUpdate = 0;
    static unsigned long lastSlowUpdate = 0;
    static unsigned long lastResend = 0;
    static int lastGear = -1000;
    static int lastRpm = -1;
    static int lastSpeedMph = -1000;
    static int lastVoltageTenths = -1;
    static bool lastWarningState[4] = {false, false, false, false};
    static bool warningStateInitialized = false;

    unsigned long now = millis();
    bool resend = now - lastResend >= DASH_RESEND_INTERVAL_MS;

    char tx[128];
    size_t pos = 0;

    if (now - lastFastUpdate >= DASH_FAST_UPDATE_INTERVAL_MS || resend)
    {
        lastFastUpdate = now;

        if (gear != lastGear || resend)
        {
            pos = appendGear(tx, pos, sizeof(tx), gear);
            lastGear = gear;
        }

        if (rpm != lastRpm || resend)
        {
            pos = appendToDashboardTx(tx, pos, sizeof(tx), "R%d\n", rpm);
            lastRpm = rpm;
        }

        int speedMph = speed > 0.0 ? (int)(speed + 0.5) : 0;
        if (speedMph != lastSpeedMph || resend)
        {
            pos = appendToDashboardTx(tx, pos, sizeof(tx), "S%d\n", speedMph);
            lastSpeedMph = speedMph;
        }
    }

    if (now - lastSlowUpdate >= DASH_SLOW_UPDATE_INTERVAL_MS || resend)
    {
        lastSlowUpdate = now;
        // Battery voltage and BATT light
        float voltage = getBatteryVoltage();
        if (voltage < BATT_WARN_ON_VOLTAGE)
        {
            warning_state[BATT] = true;
        }
        else if (voltage > BATT_WARN_OFF_VOLTAGE)
        {
            warning_state[BATT] = false;
        }

        int voltageTenths = voltage > 0.0f ? (int)(voltage * 10.0f + 0.5f) : 0;
        if (voltageTenths != lastVoltageTenths || resend)
        {
            pos = appendToDashboardTx(tx, pos, sizeof(tx), "V%d.%d\n", voltageTenths / 10, voltageTenths % 10);
            lastVoltageTenths = voltageTenths;
        }

        // Resend periodically so a dropped byte or display reboot can't
        // leave a light stale, but avoid repeating unchanged state every tick.
        for (int warning = KILL; warning <= BATT; warning++)
        {
            if (!warningStateInitialized || warning_state[warning] != lastWarningState[warning] || resend)
            {
                char command = warningCommand(warning, warning_state[warning]);
                if (command)
                    pos = appendToDashboardTx(tx, pos, sizeof(tx), "%c\n", command);
                lastWarningState[warning] = warning_state[warning];
            }
        }
        warningStateInitialized = true;
    }

    if (pos > 0 && pos < sizeof(tx))
        Serial1.write((const uint8_t *)tx, pos);

    if (resend)
        lastResend = now;
}

unsigned int laptimer = 0;

void incrementLaptimer()
{
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (now - lastTick >= 1000)
    {
        lastTick = now;

        laptimer++;

        int seconds = laptimer % 60;
        int minutes = (laptimer / 60) % 60;
        int hours = (laptimer / 3600) % 24;

        char laptimerString[9];
        snprintf(laptimerString, sizeof(laptimerString), "%02d:%02d:%02d", hours, minutes, seconds);

        Serial1.print('L');
        Serial1.println(laptimerString);
    }
}

#endif
