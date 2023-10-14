#pragma once

#include <ArduinoLog.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
// #include <CircularBuffer.h>
#include "timebuffer.h"

#include "pins.h"

#define SIGNAL_THRESHOLD 2000
#define BUFFER_DURATION_SECS 1
#define SAMPLES_PER_SECOND 10

struct SbhPressureMon
{
    enum MON_STATE {
        STATE_ZEROING,
        STATE_IDLE,
        STATE_HIGH,
        STATE_LOW,
    };

    Adafruit_BMP280 bmp;
    MON_STATE state;
    bool changedState, isActive;

    typedef TimeBuffer<int, SAMPLES_PER_SECOND, BUFFER_DURATION_SECS> Buffer;
    Buffer pressureSamples;

    int lastPollTime;
    int zeroRef, lastSignalSize;

    bool Init() {
        Log.infoln("=== Preparing BMP280 ===");
        if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
            Log.fatalln("Could not find a valid BMP280 sensor, check wiring!");
            return false;
        } else {
            Log.infoln("BMP280 found");
        }

        bmp.setSampling(
            Adafruit_BMP280::MODE_NORMAL,
            Adafruit_BMP280::SAMPLING_NONE,
            Adafruit_BMP280::SAMPLING_X1,
            Adafruit_BMP280::FILTER_OFF,
            Adafruit_BMP280::STANDBY_MS_1
        );

        state = STATE_ZEROING;
        lastPollTime = -1;
        zeroRef = -1;
        changedState = false;

        delay(1);
        bmp.readPressure();

        return true;
    }

    bool IsReady() {
        return state != STATE_ZEROING;
    }

    bool Poll() {
        auto now = millis();
        if (now - lastPollTime >= 1000.f / SAMPLES_PER_SECOND) {
            int p = bmp.readPressure();
            pressureSamples.add(p);
            Log.traceln("last pressure %i", p);
            lastPollTime = now;

            if (state == STATE_ZEROING && pressureSamples.isFull()) {
                Log.traceln("Zeroing");
                state = STATE_IDLE;
                changedState = true;
                Buffer::View v;
                if (!pressureSamples.viewDuration(&v, -BUFFER_DURATION_SECS * 0.5f)) {
                    Log.fatalln("Unable to get view");
                }
                zeroRef = v.getAverage();
                Log.traceln("zeroRef = %i", zeroRef);
            }

            return true;
        } else {
            return false;
        }
    }

    void Update() {
        // Log.traceln("SbhPressureMon::Update");
        Buffer::View active, inactive;
        pressureSamples.view(-0.5f, &inactive, &active);

        int activeAvg = active.getAverage(), lastValue = pressureSamples.lastValue();
        Log.verboseln("activeAvg = %i | lastValue = %i", activeAvg, lastValue);

        changedState = false;

        switch (state) {
        case STATE_IDLE: {
            // TODO - wait for stable variance before transitioning to other states
            int diff = abs(lastValue - zeroRef);
            Log.verboseln("diff=%i", diff);
            if (diff > SIGNAL_THRESHOLD) {
                if (lastValue > zeroRef) state = STATE_HIGH;
                else state = STATE_LOW;

                lastSignalSize = diff;
                changedState = true;
            }
        } break;

        case STATE_LOW:
        case STATE_HIGH: {
            if (abs(lastValue - zeroRef) < lastSignalSize / 2) {
                state = STATE_IDLE;
                changedState = true;
            } else if (state == STATE_HIGH && lastValue - zeroRef < -SIGNAL_THRESHOLD) {
                state = STATE_LOW;
                changedState = true;
            } else if (state == STATE_LOW && lastValue - zeroRef > SIGNAL_THRESHOLD) {
                state = STATE_HIGH;
                changedState = true;
            }
        } break;
        }
    }
};