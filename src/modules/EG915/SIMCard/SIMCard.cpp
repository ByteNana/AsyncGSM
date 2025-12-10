#include "SIMCard.h"

#include "esp_log.h"

EG915SIMCard::EG915SIMCard(AsyncATHandler &handler) { init(handler); }

void EG915SIMCard::init(AsyncATHandler &handler) { at = &handler; }

static bool parseQSIMDET(const String &resp, SIMDetectionConfig &out) {
  int pos = resp.indexOf("+QSIMDET:");
  if (pos < 0) return false;
  int lineEnd = resp.indexOf('\n', pos);
  String line = (lineEnd >= 0) ? resp.substring(pos, lineEnd) : resp.substring(pos);
  // Expected: +QSIMDET: <cardDetection>,<insertLevel>
  int colon = line.indexOf(':');
  if (colon < 0) return false;
  String payload = line.substring(colon + 1);
  payload.trim();
  int comma = payload.indexOf(',');
  if (comma < 0) return false;
  String e = payload.substring(0, comma);
  String l = payload.substring(comma + 1);
  e.trim();
  l.trim();
  if (e.length() == 0 || l.length() == 0) return false;
  int ei = e.toInt();
  int li = l.toInt();
  if (ei < 0 || ei > 1) return false;
  if (li < 0 || li > 1) return false;
  out.cardDetection = static_cast<SIMDetectionConfig::CardDetection>(ei);
  out.insertLevel = static_cast<SIMDetectionConfig::InsertLevel>(li);
  return true;
}

static bool parseQDSIM(const String &resp, SIMSlot &slotOut) {
  int pos = resp.indexOf("+QDSIM:");
  if (pos < 0) return false;
  int lineEnd = resp.indexOf('\n', pos);
  String line = (lineEnd >= 0) ? resp.substring(pos, lineEnd) : resp.substring(pos);
  int colon = line.indexOf(':');
  if (colon < 0) return false;
  String payload = line.substring(colon + 1);
  payload.trim();
  if (payload.length() == 0) return false;
  int val = payload.toInt();
  if (val != 0 && val != 1) return false;
  slotOut = static_cast<SIMSlot>(val);
  return true;
}

static bool parseQSIMSTAT(const String &resp, SIMStatusReport &out) {
  int pos = resp.indexOf("+QSIMSTAT:");
  if (pos < 0) return false;
  int lineEnd = resp.indexOf('\n', pos);
  String line = (lineEnd >= 0) ? resp.substring(pos, lineEnd) : resp.substring(pos);
  int colon = line.indexOf(':');
  if (colon < 0) return false;
  String payload = line.substring(colon + 1);
  payload.trim();
  int comma = payload.indexOf(',');
  if (comma < 0) return false;
  String e = payload.substring(0, comma);
  String s = payload.substring(comma + 1);
  e.trim();
  s.trim();
  if (e.length() == 0 || s.length() == 0) return false;
  int en = e.toInt();
  int ins = s.toInt();
  if (en < 0 || en > 1) return false;
  if (ins < 0 || ins > 2) return false;
  out.enable = static_cast<SIMStatusReport::ReportState>(en);
  out.inserted = static_cast<SIMStatusReport::InsertStatus>(ins);
  return true;
}

SIMDetectionConfig EG915SIMCard::getDetection() {
  SIMDetectionConfig out{};
  if (!at) return out;
  String resp;
  if (!at->sendSync("AT+QSIMDET?", resp)) return out;
  if (!parseQSIMDET(resp, out)) { log_e("Failed to parse +QSIMDET response"); }
  return out;
}

SIMStatusReport EG915SIMCard::getStatus() {
  SIMStatusReport out{};
  if (!at) return out;
  String resp;
  if (!at->sendSync("AT+QSIMSTAT?", resp)) return out;
  if (!parseQSIMSTAT(resp, out)) { log_e("Failed to parse +QSIMSTAT response"); }
  return out;
}

bool EG915SIMCard::setStatusReport(bool enable) {
  if (!at) return false;
  String cmd = String("AT+QSIMSTAT=") + (enable ? "1" : "0");
  return at->sendSync(cmd);
}

SIMSlot EG915SIMCard::getCurrentSlot() {
  if (!at) return currentSlot;
  String resp;
  if (!at->sendSync("AT+QDSIM?", resp)) return currentSlot;
  if (!parseQDSIM(resp, currentSlot)) {
    log_e("Failed to parse +QDSIM response");
    return currentSlot;
  }
  return currentSlot;
}

bool EG915SIMCard::setSlot(SIMSlot slot) {
  if (!at) return false;
  if (slot != SIMSlot::SLOT_1 && slot != SIMSlot::SLOT_2) return false;
  if (slot == currentSlot) return true;

  String cmd = String("AT+QDSIM=") + String(u8(slot));
  bool ok = at->sendSync(cmd);
  if (ok) { currentSlot = slot; }
  return ok;
}

bool EG915SIMCard::isReady() {
  if (!at) return false;
  String resp;
  if (!at->sendSync("AT+CPIN?", resp)) return false;
  return resp.indexOf("READY") != -1;
}
