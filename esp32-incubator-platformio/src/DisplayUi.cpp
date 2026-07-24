#include "DisplayUi.h"
#include <stdio.h>
#include <string.h>

void DisplayUi::begin(U8G2* display, SemaphoreHandle_t i2cMutex) {
    display_   = display;
    i2cMutex_  = i2cMutex;
    dataMutex_ = xSemaphoreCreateMutex();
}

void DisplayUi::publish(const DisplaySnapshot& snap) {
    xSemaphoreTake(dataMutex_, portMAX_DELAY);
    snap_ = snap;
    seq_++;
    xSemaphoreGive(dataMutex_);
}

void DisplayUi::run() {
    uint32_t lastSeq    = 0;
    bool     lastPhase  = false;
    bool     needRedraw = false;

    for (;;) {
        DisplaySnapshot snap;
        uint32_t seq;
        xSemaphoreTake(dataMutex_, portMAX_DELAY);
        snap = snap_;
        seq  = seq_;
        xSemaphoreGive(dataMutex_);

        if (snap.valid) {
            bool alertPhase = snap.alert && ((millis() / ALTERNATE_MS) & 1);
            if (seq != lastSeq || alertPhase != lastPhase) {
                needRedraw = true;
                lastSeq    = seq;
                lastPhase  = alertPhase;
            }
            if (needRedraw) {
                // Drawing targets the RAM framebuffer — no bus access. Only
                // sendBuffer() touches I2C and thus needs the bus mutex.
                display_->clearBuffer();
                if (alertPhase) renderAlertList(snap);
                else            renderNormal(snap);
                if (xSemaphoreTake(i2cMutex_, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) == pdTRUE) {
                    display_->sendBuffer();
                    xSemaphoreGive(i2cMutex_);
                    needRedraw = false;
                }
                // On bus timeout needRedraw stays true — retried next tick.
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void DisplayUi::renderNormal(const DisplaySnapshot& s) {
    char vBuf[20], iBuf[20], tBuf[20], hBuf[20], co2Buf[20], timeBuf[6];
    snprintf(vBuf, sizeof(vBuf), "V:%.1fV %u%%", s.voltage, s.batteryPct);
    snprintf(iBuf, sizeof(iBuf), "I:   %.3f A", s.currentA);
    if (s.scdValid) {
        snprintf(tBuf, sizeof(tBuf), "T:   %.1f C", s.temperature);
        snprintf(hBuf, sizeof(hBuf), "H:   %.0f %%", s.humidity);
        if (s.alert) {
            // Without " ppm" the row can't collide with the "!" glyph.
            snprintf(co2Buf, sizeof(co2Buf), "CO2: %u", s.co2);
        } else {
            snprintf(co2Buf, sizeof(co2Buf), "CO2: %u ppm", s.co2);
        }
    } else {
        strncpy(tBuf,   "T:   ---", sizeof(tBuf));
        strncpy(hBuf,   "H:   ---", sizeof(hBuf));
        strncpy(co2Buf, "CO2: ---", sizeof(co2Buf));
    }
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", s.hour, s.minute);

    display_->setFont(u8g2_font_ncenB10_tr);
    display_->drawStr(128 - display_->getStrWidth(timeBuf), 12, timeBuf);
    display_->drawStr(0, 28,  vBuf);
    display_->drawStr(0, 46,  iBuf);
    display_->drawStr(0, 64,  tBuf);
    display_->drawStr(0, 82,  hBuf);
    display_->drawStr(0, 100, co2Buf);
    display_->drawStr(0, 118, s.alert ? "ALERT!" : "Incubator OK");

    if (s.alert) {
        display_->setFont(u8g2_font_ncenB24_tr);
        display_->drawStr(114, 88, "!");
    }
}

void DisplayUi::renderAlertList(const DisplaySnapshot& s) {
    display_->setFont(u8g2_font_ncenB10_tr);
    const char* title = "!! ALERT !!";
    display_->drawStr((128 - display_->getStrWidth(title)) / 2, 12, title);
    display_->drawHLine(0, 16, 128);

    display_->setFont(u8g2_font_6x12_tr);
    const uint8_t maxLines = 9;  // baselines 28..116, 11 px apart
    uint8_t listed = (s.violationCount > maxLines) ? (uint8_t)(maxLines - 1)
                                                   : s.violationCount;
    uint8_t y = 28;
    for (uint8_t i = 0; i < listed; i++, y += 11) {
        display_->drawStr(0, y, s.lines[i]);
    }
    if (s.violationCount > maxLines) {
        char more[16];
        snprintf(more, sizeof(more), "+%u more", s.violationCount - listed);
        display_->drawStr(0, y, more);
    }
}

void displayUiTask(void* param) {
    static_cast<DisplayUi*>(param)->run();
}
