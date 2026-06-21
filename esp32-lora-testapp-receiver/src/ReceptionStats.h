#pragma once
#include <Arduino.h>
#include <vector>

class ReceptionStats {
public:
    void recordReception(bool matched, float rssi, float snr, uint32_t receiveMs) {
        totalReceived++;
        if (matched) matchCount++;
        else mismatchCount++;
        rssiValues.push_back(rssi);
        snrValues.push_back(snr);
        receiveTimes.push_back(receiveMs);
    }

    uint32_t getTotalReceived() const { return totalReceived; }
    uint32_t getMatchCount() const { return matchCount; }
    uint32_t getMismatchCount() const { return mismatchCount; }
    const std::vector<float>& getRssiValues() const { return rssiValues; }
    const std::vector<float>& getSnrValues() const { return snrValues; }

    float getAvgRssi() const {
        if (rssiValues.empty()) return 0;
        float sum = 0;
        for (float v : rssiValues) sum += v;
        return sum / rssiValues.size();
    }

    float getAvgSnr() const {
        if (snrValues.empty()) return 0;
        float sum = 0;
        for (float v : snrValues) sum += v;
        return sum / snrValues.size();
    }

    float getMinSnr() const {
        if (snrValues.empty()) return 0;
        float m = snrValues[0];
        for (float v : snrValues) if (v < m) m = v;
        return m;
    }

    float getMaxSnr() const {
        if (snrValues.empty()) return 0;
        float m = snrValues[0];
        for (float v : snrValues) if (v > m) m = v;
        return m;
    }

    float getMinRssi() const {
        if (rssiValues.empty()) return 0;
        float m = rssiValues[0];
        for (float v : rssiValues) if (v < m) m = v;
        return m;
    }

    float getMaxRssi() const {
        if (rssiValues.empty()) return 0;
        float m = rssiValues[0];
        for (float v : rssiValues) if (v > m) m = v;
        return m;
    }

    float getAvgReceiveTime() const {
        if (receiveTimes.empty()) return 0;
        uint32_t sum = 0;
        for (uint32_t v : receiveTimes) sum += v;
        return (float)sum / receiveTimes.size();
    }

    uint32_t getMinReceiveTime() const {
        if (receiveTimes.empty()) return 0;
        uint32_t m = receiveTimes[0];
        for (uint32_t v : receiveTimes) if (v < m) m = v;
        return m;
    }

    uint32_t getMaxReceiveTime() const {
        if (receiveTimes.empty()) return 0;
        uint32_t m = receiveTimes[0];
        for (uint32_t v : receiveTimes) if (v > m) m = v;
        return m;
    }

    void printSummary() const {
        Serial.println();
        Serial.println("====== Reception Statistics ======");
        Serial.print("  Total received:  "); Serial.println(totalReceived);
        Serial.print("  Matched:         "); Serial.println(matchCount);
        Serial.print("  Mismatched:      "); Serial.println(mismatchCount);
        if (totalReceived > 0) {
            Serial.print("  Success rate:    ");
            Serial.print(matchCount * 100.0f / totalReceived, 1);
            Serial.println(" %");
            Serial.print("  RSSI avg/min/max: ");
            Serial.print(getAvgRssi(), 1); Serial.print(" / ");
            Serial.print(getMinRssi(), 1); Serial.print(" / ");
            Serial.print(getMaxRssi(), 1); Serial.println(" dBm");
            Serial.print("  SNR  avg/min/max: ");
            Serial.print(getAvgSnr(), 1); Serial.print(" / ");
            Serial.print(getMinSnr(), 1); Serial.print(" / ");
            Serial.print(getMaxSnr(), 1); Serial.println(" dB");
            Serial.print("  Recv time avg/min/max: ");
            Serial.print(getAvgReceiveTime(), 0); Serial.print(" / ");
            Serial.print(getMinReceiveTime()); Serial.print(" / ");
            Serial.print(getMaxReceiveTime()); Serial.println(" ms");
        }
        Serial.println("==================================");
        Serial.println();
    }

private:
    uint32_t totalReceived = 0;
    uint32_t matchCount = 0;
    uint32_t mismatchCount = 0;
    std::vector<float> rssiValues;
    std::vector<float> snrValues;
    std::vector<uint32_t> receiveTimes;
};
