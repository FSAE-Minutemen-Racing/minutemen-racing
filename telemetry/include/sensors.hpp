#pragma once

// 2004 Yamaha YZF-R6 drivetrain, spec-sheet values (dimensionless reductions)
#define PRIMARY_RATIO 1.955 // 86/44, crank -> clutch
#define RATIO_1 2.846       // 37/13
#define RATIO_2 1.947       // 37/19
#define RATIO_3 1.556       // 28/18
#define RATIO_4 1.333       // 32/24
#define RATIO_5 1.190       // 25/21
#define RATIO_6 1.083       // 26/24

// MR19 drivetrain, measured on-car 2026-07-18.
#define FINAL_DRIVE_RATIO 3.48     // measured MR19 sprockets
#define TIRE_CIRCUMFERENCE_IN 64.4 // measured 20.5 in OD tire × pi

// Spec gearbox ratios 1st–6th, indexed by gear - 1.
static const double GEARBOX_RATIOS[6] = {RATIO_1, RATIO_2, RATIO_3,
                                         RATIO_4, RATIO_5, RATIO_6};

// Engine RPM per mph of ground speed in a given gear (1 mph = 1056 in/min).
#define RPM_PER_MPH(gearboxRatio) \
    (PRIMARY_RATIO * (gearboxRatio) * FINAL_DRIVE_RATIO * 1056.0 / TIRE_CIRCUMFERENCE_IN)

const byte rpm_pulse_pin = 2;
const unsigned int gear_up_pin = 19;
const unsigned int gear_down_pin = 8;
const unsigned int neutral_pin = 4;

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;

void pulseISR()
{
    unsigned long now = micros();
    unsigned long interval = now - lastPulseTime;

    if (interval > 920)
    {
        pulseInterval = interval;
        lastPulseTime = now;
    }
}

void initSensors()
{
    pinMode(rpm_pulse_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(rpm_pulse_pin), pulseISR, RISING);

    pinMode(gear_up_pin, INPUT);
    pinMode(gear_down_pin, INPUT);
    pinMode(neutral_pin, INPUT_PULLUP);
}

int calculateRPM()
{
    if (micros() - lastPulseTime > 500000UL)
        return 0;

    // No pulse measured yet (e.g. first ~500 ms after boot); avoid divide-by-zero.
    if (pulseInterval == 0)
        return 0;

    return (60000000UL / pulseInterval) / 4;
}

static const unsigned long SHIFT_TIMEOUT_MS = 300UL;

// Dead-reckoned gear state. File-scope so crossCheckGear() can resync it
// when the drivetrain ratio proves it wrong (issue #8).
static int currentGear = 1;
static bool inNeutral = false;
static bool shifting = false;
static bool waitingForRelease = false;
static unsigned long shiftStartMs = 0;

int senseGear()
{
    const unsigned long now = millis();
    const int neutralLevel = digitalRead(neutral_pin);
    const int gearUpLevel = digitalRead(gear_up_pin);
    const int gearDownLevel = digitalRead(gear_down_pin);

    // ── Neutral sensor ────────────────────────────────────────────────────
    // Neutral sits between 1st and 2nd (1-N-2-3-4-5-6 sequential box, 2004
    // Yamaha R6), so it is tracked as its own state: down goes to 1st, up
    // goes to 2nd.
    if (!shifting && neutralLevel == LOW)
    {
        inNeutral = true;
        return 0;
    }

    // ── Both paddles LOW → invalid / ignition off ─────────────────────────
    if (gearUpLevel == LOW && gearDownLevel == LOW)
    {
        shifting = false;
        waitingForRelease = false;
        return inNeutral ? 0 : currentGear;
    }

    // ── Both paddles released → stroke complete, timeout latch cleared ────
    if (gearUpLevel == HIGH && gearDownLevel == HIGH)
    {
        shifting = false;
        waitingForRelease = false;
    }

    // ── Timeout guard: latch until both paddles release before re-arming ──
    if (shifting && (now - shiftStartMs >= SHIFT_TIMEOUT_MS))
    {
        shifting = false;
        waitingForRelease = true;
    }

    // ── Shift strokes (exactly one paddle LOW past the early returns) ─────
    if (!shifting && !waitingForRelease)
    {
        const bool shiftUp = gearUpLevel == LOW && (inNeutral || currentGear < 6);
        const bool shiftDown = gearDownLevel == LOW && (inNeutral || currentGear > 1);

        if (shiftUp || shiftDown)
        {
            shifting = true;
            shiftStartMs = now;
            if (inNeutral)
            {
                // Neutral sits between 1st and 2nd: up lands in 2nd, down in 1st.
                currentGear = shiftUp ? 2 : 1;
                inNeutral = false;
            }
            else
            {
                currentGear += shiftUp ? 1 : -1;
            }
            return currentGear;
        }
    }

    return inNeutral ? 0 : currentGear;
}

