#include "WifiManager.h"
#include <WiFi.h>
#include "WifiConfig.h"

void WifiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.setSleep(false);
    startConnect();
}

void WifiManager::startConnect() {
    Serial.print("[WiFi] Connecting to \"");
    Serial.print(WIFI_SSID);
    Serial.println("\" ...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    state_          = State::Connecting;
    connectStartMs_ = millis();
}

unsigned long WifiManager::retryDelay() const {
    uint8_t exponent = retryCount_ < RETRY_BACKOFF_CAP ? retryCount_ : RETRY_BACKOFF_CAP;
    unsigned long delay = RETRY_BASE_DELAY_MS << exponent;
    return delay < RETRY_MAX_DELAY_MS ? delay : RETRY_MAX_DELAY_MS;
}

void WifiManager::loop() {
    unsigned long now = millis();

    switch (state_) {
        case State::Idle:
            if (now >= nextAttemptMs_) {
                startConnect();
            }
            break;

        case State::Connecting:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("[WiFi] Connected, IP: ");
                Serial.println(WiFi.localIP());
                state_      = State::Connected;
                retryCount_ = 0;
            } else if (now - connectStartMs_ >= CONNECT_TIMEOUT_MS) {
                Serial.println("[WiFi] Connect attempt timed out");
                WiFi.disconnect(true);
                state_ = State::Idle;
                if (retryCount_ < 255) retryCount_++;
                nextAttemptMs_ = now + retryDelay();
                Serial.print("[WiFi] Retrying in ");
                Serial.print(retryDelay());
                Serial.println(" ms");
            }
            break;

        case State::Connected:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Connection lost");
                state_         = State::Idle;
                retryCount_    = 0;
                nextAttemptMs_ = now;
            }
            break;
    }
}

bool WifiManager::isConnected() const {
    return state_ == State::Connected;
}
