#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "ControlLogic.h"

// Snapshot of everything the display renders; produced once per measurement
// cycle by sensorReadingTask, consumed by the display task.
struct DisplaySnapshot {
    bool     valid;  // false until the first measurement cycle completes
    float    voltage;
    float    currentA;
    float    temperature;
    float    humidity;
    uint16_t co2;
    bool     scdValid;
    uint8_t  hour, minute;
    bool     alert;
    uint8_t  violationCount;
    char     lines[MAX_VIOLATIONS][VIOLATION_LINE_LEN];
};

// Owns the OLED rendering: the normal measurement screen, plus — while any
// value is out of range — a big "!" on its right edge and a 5 s alternation
// with a violation-list screen. publish() and run() execute on different
// tasks; the snapshot handoff is guarded by an internal mutex, the actual I2C
// framebuffer push by the shared bus mutex.
class DisplayUi {
public:
    void begin(U8G2* display, SemaphoreHandle_t i2cMutex);
    void publish(const DisplaySnapshot& snap);  // sensor task, once per cycle
    void run();                                 // display task body, never returns

private:
    static constexpr uint32_t TICK_MS        = 250;
    static constexpr uint32_t ALTERNATE_MS   = 5000;
    static constexpr uint32_t I2C_TIMEOUT_MS = 300;

    U8G2*             display_   = nullptr;
    SemaphoreHandle_t i2cMutex_  = nullptr;
    SemaphoreHandle_t dataMutex_ = nullptr;
    DisplaySnapshot   snap_      = {};
    uint32_t          seq_       = 0;  // guarded by dataMutex_

    void renderNormal(const DisplaySnapshot& s);
    void renderAlertList(const DisplaySnapshot& s);
};

// FreeRTOS task entry; param = DisplayUi*.
void displayUiTask(void* param);
