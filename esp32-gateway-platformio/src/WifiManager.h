#pragma once

#include <Arduino.h>

// Non-blocking WiFi station manager with automatic reconnect (exponential
// backoff). Call begin() once in setup() and loop() on every main loop
// iteration — neither call blocks.
class WifiManager {
public:
    void begin();
    void loop();
    bool isConnected() const;

private:
    enum class State { Idle, Connecting, Connected };

    static constexpr unsigned long CONNECT_TIMEOUT_MS   = 15000;
    static constexpr unsigned long RETRY_BASE_DELAY_MS  = 2000;
    static constexpr unsigned long RETRY_MAX_DELAY_MS   = 60000;
    static constexpr uint8_t       RETRY_BACKOFF_CAP    = 5;  // 2s * 2^5 = 64s, clamped to max above

    State         state_          = State::Idle;
    unsigned long connectStartMs_ = 0;
    unsigned long nextAttemptMs_  = 0;
    uint8_t       retryCount_     = 0;

    void startConnect();
    unsigned long retryDelay() const;
};
