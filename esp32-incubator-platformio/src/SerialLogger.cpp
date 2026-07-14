#include "SerialLogger.h"

TeeSerial g_loggedSerial;

void TeeSerial::begin(unsigned long baud) {
    RealSerial.begin(baud);
}

void TeeSerial::attachLogFile(File file) {
    logFile_ = file;
    logFileFull_ = false;
    lastFlushMs_ = millis();
}

// LittleFS flush() is a real flash-sync operation, not free — throttling it
// keeps a burst of many small print() calls (e.g. one sensor-reading block)
// from turning into one flash write per line. Worst case this risks losing
// up to FLUSH_INTERVAL_MS of the most recent log output on a crash, which is
// an acceptable trade for a diagnostic log.
void TeeSerial::maybeFlush() {
    uint32_t now = millis();
    if (now - lastFlushMs_ >= FLUSH_INTERVAL_MS) {
        logFile_.flush();
        lastFlushMs_ = now;
    }
}

size_t TeeSerial::write(uint8_t c) {
    size_t n = RealSerial.write(c);
    if (logFile_ && !logFileFull_) {
        if (logFile_.write(c) != 1) {
            logFileFull_ = true;
        } else {
            maybeFlush();
        }
    }
    return n;
}

size_t TeeSerial::write(const uint8_t* buffer, size_t size) {
    size_t n = RealSerial.write(buffer, size);
    if (logFile_ && !logFileFull_ && size > 0) {
        if (logFile_.write(buffer, size) != size) {
            logFileFull_ = true;
        } else {
            maybeFlush();
        }
    }
    return n;
}

int TeeSerial::available() { return RealSerial.available(); }
int TeeSerial::read()      { return RealSerial.read(); }
int TeeSerial::peek()      { return RealSerial.peek(); }

namespace {

bool g_fsReady = false;

// Matches "<digits>.log" (with or without a leading '/'). Returns the parsed
// number on success.
bool parseLogNumber(String name, int& outNum) {
    if (name.startsWith("/")) name = name.substring(1);
    if (!name.endsWith(".log")) return false;
    String numPart = name.substring(0, name.length() - 4);
    if (numPart.length() == 0) return false;
    for (size_t i = 0; i < numPart.length(); i++) {
        if (!isDigit(numPart[i])) return false;
    }
    outNum = numPart.toInt();
    return true;
}

void doList() {
    RealSerial.println("Log files:");
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    bool any = false;
    while (f) {
        if (!f.isDirectory()) {
            RealSerial.print("  ");
            RealSerial.print(f.name());
            RealSerial.print("  (");
            RealSerial.print(f.size());
            RealSerial.println(" bytes)");
            any = true;
        }
        f.close();
        f = root.openNextFile();
    }
    root.close();
    if (!any) RealSerial.println("  (none)");
}

void doShow(int n) {
    String path = "/" + String(n) + ".log";
    if (!LittleFS.exists(path)) {
        RealSerial.print("No such logfile: ");
        RealSerial.println(path);
        return;
    }
    File f = LittleFS.open(path, "r");
    RealSerial.print("--- ");
    RealSerial.print(path);
    RealSerial.println(" ---");
    while (f.available()) {
        RealSerial.write((uint8_t)f.read());
    }
    f.close();
    RealSerial.println();
    RealSerial.println("--- end ---");
}

void doDelete() {
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    int count = 0;
    while (f) {
        String name = f.name();
        bool isDir = f.isDirectory();
        f.close();
        int n;
        if (!isDir && parseLogNumber(name, n)) {
            String path = name.startsWith("/") ? name : "/" + name;
            if (LittleFS.remove(path)) count++;
        }
        f = root.openNextFile();
    }
    root.close();
    RealSerial.print("Deleted ");
    RealSerial.print(count);
    RealSerial.println(" logfile(s)");
}

// Returns true if `line` was a recognized command (and executes it).
bool handleCommand(const String& line) {
    if (line == "list") { doList(); return true; }
    if (line == "delete") { doDelete(); return true; }
    if (line.startsWith("show ")) {
        doShow(line.substring(5).toInt());
        return true;
    }
    return false;
}

// Blocks until a full line (terminated by '\n') is received, or timeoutMs
// elapses (0 = wait forever). Returns false on timeout.
bool readLineBlocking(String& outLine, uint32_t timeoutMs) {
    outLine = "";
    uint32_t start = millis();
    while (timeoutMs == 0 || millis() - start < timeoutMs) {
        while (RealSerial.available()) {
            char c = (char)RealSerial.read();
            if (c == '\n') {
                outLine.trim();
                return true;
            }
            if (c != '\r') outLine += c;
        }
        delay(5);
    }
    return false;
}

}  // namespace

namespace SerialLogger {

void beginFilesystem() {
    g_fsReady = LittleFS.begin(true);
    if (!g_fsReady) {
        RealSerial.println("[Log] LittleFS mount failed — logging to file disabled");
    }
}

void runStartupPrompt() {
    if (!g_fsReady) return;

    // Discard anything already buffered — e.g. stray bytes some hosts send
    // when first opening the native-USB CDC port — so it can't masquerade
    // as a real command and cut the window short.
    delay(300);
    while (RealSerial.available()) RealSerial.read();

    RealSerial.println();
    RealSerial.println("=== Type 'list', 'show <n>', or 'delete' within 10s for log command mode ===");

    uint32_t deadline = millis() + 10000;
    String   line;
    while (millis() < deadline) {
        uint32_t remaining = deadline - millis();
        if (remaining == 0) break;
        if (readLineBlocking(line, remaining) && line.length() > 0) {
            if (handleCommand(line)) {
                // A valid command was entered — stay here forever, the
                // incubator never starts up.
                for (;;) {
                    RealSerial.print("> ");
                    if (readLineBlocking(line, 0) && line.length() > 0 && !handleCommand(line)) {
                        RealSerial.println("Unrecognized command. Use: list | show <n> | delete");
                    }
                }
            }
            RealSerial.println("Unrecognized command. Use: list | show <n> | delete");
        }
    }
    RealSerial.println("Starting up normally.");
}

void startNewLogFile() {
    if (!g_fsReady) return;

    int maxN = 0;
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    while (f) {
        String name = f.name();
        bool isDir = f.isDirectory();
        f.close();
        int n;
        if (!isDir && parseLogNumber(name, n) && n > maxN) maxN = n;
        f = root.openNextFile();
    }
    root.close();

    String path = "/" + String(maxN + 1) + ".log";
    File logFile = LittleFS.open(path, FILE_WRITE);
    if (!logFile) {
        RealSerial.print("[Log] Failed to create ");
        RealSerial.println(path);
        return;
    }
    g_loggedSerial.attachLogFile(logFile);
    RealSerial.print("[Log] Logging to ");
    RealSerial.println(path);
}

}  // namespace SerialLogger
