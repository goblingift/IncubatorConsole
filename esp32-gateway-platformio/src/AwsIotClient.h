#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// MQTT publisher for AWS IoT Core over mutual TLS. Call begin() once in
// setup() and loop() on every main loop iteration to keep the connection
// alive and reconnect (with backoff) after drops. publish() is a no-op that
// returns false while not connected — callers should check isConnected()
// or the publish() return value; readings are not queued/retried.
class AwsIotClient {
public:
    void begin();
    void loop();
    bool publish(const char* topic, const char* payload);
    bool isConnected();

private:
    static constexpr unsigned long RETRY_BASE_DELAY_MS = 2000;
    static constexpr unsigned long RETRY_MAX_DELAY_MS  = 30000;
    static constexpr uint8_t       RETRY_BACKOFF_CAP   = 4;

    WiFiClientSecure netClient_;
    PubSubClient     mqttClient_{netClient_};

    unsigned long nextAttemptMs_ = 0;
    uint8_t       retryCount_    = 0;

    void tryConnect();
    unsigned long retryDelay() const;
};
