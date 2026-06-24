#pragma once
#include <Arduino.h>
#include <LittleFS.h>

struct ReceptionEntry {
    bool matched;
    float rssi;
    float snr;
    size_t packetLength;
};

class ReceptionLog {
public:
    bool begin() {
        if (!LittleFS.begin(true)) {
            Serial.println("[Log] LittleFS mount failed");
            return false;
        }
        Serial.println("[Log] LittleFS mounted");
        findNextTestNumber();
        Serial.print("[Log] Next test: test_");
        Serial.println(testNumber);
        return true;
    }

    void record(const ReceptionEntry& entry) {
        if (!fileCreated) createLogFile();
        if (filePath.isEmpty()) return;

        File f = LittleFS.open(filePath, FILE_APPEND);
        if (!f) {
            Serial.println("[Log] Failed to open log file for append");
            return;
        }

        uint32_t sec = millis() / 1000UL;
        char line[80];
        snprintf(line, sizeof(line), "%lu,%s,%.1f,%.1f,%u\n",
                 sec,
                 entry.matched ? "true" : "false",
                 entry.rssi,
                 entry.snr,
                 (unsigned)entry.packetLength);
        f.print(line);
        f.close();

        Serial.print("[Log] Written to ");
        Serial.println(filePath);
    }

    int getTestNumber() const { return testNumber; }
    const String& getFilePath() const { return filePath; }

    void dumpAllLogs() {
        File root = LittleFS.open("/");
        File f = root.openNextFile();
        if (!f) {
            Serial.println("[Log] No log files found");
            return;
        }
        while (f) {
            Serial.print("[Log] --- /");
            Serial.print(f.name());
            Serial.println(" ---");
            while (f.available()) {
                Serial.write(f.read());
            }
            Serial.println("[Log] --- end ---");
            Serial.println();
            f = root.openNextFile();
        }
    }

    void deleteAllLogs() {
        File root = LittleFS.open("/");
        File f = root.openNextFile();
        int count = 0;
        while (f) {
            String name = "/" + String(f.name());
            f = root.openNextFile();
            LittleFS.remove(name);
            count++;
        }
        Serial.print("[Log] Deleted ");
        Serial.print(count);
        Serial.println(" file(s)");
        filePath = "";
        fileCreated = false;
        testNumber = 1;
    }

    void listLogFiles() {
        Serial.println("[Log] Files on LittleFS:");
        File root = LittleFS.open("/");
        File f = root.openNextFile();
        while (f) {
            Serial.print("  ");
            Serial.print(f.name());
            Serial.print("  ");
            Serial.print(f.size());
            Serial.println(" bytes");
            f = root.openNextFile();
        }
    }

private:
    void findNextTestNumber() {
        testNumber = 1;
        while (true) {
            String path = "/test_" + String(testNumber) + ".csv";
            if (!LittleFS.exists(path)) break;
            testNumber++;
        }
    }

    void createLogFile() {
        filePath = "/test_" + String(testNumber) + ".csv";
        File f = LittleFS.open(filePath, FILE_WRITE);
        if (!f) {
            Serial.println("[Log] Failed to create log file");
            filePath = "";
            return;
        }
        f.println("seconds,matched,rssi,snr,packet_length");
        f.close();
        fileCreated = true;

        Serial.print("[Log] Created ");
        Serial.println(filePath);
    }

    int testNumber = 1;
    bool fileCreated = false;
    String filePath;
};
