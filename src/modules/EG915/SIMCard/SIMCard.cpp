#include "SIMCard.h"

#include "esp_log.h"

EG915SIMCard::EG915SIMCard(AsyncATHandler &handler) { init(handler); }

void EG915SIMCard::init(AsyncATHandler &handler) { at = &handler; }

static bool parseQSIMDET(const String &resp, EG915SimDetDual &out) {
  int pos = resp.indexOf("+QSIMDET:");
  if (pos < 0) return false;
  int lineEnd = resp.indexOf('\n', pos);
  String line = (lineEnd >= 0) ? resp.substring(pos, lineEnd) : resp.substring(pos);
  // Expected: +QSIMDET: (e1,l1),(e2,l2)
  int firstParen = line.indexOf('(');
  int secondParen = line.indexOf(')', firstParen + 1);
  int commaBetween = line.indexOf(',', secondParen + 1);
  int thirdParen = line.indexOf('(', commaBetween + 1);
  int fourthParen = line.indexOf(')', thirdParen + 1);
  if (firstParen < 0 || secondParen < 0 || thirdParen < 0 || fourthParen < 0) return false;
  String t1 = line.substring(firstParen + 1, secondParen);
  String t2 = line.substring(thirdParen + 1, fourthParen);
  t1.trim();
  t2.trim();
  auto parseTuple = [](const String &t, EG915SimDetConfig &cfg) {
    int comma = t.indexOf(',');
    if (comma < 0) return false;
    String e = t.substring(0, comma);
    String l = t.substring(comma + 1);
    e.trim();
    l.trim();
    if (e.length() == 0 || l.length() == 0) return false;
    cfg.cardDetection = static_cast<EG915SimDetConfig::CardDetection>(e.toInt());
    cfg.insertLevel = static_cast<EG915SimDetConfig::InsertLevel>(l.toInt());
    return u8(cfg.cardDetection) <= 1 && u8(cfg.insertLevel) <= 1;
  };
  if (!parseTuple(t1, out.sim1)) return false;
  if (!parseTuple(t2, out.sim2)) return false;
  return true;
}

static bool parseQDSIM(const String &resp, EG915SimSlot &slotOut) {
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
  slotOut = static_cast<EG915SimSlot>(val);
  return true;
}

static bool parseQSIMSTAT(const String &resp, EG915SimStatus &out) {
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
  out.enable = static_cast<EG915SimStatus::ReportState>(en);
  out.inserted = static_cast<EG915SimStatus::InsertStatus>(ins);
  return true;
}

EG915SimDetDual EG915SIMCard::getDetection() {
  EG915SimDetDual out{};
  if (!at) return out;
  String resp;
  if (!at->sendSync("AT+QSIMDET?", resp)) return out;
  if (!parseQSIMDET(resp, out)) { log_e("Failed to parse +QSIMDET response"); }
  return out;
}

EG915SimStatus EG915SIMCard::getStatus() {
  EG915SimStatus out{};
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

EG915SimSlot EG915SIMCard::getCurrentSlot() {
  if (!at) return currentSlot;
  String resp;
  if (!at->sendSync("AT+QDSIM?", resp)) return currentSlot;
  if (!parseQDSIM(resp, currentSlot)) {
    log_e("Failed to parse +QDSIM response");
    return currentSlot;
  }
  return currentSlot;
}

bool EG915SIMCard::setSlot(EG915SimSlot slot) {
  if (!at) return false;
  if (slot != EG915SimSlot::SLOT_1 && slot != EG915SimSlot::SLOT_2) return false;
  if (slot == currentSlot) return true;

  String cmd = String("AT+QDSIM=") + String(u8(slot));
  bool ok = at->sendSync(cmd);
  if (ok) { currentSlot = slot; }
  return ok;
}
