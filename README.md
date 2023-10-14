# Spongebob Horn

https://www.printables.com/model/615046-spongebob-loser-call-horn

_Why?_

---

Included audio files are in `audio/set1` ("loser") and `audio/set2` ("BK"). Copy the contents to the root of the micro SD card. The code is written to start from "set1" and advance to the next "set" when you press the button on the FireBeetle board, but the button doesn't seem to work for me. Change `audio.h:L65` to hard-code a file path.

An odd number of WAV files (at most 5) are required in each folder, which are played in order. Odd-numbered files are played once, even-numbered files are looped until the pressure sensor detects "a notable change". This way you can change the duration of each part of the audio based on how long you exhale/inhale.

(There's logic for MP3 support but it doesn't loop well and tends to crash.)

The program needs time to get a base reading of the ambient pressure. Without putting your mouth to the mouthpiece, hold the power button for at least 1 second. Continue to hold the power button and blow into the mouthpiece.

Some things I wanted to do but couldn't be bothered:

- Volume control
- Shorten (or remove) the initial pressure calibration period
- Make the "audio change" button work
- Use Seeed ESP32C3 instead of DFRobot FireBeetle (smaller, lower-power) (this was the original plan but I couldn't get it to work reliably with all the pins populated)
- General code cleanup