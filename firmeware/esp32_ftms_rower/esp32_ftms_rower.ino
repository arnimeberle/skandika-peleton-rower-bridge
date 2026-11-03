/*
 * ESP32 FTMS Rower (Prototyp)
 * ---------------------------------------------------------------
 * Gibt sich per Bluetooth LE als Rudergerät (Fitness Machine Service 0x1826)
 * aus, damit die Peloton-App es als "Connect a Bluetooth Device" findet.
 *
 * Modus 1 (SIMULATE = true):  simuliert Schläge (Schlagrate schwankt leicht)
 * Modus 2 (SIMULATE = false): zählt Reed-Impulse an REED_PIN
 *                             (Annahme: 1 Impuls = 1 Schlag)
 *
 * Benötigt: Board "ESP32" (Arduino-Core) + Bibliothek
 *           "NimBLE-Arduino" von h2zero, Version 2.x (aktuell)
 *
 * Reed-Kontakt: ein Pin an REED_PIN, der andere an GND (interner Pull-up).
 */

#include <NimBLEDevice.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// ---------------- Konfiguration ----------------
#define SIMULATE          true      // false = echter Reed-Kontakt
#define REED_PIN          4         // GPIO für Reed-Kontakt (Modus 2)
#define DEBOUNCE_US       300000UL  // 300 ms Entprellung
#define PULSES_PER_STROKE 5         // Kalibrierung: fünf Reed-Impulse pro vollständigem Zug
#define STROKE_TIMEOUT_MS 5000UL    // keine Schläge -> Rate 0
#define METERS_PER_STROKE 9.0f      // grobe Annahme: Meter pro Schlag
#define DEVICE_NAME       "ESP32 Rower"
#define NOTIFY_INTERVAL   1000      // ms

#define I2C_SDA           21
#define I2C_SCL           22
#define I2C_TAKT_HZ       100000
// -------------------------------------------------

// BLE-Objekte
NimBLEServer*         server   = nullptr;
NimBLECharacteristic* chRower  = nullptr;  // 0x2AD1 Rower Data
NimBLECharacteristic* chCtrl   = nullptr;  // 0x2AD9 Control Point
NimBLECharacteristic* chStatus = nullptr;  // 0x2ADA Machine Status
NimBLECharacteristic* chTrain  = nullptr;  // 0x2AD3 Training Status

bool deviceConnected = false;

// Kein separater OLED-Reset-Pin. GPIO1 ist UART0-TX und darf nicht belegt werden.
Adafruit_SSD1306 oled(128, 32, &Wire, -1);


// Schlagdaten
volatile uint32_t strokeCountIsr = 0;
volatile uint32_t lastStrokeUs   = 0;
volatile bool havePreviousStroke = false;

uint16_t strokeCount   = 0;
float    totalDistance = 0;   // Meter
float    strokeRate    = 0;   // Schläge/min
uint32_t startMs       = 0;
uint32_t lastStrokeMs  = 0;

// Simulation
uint32_t simNextStrokeMs = 0;

// ---------- Reed-Interrupt ----------
/*
void IRAM_ATTR onReed() {
  uint32_t now = micros();
  if (now - lastStrokeUs < DEBOUNCE_US) return;
  strokeIntervalUs = now - lastStrokeUs;
  lastStrokeUs = now;
  strokeCountIsr++;
}
*/

void IRAM_ATTR onReed() {
  uint32_t now = micros();

  if (havePreviousStroke &&
      (uint32_t)(now - lastStrokeUs) < DEBOUNCE_US) {
    return;
  }

  lastStrokeUs = now;
  havePreviousStroke = true;
  strokeCountIsr++;
}

// ---------- BLE Callbacks ----------
class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    startMs = millis();
    Serial.println("Client verbunden");
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("Client getrennt, starte Advertising neu");
    NimBLEDevice::getAdvertising()->start();
  }
};

class CtrlCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    NimBLEAttValue v = c->getValue();
    if (v.size() == 0) return;
    uint8_t op = v.data()[0];
    Serial.printf("Control Point Opcode: 0x%02X\n", op);

    // Antwort: 0x80, Request-Opcode, Result (0x01 = Erfolg)
    uint8_t resp[3] = {0x80, op, 0x01};
    if (op > 0x11 && op < 0x80) resp[2] = 0x02;  // nicht unterstützt
    c->setValue(resp, 3);
    c->indicate();

    // Status-Meldung bei Start/Stop
    if (op == 0x07) { uint8_t s[1] = {0x04}; chStatus->setValue(s, 1); chStatus->notify(); }
    if (op == 0x08) { uint8_t s[2] = {0x02, 0x01}; chStatus->setValue(s, 2); chStatus->notify(); }
  }
};

