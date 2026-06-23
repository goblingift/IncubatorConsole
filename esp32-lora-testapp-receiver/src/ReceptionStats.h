#pragma once
#include <Arduino.h>
#include <vector>

class ReceptionStats {
public:
    ReceptionStats(uint32_t expectedPackets) : expectedPacketCount(expectedPackets) {}

    void recordReception(bool matched, float rssi, float snr) {
        totalReceived++;
        if (matched) matchCount++;
        else mismatchCount++;
        rssiValues.push_back(rssi);
        snrValues.push_back(snr);
        lastRssi = rssi;
        lastSnr = snr;
        lastMatched = matched;
    }

    uint32_t getTotalReceived() const { return totalReceived; }
    uint32_t getMatchCount() const { return matchCount; }
    uint32_t getMismatchCount() const { return mismatchCount; }
    uint32_t getExpectedPacketCount() const { return expectedPacketCount; }
    float getLastRssi() const { return lastRssi; }
    float getLastSnr() const { return lastSnr; }
    bool wasLastMatched() const { return lastMatched; }

    float getPdr() const {
        return totalReceived * 100.0f / expectedPacketCount;
    }

    float getSuccessRate() const {
        if (totalReceived == 0) return 0;
        return matchCount * 100.0f / totalReceived;
    }

    float getAvgRssi() const {
        if (rssiValues.empty()) return 0;
        float sum = 0;
        for (float v : rssiValues) sum += v;
        return sum / rssiValues.size();
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
            Serial.print("  PDR:             ");
            Serial.print(totalReceived * 100.0f / expectedPacketCount, 1);
            Serial.print(" % (");
            Serial.print(totalReceived);
            Serial.print("/");
            Serial.print(expectedPacketCount);
            Serial.println(")");
        }
        Serial.println("==================================");
        Serial.println();
    }

private:
    uint32_t expectedPacketCount;
    uint32_t totalReceived = 0;
    uint32_t matchCount = 0;
    uint32_t mismatchCount = 0;
    float lastRssi = 0;
    float lastSnr = 0;
    bool lastMatched = false;
    std::vector<float> rssiValues;
    std::vector<float> snrValues;
};
