#pragma once

#include <string.h>

#define K_LINE_SERIAL Serial

byte klineBuffer[64];

int activeCode = 0;
int diagCodes[] = {1, 2, 3, 5, 6, 7, 8, 9, 20, 21, 30, 31, 32, 33, 36, 37, 38, 39, 48, 50, 51, 52, 60, 61, 62, 70};

void requestResponsePair(byte command)
{
    K_LINE_SERIAL.write(command);

    delay(10);

    int available = K_LINE_SERIAL.available();
    if (available > 0)
        K_LINE_SERIAL.readBytes(klineBuffer, min(available, (int)sizeof(klineBuffer)));

    delay(5);
}

static bool responseMatches(const byte expected[6])
{
    return memcmp(klineBuffer, expected, 6) == 0;
}

bool initKLine()
{
    // Wait for the ECU to initialize before talking to it
    delay(611);

    requestResponsePair(0xFE);

    const byte ack[6] = {0xFE, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (!responseMatches(ack))
        return false;

    delay(16);
    return true;
}

bool enterDiagMode()
{
    const byte diagReady[6] = {0xCD, 0x00, 0x00, 0x40, 0xD0, 0x10};
    do
    {
        requestResponsePair(0xCD);
    } while (!responseMatches(diagReady));

    for (int i = 0; i < 20; i++)
        requestResponsePair(0xCB);

    const byte diagEntered[6] = {0xCD, 0x00, 0x00, 0x40, 0x01, 0x41};
    do
    {
        requestResponsePair(0xCD);
    } while (!responseMatches(diagEntered));

    return true;
}

int getNormalData()
{
    requestResponsePair(0x01);

    // Validate checksum
    if (klineBuffer[1] + klineBuffer[2] + klineBuffer[3] + klineBuffer[4] != klineBuffer[5])
        return -1;

    return klineBuffer[4];
}
