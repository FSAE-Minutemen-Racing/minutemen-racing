#pragma once

#include <SoftwareSerial.h>

// Serial allocation (see #18): the dashboard owns Serial1, USB owns Serial,
// and the WiFi co-processor owns Serial2, so the K-line transceiver gets a
// software UART on otherwise-unused pins.
// RX must be an ICU-IRQ-capable pin or SoftwareSerial::begin() fails and
// never receives; D6 (P111, IRQ4) qualifies, D5 (P107) does not.
#define K_LINE_RX_PIN 6
#define K_LINE_TX_PIN 5

// Standard diagnostic K-line rate -- confirm on the bench (#18).
#define K_LINE_BAUD 10400

// Protocol timing (ms)
#define K_LINE_INIT_DELAY 611       // ECU power-on wait before init
#define K_LINE_POST_INIT_DELAY 16   // settle time after successful init
#define K_LINE_RESPONSE_TIMEOUT 50  // deadline for a full response frame
#define K_LINE_POLL_INTERVAL 100    // gap between data requests
#define K_LINE_RETRY_DELAY 1000     // wait before re-running init after a failure

// Consecutive missed/invalid data responses before re-initializing
#define K_LINE_MAX_MISSED_RESPONSES 5

// Command bytes (each response frame echoes the command it answers)
#define K_LINE_CMD_INIT 0xFE
#define K_LINE_CMD_DATA 0x01

SoftwareSerial klineSerial(K_LINE_RX_PIN, K_LINE_TX_PIN);
#define K_LINE_SERIAL klineSerial

// Latest values decoded from the ECU
int klineSpeed = 0;
int klineCoolant = 0;

// --- State Machine ---
enum class KLineState
{
    INIT_WAIT,       // waiting out the ECU power-on delay
    INIT_CMD_SENT,   // init sent, waiting for the echo/response frame
    INIT_POST_DELAY, // settling after a successful init
    READY,           // initialized, idle between polls
    REQ_CMD_SENT,    // data request sent, waiting for the response frame
    RETRY_WAIT       // init failed, waiting before trying again
};

static KLineState kState = KLineState::INIT_WAIT;
static unsigned long kTimer = 0;

// One transaction frame: the echo of our command byte plus five payload bytes
static byte kResponse[6];
static byte kMissedResponses = 0;
static bool kInitialized = false;
static bool kPortFailed = false;

bool kLineReady()
{
    return kInitialized;
}

void initKLine()
{
    // begin() fails if the RX pin has no ICU interrupt channel or the
    // timers/DMA it needs are taken; don't run the state machine against
    // a port that can never receive.
    kPortFailed = K_LINE_SERIAL.begin(K_LINE_BAUD) == 0;
    kTimer = millis();
    kState = KLineState::INIT_WAIT;
}

static void sendKLineCommand(byte command)
{
    // Drop any stale bytes so the next read only sees this transaction
    while (K_LINE_SERIAL.available() > 0)
        K_LINE_SERIAL.read();

    K_LINE_SERIAL.write(command);
    kTimer = millis();
}

// 1 = full frame read into kResponse, -1 = timed out, 0 = still waiting
static int readKLineResponse(unsigned long now)
{
    if (K_LINE_SERIAL.available() >= (int)sizeof(kResponse))
    {
        K_LINE_SERIAL.readBytes(kResponse, sizeof(kResponse));
        return 1;
    }

    if (now - kTimer >= K_LINE_RESPONSE_TIMEOUT)
        return -1;

    return 0;
}

// Non-blocking; call every loop(). Handles init, polling, and re-init
// after repeated failures.
void updateKLine()
{
    if (kPortFailed)
        return;

    unsigned long now = millis();

    switch (kState)
    {
    case KLineState::INIT_WAIT:
        if (now - kTimer >= K_LINE_INIT_DELAY)
        {
            sendKLineCommand(K_LINE_CMD_INIT);
            kState = KLineState::INIT_CMD_SENT;
        }
        break;

    case KLineState::INIT_CMD_SENT:
        switch (readKLineResponse(now))
        {
        case 1:
            if (kResponse[0] == K_LINE_CMD_INIT &&
                kResponse[1] == 0x00 && kResponse[2] == 0x00 &&
                kResponse[3] == 0x00 && kResponse[4] == 0x00 &&
                kResponse[5] == 0x00)
            {
                kTimer = now;
                kState = KLineState::INIT_POST_DELAY;
            }
            else
            {
                kTimer = now;
                kState = KLineState::RETRY_WAIT;
            }
            break;

        case -1:
            kTimer = now;
            kState = KLineState::RETRY_WAIT;
            break;
        }
        break;

    case KLineState::INIT_POST_DELAY:
        if (now - kTimer >= K_LINE_POST_INIT_DELAY)
        {
            kInitialized = true;
            kMissedResponses = 0;
            kTimer = now;
            kState = KLineState::READY;
        }
        break;

    case KLineState::READY:
        if (now - kTimer >= K_LINE_POLL_INTERVAL)
        {
            sendKLineCommand(K_LINE_CMD_DATA);
            kState = KLineState::REQ_CMD_SENT;
        }
        break;

    case KLineState::REQ_CMD_SENT:
        switch (readKLineResponse(now))
        {
        case 1:
            // Frame must echo the command; the mod-256 payload checksum
            // alone would accept an all-zero frame from an idle line
            if (kResponse[0] == K_LINE_CMD_DATA &&
                (byte)(kResponse[1] + kResponse[2] + kResponse[3] + kResponse[4]) == kResponse[5])
            {
                klineSpeed = kResponse[2];
                klineCoolant = kResponse[4];
                kMissedResponses = 0;
            }
            else
            {
                kMissedResponses++;
            }
            kTimer = now;
            kState = KLineState::READY;
            break;

        case -1:
            kMissedResponses++;
            kTimer = now;
            kState = KLineState::READY;
            break;
        }

        // ECU stopped answering -- treat the session as dead and re-init
        if (kMissedResponses >= K_LINE_MAX_MISSED_RESPONSES)
        {
            kInitialized = false;
            kMissedResponses = 0;
            kTimer = now;
            kState = KLineState::RETRY_WAIT;
        }
        break;

    case KLineState::RETRY_WAIT:
        if (now - kTimer >= K_LINE_RETRY_DELAY)
        {
            sendKLineCommand(K_LINE_CMD_INIT);
            kState = KLineState::INIT_CMD_SENT;
        }
        break;
    }
}
