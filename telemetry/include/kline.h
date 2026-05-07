#pragma once
#ifndef KLINE_HPP
#define KLINE_HPP

#define K_LINE_SERIAL Serial

byte buffer[64];
int speed = 0;
int coolant = 45;

// --- State Machine ---
enum class KLineState {
    IDLE,
    INIT_WAIT,        // 611 ms power-on wait
    INIT_CMD_SENT,    // waiting 10 ms for ECU response
    INIT_CMD_READ,    // waiting 5 ms post-read
    INIT_POST_DELAY,  // 16 ms after successful init
    READY,
    REQ_CMD_SENT,     // waiting 10 ms for response
    REQ_CMD_READ,     // waiting 5 ms post-read
    ERROR
};

static KLineState kState     = KLineState::IDLE;
static unsigned long kTimer  = 0;
static byte pendingCommand   = 0x00;
static bool initSuccess      = false;

bool initKLine() {
    unsigned long now = millis();

    switch (kState) {
        case KLineState::IDLE:
            kTimer = now;
            kState = KLineState::INIT_WAIT;
            break;

        case KLineState::INIT_WAIT:
            if (now - kTimer >= 611) {
                K_LINE_SERIAL.write(0xFE);
                kTimer = now;
                kState = KLineState::INIT_CMD_SENT;
            }
            break;

        case KLineState::INIT_CMD_SENT:
            if (now - kTimer >= 10) {
                int available = K_LINE_SERIAL.available();
                if (available > 0)
                    K_LINE_SERIAL.readBytes(buffer, min(available, (int)sizeof(buffer)));
                kTimer = now;
                kState = KLineState::INIT_CMD_READ;
            }
            break;

        case KLineState::INIT_CMD_READ:
            if (now - kTimer >= 5) {
                if (buffer[0] == 0xFE &&
                    buffer[1] == 0x00 && buffer[2] == 0x00 &&
                    buffer[3] == 0x00 && buffer[4] == 0x00 && buffer[5] == 0x00) {
                    kTimer = now;
                    kState = KLineState::INIT_POST_DELAY;
                } else {
                    kState = KLineState::ERROR;
                }
            }
            break;

        case KLineState::INIT_POST_DELAY:
            if (now - kTimer >= 16) {
                kState    = KLineState::READY;
                initSuccess = true;
                return true;   // init done, success
            }
            break;

        case KLineState::ERROR:
            return false;      // init done, failed

        default:
            break;
    }

    return false;  // still in progress
}

void getKLineData() {
    unsigned long now = millis();

    switch (kState) {
        case KLineState::READY:
            K_LINE_SERIAL.write(0x01);
            kTimer = now;
            kState = KLineState::REQ_CMD_SENT;
            break;

        case KLineState::REQ_CMD_SENT:
            if (now - kTimer >= 10) {
                int available = K_LINE_SERIAL.available();
                if (available > 0)
                    K_LINE_SERIAL.readBytes(buffer, min(available, (int)sizeof(buffer)));
                kTimer = now;
                kState = KLineState::REQ_CMD_READ;
            }
            break;

        case KLineState::REQ_CMD_READ:
            if (now - kTimer >= 5) {
                // Validate checksum then update values
                if (buffer[1] + buffer[2] + buffer[3] + buffer[4] == buffer[5]) {
                    speed   = buffer[2];
                    coolant = buffer[4];
                }
                kState = KLineState::READY;  // ready for next poll
            }
            break;

        default:
            break;
    }
}

#endif