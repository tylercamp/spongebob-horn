#include <ArduinoLog.h>
#include <Arduino.h>

#include "pins.h"

#include "audio.h"
#include "pressuremon.h"

#include <Button2.h>

SbhAudio audio;
SbhPressureMon pressureMon;

Button2 button;

void printTimestamp(Print *_logOutput, int lvl) {
  char c[12];
  int m = sprintf(c, "%10lu ", millis());
  _logOutput->print(c);
}

void setup() {
  Serial.begin(115200);
  Log.setPrefix(printTimestamp);
  Log.begin(LOG_LEVEL_TRACE, &Serial);

  Log.verboseln("init");

  Log.infoln("free memory: %i", ESP.getFreeHeap());

  bool success = true;
  if (!audio.Init()) {
    success = false;
    Log.fatalln("audio.Init() failed");
  }

  if (!pressureMon.Init()) {
    success = false;
    Log.fatalln("pressureMon.Init() failed");
  }

  if (!success) {
    while (true) delay(1);
  }

  // TODO - button broken?
  // button.begin(27, INPUT);
  // button.setTapHandler([](Button2& b) {
  //   Log.infoln("updating to next audio set");
  //   prefs.putInt("idx", prefs.getInt("idx") + 1);
  //   if (!audio.Init()) {
  //     Log.fatalln("audio.Init() failed");
  //     while (true) delay(10);
  //   }
  // });
}

void LogPressureState() {
  if (pressureMon.changedState) {
      switch (pressureMon.state) {
        case SbhPressureMon::STATE_ZEROING: Log.infoln("State updated to ZEROING"); break;
        case SbhPressureMon::STATE_IDLE:
          Log.infoln("State updated to IDLE");
          break;

        case SbhPressureMon::STATE_HIGH: Log.infoln("State updated to HIGH"); break;

        case SbhPressureMon::STATE_LOW: Log.infoln("State updated to LOW"); break;
      }
    }
}

void loop() {
  bool polled = pressureMon.Poll();
  if (!pressureMon.IsReady()) {
    delay(1);
    return;
  }

  if (polled) {
    pressureMon.Update();
    LogPressureState();
  }

  // Serial.printf("%i\n", digitalRead(27));

  bool isActive = pressureMon.state == SbhPressureMon::STATE_HIGH || pressureMon.state == SbhPressureMon::STATE_LOW;
  if (isActive && !audio.IsPlaying()) {
    audio.Start();
  }

  audio.shouldRetain = !pressureMon.changedState && isActive;
  audio.Run();
}

