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

// Coolant temperature warning bands. HEAT is blue while the engine is too
// cold to load hard, off in the normal range, and red when overheated.
#define COOLANT_COLD_WARN_ON_F 140
#define COOLANT_COLD_WARN_OFF_F 160
#define COOLANT_HOT_WARN_ON_F 220
#define COOLANT_HOT_WARN_OFF_F 210

// Stall warning arms only after the engine has plausibly run once, then turns
// on when RPM collapses while a gear is selected and the kill switch is not on.
#define STALL_ARM_RPM 1200
#define STALL_WARN_RPM 500

enum Warnings
{
    KILL,
    HEAT,
    STALL,
    BATT
};

bool warning_state[4] = {false, false, false, false};

enum HeatWarningState
{
    HEAT_NORMAL,
    HEAT_COLD,
    HEAT_HOT
};

HeatWarningState heat_warning_state = HEAT_NORMAL;

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

static char heatWarningCommand(HeatWarningState state)
{
    switch (state)
    {
    case HEAT_COLD:
        return 'L';
    case HEAT_HOT:
        return 'H';
    case HEAT_NORMAL:
    default:
        return 'h';
    }
}

void warning_lights(int warning, bool state)
{
    char command = warningCommand(warning, state);
    if (command)
        Serial1.println(command);
}

void heat_warning_light(HeatWarningState state)
{
    Serial1.println(heatWarningCommand(state));
}

static HeatWarningState nextHeatWarningState(HeatWarningState current, int coolantTempF)
{
    switch (current)
    {
    case HEAT_COLD:
        if (coolantTempF < COOLANT_COLD_WARN_OFF_F)
            return HEAT_COLD;
        break;
    case HEAT_HOT:
        if (coolantTempF > COOLANT_HOT_WARN_OFF_F)
            return HEAT_HOT;
        break;
    case HEAT_NORMAL:
    default:
        break;
    }

    if (coolantTempF < COOLANT_COLD_WARN_ON_F)
        return HEAT_COLD;
    if (coolantTempF > COOLANT_HOT_WARN_ON_F)
        return HEAT_HOT;

    return HEAT_NORMAL;
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
    static int lastCoolantTempF = -1000;
    static int lastVoltageTenths = -1;
    static bool lastWarningState[4] = {false, false, false, false};
    static HeatWarningState lastHeatWarningState = HEAT_NORMAL;
    static bool warningStateInitialized = false;
    static bool engineHasRun = false;

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

        const bool killSwitchActive = isKillSwitchActive();
        warning_state[KILL] = killSwitchActive;

        if (rpm >= STALL_ARM_RPM)
            engineHasRun = true;
        warning_state[STALL] = engineHasRun && !killSwitchActive && gear > 0 && rpm < STALL_WARN_RPM;

        int coolantTempF = readSensors(COOLANT_TEMP_F);
        heat_warning_state = nextHeatWarningState(heat_warning_state, coolantTempF);
        if (coolantTempF != lastCoolantTempF || resend)
        {
            pos = appendToDashboardTx(tx, pos, sizeof(tx), "C%d\n", coolantTempF);
            lastCoolantTempF = coolantTempF;
        }

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
            if (warning == HEAT)
                continue;

            if (!warningStateInitialized || warning_state[warning] != lastWarningState[warning] || resend)
            {
                char command = warningCommand(warning, warning_state[warning]);
                if (command)
                    pos = appendToDashboardTx(tx, pos, sizeof(tx), "%c\n", command);
                lastWarningState[warning] = warning_state[warning];
            }
        }
        warningStateInitialized = true;

        if (heat_warning_state != lastHeatWarningState || resend)
        {
            pos = appendToDashboardTx(tx, pos, sizeof(tx), "%c\n",
                                      heatWarningCommand(heat_warning_state));
            lastHeatWarningState = heat_warning_state;
        }
    }

    if (pos > 0 && pos < sizeof(tx))
        Serial1.write((const uint8_t *)tx, pos);

    if (resend)
        lastResend = now;
}

#endif
