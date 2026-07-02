#include "AwsIotClient.h"
#include <WiFi.h>
#include "AwsIotConfig.h"

void AwsIotClient::begin() {
    netClient_.setCACert(AWS_IOT_ROOT_CA);
    netClient_.setCertificate(AWS_IOT_CERT);
    netClient_.setPrivateKey(AWS_IOT_PRIVATE_KEY);
    mqttClient_.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
}

unsigned long AwsIotClient::retryDelay() const {
    uint8_t exponent = retryCount_ < RETRY_BACKOFF_CAP ? retryCount_ : RETRY_BACKOFF_CAP;
    unsigned long delay = RETRY_BASE_DELAY_MS << exponent;
    return delay < RETRY_MAX_DELAY_MS ? delay : RETRY_MAX_DELAY_MS;
}

void AwsIotClient::tryConnect() {
    Serial.print("[AWS IoT] Connecting to \"");
    Serial.print(AWS_IOT_ENDPOINT);
    Serial.println("\" ...");

    if (mqttClient_.connect(AWS_IOT_CLIENT_ID)) {
        Serial.println("[AWS IoT] Connected");
        retryCount_ = 0;
    } else {
        Serial.print("[AWS IoT] Connect failed, state ");
        Serial.println(mqttClient_.state());

        char tlsError[128];
        netClient_.lastError(tlsError, sizeof(tlsError));
        Serial.print("[AWS IoT] TLS error: ");
        Serial.println(tlsError);

        if (retryCount_ < 255) retryCount_++;
        nextAttemptMs_ = millis() + retryDelay();
        Serial.print("[AWS IoT] Retrying in ");
        Serial.print(retryDelay());
        Serial.println(" ms");
    }
}

void AwsIotClient::loop() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!mqttClient_.connected()) {
        if (millis() >= nextAttemptMs_) {
            tryConnect();
        }
        return;
    }

    mqttClient_.loop();
}

bool AwsIotClient::publish(const char* topic, const char* payload) {
    if (!mqttClient_.connected()) return false;
    return mqttClient_.publish(topic, payload);
}

bool AwsIotClient::isConnected() {
    return mqttClient_.connected();
}
