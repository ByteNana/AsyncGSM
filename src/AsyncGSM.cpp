#include "AsyncGSM.h"
#include "esp_log.h"
#include <limits>
#include <algorithm> // For std::min

AsyncGSM::AsyncGSM() : _connected(false) {
}

bool AsyncGSM::init(Stream &stream) {
    _connected = false;
    log_i("Initializing AsyncGSM...");
    if (!at.begin(stream)) {
        log_e("Failed to initialize AsyncATHandler");
        return false;
    }
    return true;
}

int AsyncGSM::timedRead() {
    log_d("timedRead() called");
    unsigned long startTime = millis();
    const unsigned long timeout = 1000;

    while (millis() - startTime < timeout) {
        int c = read();
        if (c >= 0) {
            return c;
        }
        delay(10);
    }

    log_d("timedRead timeout");
    return -1;
}

bool AsyncGSM::begin(const char *apn) {
    bool _canRead = false;
    for (int i = 0; i < 4; i++) {
        if (at.sendCommand("AT", "OK", 2000)) {
            _canRead = true;
            break;
        }
    }
    if (!_canRead) {
        log_e("Failed to communicate with modem");
        return false;
    }

    if (!at.sendCommand("ATE0", "OK", 2000)) {
        log_e("Failed to set echo off");
        return false;
    }

    if (!at.sendCommand("AT+CMEE=2", "OK", 2000)) {
        log_w("Failed to enable verbose errors");
    }

    if (!at.sendCommand("AT+CGMI", "Quectel")) {
        log_e("Failed to get modem manufacturer");
        return false;
    }

    if (!at.sendCommand("AT+CGMM", "EG915U")) {
        log_e("Failed to get modem model");
        return false;
    }

    if (!at.sendCommand("AT+CTZU=1", "OK")) {
        log_e("Failed to set time zone update");
        return false;
    }

    while (!at.sendCommand("AT+CPIN?", "OK", 2000)) {
        log_w("Waiting for SIM card...");
        delay(1000);
    }

    for (int i = 0; i < 4; i++) {
        String response;
        at.sendCommand("AT+QICLOSE=" + String(i), response, "OK", 5000);
    }

    if (!at.sendCommand("AT+QSCLK=0", "OK")) {
        log_e("Failed to disable sleep mode");
        return false;
    }

    if (!gprsConnect(apn)) {
        return false;
    }

    for (int i = 0; i < 10; ++i) {
        String resp;
        if (at.sendCommand("AT+CGATT?", resp, "+CGATT:", 1000)) {
            if (resp.indexOf("+CGATT: 1") != -1) {
                return true;
            }
        }
        delay(500);
    }
    return false;
}

int AsyncGSM::connect(IPAddress ip, uint16_t port) {
    return connect(ip.toString().c_str(), port);
}

int AsyncGSM::connect(const char *host, uint16_t port) {
    log_i("Connecting to %s:%d", host, port);
    stop();
    _connected = modemConnect(host, port);
    return _connected ? 1 : 0;
}

void AsyncGSM::stop() {
    log_i("Stopping connection...");
    _connected = false;

    if (!at.sendCommand("AT+QICLOSE=0", "OK", 4000)) {
        log_e("Failed to close connection");
    }
    if (!at.sendCommand("AT+QIDEACT=1", "OK", 4000)) {
        log_w("PDP context deactivation failed or already inactive");
    }
}

size_t AsyncGSM::write(uint8_t c) {
    log_d("write() called with character: '%c' (0x%02X)", c, c);
    return write(&c, 1);
}

size_t AsyncGSM::write(const uint8_t *buf, size_t size) {
    if (!_connected || !at._stream) {
        log_e("Not connected or stream not initialized");
        return 0;
    }

    if (size == 0) return 0;

    log_i("Writing %zu bytes to modem...", size);

    String response;
    if (!at.sendCommand(response, ">", 10000, "AT+QISEND=0,", String(size))) {
        log_e("Failed to send AT+QISEND command");
        return 0;
    }

    at._stream->write(buf, size);
    at._stream->flush();

    if (!at.waitResponse(15000, "SEND OK") || at.getResponse("SEND OK").empty()) {
        log_e("Failed to get SEND OK confirmation");
        return 0;
    }

    log_i("Write successful.");
    return size;
}

int AsyncGSM::available() {
    log_d("available() called");
    if (!_connected) {
        log_e("Not connected, returning 0");
        return 0;
    }

    String response;
    if (!at.sendCommand(response, "+QIRD:", 1000, "AT+QIRD=0")) {
        return 0;
    }

    int lastQirdPos = response.lastIndexOf("+QIRD:");
    if (lastQirdPos == -1) {
        return 0;
    }

    String qirdPart = response.substring(lastQirdPos);
    int startPos = qirdPart.indexOf(':');
    int lengthEnd = qirdPart.indexOf('\r', startPos);
    String lengthStr = qirdPart.substring(startPos + 1, lengthEnd);
    lengthStr.trim();
    return lengthStr.toInt();
}

int AsyncGSM::read() {
    log_d("read() called");
    if (!_connected) {
        return -1;
    }

    // We get a single byte from the bulk read function.
    uint8_t buf[1];
    int bytesRead = read(buf, 1);
    if (bytesRead == 1) {
        return buf[0];
    }
    return -1;
}

int AsyncGSM::read(uint8_t *buf, size_t size) {
    log_d("read(buf, size) called with size: %zu", size);
    if (!_connected || size == 0) return 0;

    String response;
    // Send AT+QIRD=0,<size> to request the specified number of bytes
    at.sendAT("AT+QIRD=0,", size);

    if (at.waitResponse(5000, "+QIRD:") == 0) {
        log_e("No +QIRD response received");
        return 0;
    }

    // Now read the data from the buffer
    int bytesRead = at.readData(buf, size);
    log_d("read(buf, size) returning total bytes read: %zu", bytesRead);

    return bytesRead;
}

int AsyncGSM::peek() {
    log_d("peek() called");
    if (!_connected) return -1;
    return at._stream->peek();
}

void AsyncGSM::flush() {
    log_d("flush() called");
    if (!at._stream) {
        log_e("Stream not initialized");
        return;
    }
    at._stream->flush();
}

uint8_t AsyncGSM::connected() {
    log_d("connected() called");
    if (!_connected) return 0;
    return 1;
}
