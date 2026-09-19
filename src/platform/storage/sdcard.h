#pragma once
#include <stdint.h>

// microSD (TF) slot. Verified SPI-mode pins from the official Waveshare
// ESP32-S3-Touch-AMOLED-1.75 schematic (SD-CARD block): GPIO1=MOSI, GPIO2=SCK,
// GPIO3=MISO, GPIO41=CS. Do NOT guess these — see docs/HARDWARE.md.
namespace sdcard {
    bool begin();          // mounts at /sd; false if no card is present
    bool mounted();
    uint64_t sizeBytes();

    // The card is one SPI device behind a FAT layer that is not safe to enter from two tasks
    // at once, and this firmware does enter it from both cores: the audio task streams
    // chimes, the ADS-B task reads road tiles, the loop reads theme art and runs the
    // installer. Every call into SD, SD.open included, must be made holding this.
    //
    // It is one recursive mutex, created in begin() and never lazily, so there is nothing to
    // race on and nothing to remember: a function that takes a Guard can call another that
    // does. A Guard covers an OPERATION (open, read, close; or one whole short file). A
    // transfer that stays open across many calls, such as an upload, locks per call instead
    // of holding the card for its whole length, so a long copy never starves a chime.
    //
    // tools/check_sd_guard.py, run by tests/test_sd_guard.py, fails on any SD.* call that is
    // not inside a Guard, because a note asking people to remember this had already failed:
    // the old lock was opt-in and two of the six readers took it.
#ifdef ARDUINO
    void lock();
    void unlock();
#else
    inline void lock() {}      // the simulator's card is a directory and has one thread
    inline void unlock() {}
#endif
    struct Guard {
        Guard()  { lock(); }
        ~Guard() { unlock(); }
        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
    };
}