// ── Gear plausibility cross-check (issue #8) ──────────────────────────────
// senseGear() dead-reckons from paddle presses, so a missed or failed shift
// desyncs it until the car returns to neutral. While the engine is coupled
// to the wheels, the ratio of RPM to true (GPS) ground speed identifies the
// gear, so persistent disagreement means the tracked gear is wrong and we
// resync to the measured one.

#define GEAR_CHECK_MIN_SPEED_MPH 10.0 // below this, launch clutch slip and GPS noise dominate
#define GEAR_CHECK_TOLERANCE 0.06     // ±6% outer bound. 5th/6th differ by only
                                      // 9.9%, so their bands overlap slightly;
                                      // nearest match decides. Don't widen this.
#define GEAR_CHECK_AGREE_FIXES 3      // consecutive GPS fixes (~1/s) before resync
#define GEAR_CHECK_MAX_DELTA_MPH 3.0  // speed change per fix above which GPS lag
                                      // skews the ratio toward the wrong gear

// Gear implied by the observed RPM/speed ratio, or 0 if no gear is within
// tolerance (clutch in, mid-shift, or coasting in neutral).
int gearFromRatio(int rpm, double speedMph)
{
    const double observed = rpm / speedMph;
    int best = 0;
    double bestError = GEAR_CHECK_TOLERANCE;

    for (int g = 0; g < 6; g++)
    {
        double error = fabs(observed / RPM_PER_MPH(GEARBOX_RATIOS[g]) - 1.0);
        if (error < bestError)
        {
            bestError = error;
            best = g + 1;
        }
    }
    return best;
}

// Call once per fresh GPS speed fix. Resyncs the dead-reckoned gear when the
// drivetrain ratio names the same different gear GEAR_CHECK_AGREE_FIXES
// fixes in a row.
void crossCheckGear(int rpm, double speedMph)
{
    static int candidate = 0;
    static int agreeCount = 0;
    static double lastSpeedMph = -1.0;

    const double deltaMph =
        (lastSpeedMph < 0) ? 2 * GEAR_CHECK_MAX_DELTA_MPH : fabs(speedMph - lastSpeedMph);
    lastSpeedMph = speedMph;

    // The neutral switch is ground truth; mid-shift or low-speed ratios don't
    // identify a gear. Under hard acceleration or braking the GPS speed lags
    // true speed, which biases the observed ratio toward an adjacent gear, so
    // those samples are skipped too.
    if (shifting || digitalRead(neutral_pin) == LOW ||
        rpm <= 0 || speedMph < GEAR_CHECK_MIN_SPEED_MPH ||
        deltaMph > GEAR_CHECK_MAX_DELTA_MPH)
    {
        candidate = 0;
        agreeCount = 0;
        return;
    }

    const int measured = gearFromRatio(rpm, speedMph);

    // No confident match, or it agrees with the tracked gear (a stale
    // tracked neutral is still a disagreement — the car is moving in gear).
    if (measured == 0 || (measured == currentGear && !inNeutral))
    {
        candidate = 0;
        agreeCount = 0;
        return;
    }

    if (measured == candidate)
    {
        agreeCount++;
    }
    else
    {
        candidate = measured;
        agreeCount = 1;
    }

    if (agreeCount >= GEAR_CHECK_AGREE_FIXES)
    {
        currentGear = candidate;
        inNeutral = false;
        candidate = 0;
        agreeCount = 0;
    }
}

double getSpeed(int gear, int rpm)
{
    if (gear == 0)
        return 0.0;

    if (gear < 1 || gear > 6)
        return -0.5;

    return rpm / RPM_PER_MPH(GEARBOX_RATIOS[gear - 1]);
}

float getBatteryVoltage()
{
    // Throwaway read lets the ADC input settle after other channels.
    analogRead(A4);

    long sum = 0;
    for (int i = 0; i < 5; i++)
        sum += analogRead(A4);

    return (sum / 1023.0) * 3.43;
}
