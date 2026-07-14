#pragma once
#include <Arduino.h>
#include <LittleFS.h>

// Mirrors every Serial.print/println/printf call anywhere in this project
// into a per-boot log file on LittleFS, without touching any existing call
// site. This header is force-included at the top of every translation unit
// (see platformio.ini's `-include`), which lets it capture RealSerial — a
// reference to the genuine hardware serial object — BEFORE redefining the
// `Serial` token below to point at the tee-ing wrapper. From this point on
// in each file, "Serial" transparently means "the real port, plus whatever
// log file is currently attached". Only SerialLogger.cpp itself talks to
// RealSerial directly (writing into TeeSerial via the "Serial" macro would
// recurse into itself).
// Derives from Stream (not just Print) because some library headers we
// depend on (e.g. Adafruit BusIO) declare defaulted parameters like
// `Stream* s = &Serial` — with the macro below active while compiling
// those headers, "Serial" resolves to g_loggedSerial, so it must satisfy
// Stream* for those call sites to keep compiling unmodified.
class TeeSerial : public Stream {
public:
    void begin(unsigned long baud);
    void attachLogFile(File file);
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    int available() override;
    int read() override;
    int peek() override;

private:
    static constexpr uint32_t FLUSH_INTERVAL_MS = 1000;

    void maybeFlush();

    File     logFile_;
    bool     logFileFull_ = false;
    uint32_t lastFlushMs_ = 0;
};

extern TeeSerial g_loggedSerial;
static HWCDC& RealSerial __attribute__((unused)) = ::Serial;

#define Serial g_loggedSerial

namespace SerialLogger {

// Mounts LittleFS (format-on-fail). Safe to call even if it fails; logging
// to file is then simply skipped for the rest of the boot.
void beginFilesystem();

// Blocks up to 10 seconds waiting for "list", "show <n>", or "delete" on
// the real serial port. If one of those is entered, executes it and then
// loops forever accepting further commands — this function never returns
// and the incubator never starts up. Returns normally only if the 10 s
// window elapses with no recognized command.
void runStartupPrompt();

// Scans existing "<n>.log" files, creates "<max+1>.log", and attaches it to
// g_loggedSerial so all subsequent Serial output is also written to it.
void startNewLogFile();

}  // namespace SerialLogger
