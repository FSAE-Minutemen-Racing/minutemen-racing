#pragma once
#ifndef KLINE_HPP
#define KLINE_HPP

#define K_LINE_SERIAL Serial1

byte buffer[64];

int activeCode = 0;
int diagCodes[] = {1, 2, 3, 5, 6, 7, 8, 9, 20, 21, 30, 31, 32, 33, 36, 37, 38, 39, 48, 50, 51, 52, 60, 61, 62, 70};

void requestReponsePair(byte command)
{
    K_LINE_SERIAL.write(command);

    delay(10);

    int available = K_LINE_SERIAL.available();
    if (available > 0)
    {
        K_LINE_SERIAL.readBytes(buffer, (min(available, (int)sizeof(buffer))));
    }

    delay(5);
}

bool initKLine()
{
    Serial.println("Starting ECU communication...");

    // Wait for 611 milliseconds to allow the ECU to initialize properly
    delay(611);

    // Send initizalization command
    requestReponsePair(0xFE);

    if (buffer[0] == 0xFE && buffer[1] == 0x00 && buffer[2] == 0x00 && buffer[3] == 0x00 && buffer[4] == 0x00 && buffer[5] == 0x00)
    {
        delay(16);
        Serial.println("Starting ECU communication SUCCESSFUL");
        return true;
    }
    else
    {
        Serial.println("Starting ECU communication FAILED");
        return false;
    }
}

bool enterDiagMode()
{
    while (true)
    {
        requestReponsePair(0xCD);

        if (buffer[0] == 0xCD && buffer[1] == 0x00 && buffer[2] == 0x00 && buffer[3] == 0x40 && buffer[4] == 0xD0 && buffer[5] == 0x10)
        {
            break;
        }
    }

    for (int i = 0; i < 20; i++)
    {
        requestReponsePair(0xCB);
    }

    while (true)
    {
        requestReponsePair(0xCD);

        if (buffer[0] == 0xCD && buffer[1] == 0x00 && buffer[2] == 0x00 && buffer[3] == 0x40 && buffer[4] == 0x01 && buffer[5] == 0x41)
            return true;
    }
}

int getNormalData()
{
    requestReponsePair(0x01);

    // Validate checksum
    if (buffer[1] + buffer[2] + buffer[3] + buffer[4] != buffer[5])
        return -1;

    return buffer[4];
}

int getDiagData()
{
    requestReponsePair(0xCA);

    // Validate checksum
    if (buffer[1] + buffer[2] + buffer[3] + buffer[4] != buffer[5])
        return -1;

    // Tag packet, check diag active code
    if (buffer[3] == 0x40)
    {
        activeCode = buffer[4];
        return 400 + activeCode;
    }

    // Data packet, read value
    else
        return buffer[4];
}

void switchDiagCode(int code)
{
    while (activeCode != code)
    {
        requestReponsePair(0xCC);

        if (buffer[3] == 0x40)
        {
            activeCode = buffer[4];
            requestReponsePair(0xCA);
        }
    }
}
#endif