// ---------- Aktuelle Ruderwerte berechnen ----------
void calculateRowerMetrics(uint32_t& dist, uint16_t& pace, int16_t& power) {
  // Pace (Sekunden pro 500 m) aus Schlagrate und Meter/Schlag
  float speed = (strokeRate / 60.0f) * METERS_PER_STROKE;  // m/s
  pace = 0;
  power = 0;
  if (speed > 0.1f) {
    float p = 500.0f / speed;
    if (p > 600) p = 600;
    pace = (uint16_t)p;
    float perMeter = p / 500.0f;
    power = (int16_t)(2.8f / (perMeter * perMeter * perMeter));  // Concept2-Formel
  }

  dist = (uint32_t)totalDistance;
}

// ---------- Rower Data senden ----------
void sendRowerData() {
  uint32_t dist;
  uint16_t pace;
  int16_t power;
  calculateRowerMetrics(dist, pace, power);

  uint16_t elapsed = (millis() - startMs) / 1000;

  // Flags: Total Distance (bit2) + Inst. Pace (bit3) + Inst. Power (bit5) + Elapsed Time (bit11)
  // Bit0 = 0 -> Stroke Rate + Stroke Count sind enthalten
  uint16_t flags = 0x0004 | 0x0008 | 0x0020 | 0x0800;

  uint8_t buf[14];
  int i = 0;
  buf[i++] = flags & 0xFF;
  buf[i++] = flags >> 8;
  buf[i++] = (uint8_t)(strokeRate * 2);          // Auflösung 0,5 spm
  buf[i++] = strokeCount & 0xFF;
  buf[i++] = strokeCount >> 8;
  buf[i++] = dist & 0xFF;                         // Total Distance, 24 bit
  buf[i++] = (dist >> 8) & 0xFF;
  buf[i++] = (dist >> 16) & 0xFF;
  buf[i++] = pace & 0xFF;
  buf[i++] = pace >> 8;
  buf[i++] = power & 0xFF;
  buf[i++] = (power >> 8) & 0xFF;
  buf[i++] = elapsed & 0xFF;
  buf[i++] = elapsed >> 8;

  chRower->setValue(buf, i);
  chRower->notify();
}

// ---------- Schlagverarbeitung ----------
void registerStroke(float intervalSec) {
  strokeCount++;
  totalDistance += METERS_PER_STROKE;
  lastStrokeMs = millis();
  if (intervalSec > 0.3f) {
    float inst = 60.0f / intervalSec;
    strokeRate = (strokeRate == 0) ? inst : (0.7f * strokeRate + 0.3f * inst);  // glätten
  }
}

void drawStatusDisplay(float strokeRate, uint16_t strokeCount, uint16_t pace,
                       uint32_t dist, int16_t power) {


  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("SPM: "); oled.print((int)strokeRate);
  oled.print(" FQZ: "); oled.print((int)strokeCount);
  oled.setCursor(0, 12);
  oled.print("Pace "); oled.print((int)pace);
  oled.print(" Dist: "); oled.print((int)dist);
  oled.setCursor(0, 24);  
  oled.print("Power: "); oled.print((int)power);

  /*
  oled.setCursor(0, 17);
  oled.print("VIC"); oled.print(s.victronValues ? "+" : "-");
  oled.print(" "); oled.print(timeText(s.rtcReady));
  oled.setCursor(0, 23);
  oled.setTextColor(COLOR_ORANGE);
  oled.print("B: ");
  oled.print(BUILD_VERSION);
  oled.setCursor(0, 29);
  oled.print("IP ");
  oled.print(s.wlan ? WiFi.localIP().toString() : "--");
  */
  oled.display();
  
}


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nESP32 Rower startet");
  Wire.begin(I2C_SDA, I2C_SCL, I2C_TAKT_HZ);
  
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // ggf. Adresse prüfen
    Serial.println("OLED init failed");
    while (true) delay(10);
  }
  oled.clearDisplay();
  oled.display();
  drawStatusDisplay(0, 0, 0, 0, 0);

  if (!SIMULATE) {
    pinMode(REED_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(REED_PIN), onReed, FALLING);
  }
  NimBLEDevice::init(DEVICE_NAME);
  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  NimBLEService* svc = server->createService(NimBLEUUID((uint16_t)0x1826));

  // Fitness Machine Feature (0x2ACC): Cadence, Distance, Pace, Elapsed Time, Power
  NimBLECharacteristic* chFeature = svc->createCharacteristic(
      NimBLEUUID((uint16_t)0x2ACC), NIMBLE_PROPERTY::READ);
  uint32_t features = 0x00000002 | 0x00000004 | 0x00000020 | 0x00001000 | 0x00004000;
  uint8_t feat[8] = {0};
  memcpy(feat, &features, 4);
  chFeature->setValue(feat, 8);

  chRower = svc->createCharacteristic(
      NimBLEUUID((uint16_t)0x2AD1), NIMBLE_PROPERTY::NOTIFY);

  chCtrl = svc->createCharacteristic(
      NimBLEUUID((uint16_t)0x2AD9),
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
  chCtrl->setCallbacks(new CtrlCB());

  chStatus = svc->createCharacteristic(
      NimBLEUUID((uint16_t)0x2ADA), NIMBLE_PROPERTY::NOTIFY);

  chTrain = svc->createCharacteristic(
      NimBLEUUID((uint16_t)0x2AD3),
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  uint8_t ts[2] = {0x00, 0x01};  // Status: "Other/Idle"
  chTrain->setValue(ts, 2);

  svc->start();

  // Device Information Service (0x180A), wie bei echten Geräten
  NimBLEService* dis = server->createService(NimBLEUUID((uint16_t)0x180A));
  NimBLECharacteristic* chMan = dis->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A29), NIMBLE_PROPERTY::READ);
  chMan->setValue("ESP32 DIY");
  NimBLECharacteristic* chMod = dis->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A24), NIMBLE_PROPERTY::READ);
  chMod->setValue("FTMS Rower");
  dis->start();

  // Advertising-Paket: Flags + FTMS-UUID + FTMS Service Data
  // Service Data laut FTMS-Spezifikation: Flags 0x01 (Machine Available),
  // Machine Type 0x0010 (Bit 4 = Rower), little endian
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);  // LE General Discoverable, kein BR/EDR
  advData.addServiceUUID(NimBLEUUID((uint16_t)0x1826));
  advData.setServiceData(NimBLEUUID((uint16_t)0x1826), std::string("\x01\x10\x00", 3));

  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);

  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->enableScanResponse(true);
  adv->start();

  Serial.println(SIMULATE ? "Modus: SIMULATION" : "Modus: REED-KONTAKT");
  Serial.println("Advertising gestartet, in Peloton nach 'ESP32 Rower' suchen");
}

