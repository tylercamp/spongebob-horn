#pragma once

#include <ArduinoLog.h>

#include <SD.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include "./AudioOutputI2S.h"

#include <Preferences.h>

#include <vector>

#include "pins.h"

Preferences prefs;

struct SbhAudio
{
    enum AUDIO_STATE {
        STATE_IDLE,
        STATE_PART1_SINGLE, // parts[0]
        STATE_PART2_LOOP, // parts[1]
        STATE_PART3_SINGLE, // ...
        STATE_PART4_LOOP,
        STATE_PART5_SINGLE,
    } state;

    bool shouldRetain; // whether to continue looping

    AUDIO_STATE _NextState(AUDIO_STATE cur) {
        if (cur == STATE_PART5_SINGLE) return STATE_IDLE;
        else return (AUDIO_STATE)(cur + 1);
    }

    std::vector<char>* rawAudio[5];
    AudioGenerator* gen;
    AudioOutputI2S* out;
    AudioFileSource* src;

    inline SbhAudio() {
        for (int i = 0; i < 5; i++) rawAudio[i] = nullptr;
    }

    inline bool Init()
    {
        for (int i = 0; i < 5; i++) {
            if (rawAudio[i]) delete rawAudio[i];
        }

        Log.infoln("== Reading audio ==");
        if (!SD.begin(SD_CARD_CS)) {
            Log.fatalln("SD.begin failed");
            return false;
        }

        prefs.begin("sbh_audio", false);
        if (!prefs.isKey("idx")) {
            prefs.putInt("idx", 0);
        }

        bool isMp3 = false;
        int i;
        String rootPath = "/set" + String(prefs.getInt("idx") + 1);
        File root = SD.open(rootPath);
        if (!root) {
            Log.infoln("failed to open %s, resetting idx=0", rootPath.c_str());
            prefs.putInt("idx", 0);
            rootPath = "/set1";
            root = SD.open(rootPath);
            if (!root) {
                Log.fatalln("could not find default folder at %s", rootPath.c_str());
                return false;
            }
        }
        for (i = 0; i < 5; i++)
        {
            File f = root.openNextFile();
            if (f)
            {
                Log.infoln("got file %s", f.name());
                if (String(f.name()).endsWith(".mp3")) isMp3 = true;
            }
            else
            {
                if (i == 0) {
                    Log.fatalln("failure");
                    return false;
                }
                else break;
            }
            size_t size = f.size();
            Log.verboseln("%i bytes", size);
            rawAudio[i] = new std::vector<char>(size);
            Log.verboseln("reading");
            Log.verboseln("read %i bytes", f.readBytes(rawAudio[i]->data(), size));
            f.close();
            Log.verboseln("closed");

            Log.verboseln("%i bytes remaining", ESP.getFreeHeap());
        }

        if (!(i % 2)) {
            Log.fatalln("must have odd number of files (got %i)", i);
            return false;
        }

        Log.infoln("=== Preparing I2S ===");
        out = new AudioOutputI2S();

        Log.verboseln("out->SetPinout(...) = %b", out->SetPinout(I2S_BCLK, I2S_WCLK, I2S_DOUT));
        // out->SetGain(0.1f);
        out->SetGain(0.8f);

        if (isMp3) gen = new AudioGeneratorMP3();
        else gen = new AudioGeneratorWAV();
        src = nullptr;

        state = STATE_IDLE;

        SD.end();

        return true;
    }

    void _AdvanceState() {
        state = _NextState(state);
    }

    void _RunSingleState(std::vector<char>* nextSrc) {
        if (!gen->loop()) {
            delete src;

            if (nextSrc) {
                _AdvanceState();

                src = new AudioFileSourcePROGMEM(nextSrc->data(), nextSrc->size());
                gen->begin(src, out);
                gen->loop();
            } else {
                state = STATE_IDLE;
            }
        }
    }

    void _RunLoopState(std::vector<char>* currentSrc, std::vector<char>* nextSrc) {
        if (!gen->loop()) {
            delete src;

            if (shouldRetain) {
                src = new AudioFileSourcePROGMEM(currentSrc->data(), currentSrc->size());
            } else {
                // guaranteed there will be a next state (init fails if input count is even)
                _AdvanceState();
                src = new AudioFileSourcePROGMEM(nextSrc->data(), nextSrc->size());
            }

            gen->begin(src, out);
            gen->loop();
        }
    }

    void Run() {
        switch (state) {
        case STATE_PART1_SINGLE: {
            out->seamless = true;
            _RunSingleState(rawAudio[1]);
        } break;

        case STATE_PART2_LOOP: {
            _RunLoopState(rawAudio[1], rawAudio[2]);
        } break;

        case STATE_PART3_SINGLE: {
            _RunSingleState(rawAudio[3]);
        } break;

        case STATE_PART4_LOOP: {
            _RunLoopState(rawAudio[3], rawAudio[4]);
        } break;

        case STATE_PART5_SINGLE: {
            out->seamless = false;
            _RunSingleState(nullptr);
        } break;

        }
    }

    void Start() {
        if (state == STATE_IDLE) {
            state = STATE_PART1_SINGLE;
            src = new AudioFileSourcePROGMEM(rawAudio[0]->data(), rawAudio[0]->size());
            gen->begin(src, out);
        }
    }

    bool IsPlaying() {
        return state != STATE_IDLE;
    }
};