void loop() {
  static uint32_t lastNotify = 0;
  static uint32_t seenIsr = 0;
  static uint32_t lastGroupedStrokeMs = 0;
  uint32_t now = millis();

  if (SIMULATE) {
    // Schlagrate schwankt zwischen ca. 22 und 26 spm
    if (now >= simNextStrokeMs) {
      float spm = 24.0f + 2.0f * sinf(now / 15000.0f);
      float interval = 60.0f / spm;
      registerStroke(interval);
      simNextStrokeMs = now + (uint32_t)(interval * 1000);
    }
  /*} else {
    noInterrupts();
    uint32_t cnt = strokeCountIsr;
    uint32_t iv  = strokeIntervalUs;
    interrupts();
    while (seenIsr < cnt) {
      seenIsr++;
      registerStroke(iv / 1000000.0f);
    }
  }*/
  } else {
    noInterrupts();
    uint32_t cnt = strokeCountIsr;
    interrupts();

    while (seenIsr < cnt) {
      seenIsr++;
      if (seenIsr % PULSES_PER_STROKE == 0) {
        float strokeIntervalSec = lastGroupedStrokeMs == 0
                                    ? 0.0f
                                    : (now - lastGroupedStrokeMs) / 1000.0f;
        if (strokeIntervalSec * 1000.0f > STROKE_TIMEOUT_MS) {
          strokeIntervalSec = 0.0f;  // erster Schlag nach einer Pause
          strokeRate = 0;
        }
        lastGroupedStrokeMs = now;
        registerStroke(strokeIntervalSec);
        Serial.printf("SCHLAG erkannt | Anzahl=%u | Schlagintervall=%.3f s\n",
                      strokeCount, strokeIntervalSec);
      }
    }
  }  


  // Kein Schlag mehr -> Rate langsam auf 0
  if (strokeRate > 0 && now - lastStrokeMs > STROKE_TIMEOUT_MS) strokeRate = 0;

  // Sensorwerte und OLED laufen unabhängig von einer BLE-Verbindung.
  if (now - lastNotify >= NOTIFY_INTERVAL) {
    lastNotify = now;

    uint32_t dist;
    uint16_t pace;
    int16_t power;
    calculateRowerMetrics(dist, pace, power);
    drawStatusDisplay(strokeRate, strokeCount, pace, dist, power);

    Serial.printf("SPM %.1f | Schläge %u | %lu m | Pace %u s/500m | %d W | BLE %s\n",
                  strokeRate, strokeCount, (unsigned long)dist, pace, power,
                  deviceConnected ? "verbunden" : "getrennt");

    if (deviceConnected) sendRowerData();
  }
  delay(5);
}
