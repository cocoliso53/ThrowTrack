#include <Arduino.h>
#include <SPI.h>
#include <DW1000Ranging.h>
#include <cstring>

#ifdef NODE_ROLE_INITIATOR
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

namespace {
constexpr uint8_t PIN_RST = D2;
constexpr uint8_t PIN_CS = D3;
constexpr uint8_t PIN_IRQ = D1;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 2000;

uint32_t lastHeartbeatMs = 0;
bool uwbReady = false;
uint32_t rangeCount = 0;

#ifdef NODE_ROLE_INITIATOR
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int8_t OLED_RESET = -1;
constexpr uint32_t DISPLAY_INTERVAL_MS = 2000;
constexpr uint32_t RANGE_TIMEOUT_MS = 4000;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

bool oledReady = false;
bool hasRange = false;
float latestRangeM = 0.0F;
uint32_t lastRangeMs = 0;
uint32_t lastDisplayMs = 0;

char nodeAddress[] = "7D:00:22:EA:82:60:3B:9C";
#else
char nodeAddress[] = "82:17:5B:D5:A9:9A:E2:9C";
#endif

void onNewRange();
void onNewDevice(DW1000Device* device);
void onInactiveDevice(DW1000Device* device);
#ifdef NODE_ROLE_INITIATOR
void updateDisplay(uint32_t now);
void printRawSpiDiagnostics();
#endif
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

#ifdef NODE_ROLE_INITIATOR
  Serial.println("ThrowTrack UWB initiator starting");
#else
  Serial.println("ThrowTrack UWB responder starting");
#endif

#ifdef NODE_ROLE_INITIATOR
  Wire.begin(D4, D5);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  if (oledReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ThrowTrack");
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.println("Waiting...");
    display.display();
  } else {
    Serial.println("OLED not found at I2C address 0x3C");
  }
#endif

  // SPI uses the XIAO defaults: D8=SCK, D9=MISO, D10=MOSI.
  DW1000Ranging.initCommunication(PIN_RST, PIN_CS, PIN_IRQ);

  char deviceId[128];
  DW1000.getPrintableDeviceIdentifier(deviceId);
  Serial.print("BU01 SPI check: ");
  Serial.println(deviceId);

#ifdef NODE_ROLE_INITIATOR
  if (strncmp(deviceId, "DECA", 4) != 0) {
    Serial.println("BU01 error: invalid DW1000 device ID; ranging disabled");
    updateDisplay(millis());
    return;
  }
#endif
  uwbReady = true;

  DW1000Ranging.attachNewRange(onNewRange);
  DW1000Ranging.attachInactiveDevice(onInactiveDevice);

#ifdef NODE_ROLE_INITIATOR
  DW1000Ranging.attachNewDevice(onNewDevice);
  DW1000Ranging.startAsTag(
      nodeAddress, DW1000.MODE_LONGDATA_RANGE_ACCURACY);
#else
  DW1000Ranging.attachBlinkDevice(onNewDevice);
  DW1000Ranging.startAsAnchor(
      nodeAddress, DW1000.MODE_LONGDATA_RANGE_ACCURACY);
#endif
}

void loop() {
  if (uwbReady) {
#ifdef DW1000_POLL_IRQ
    if (digitalRead(PIN_IRQ) == HIGH) {
      DW1000.handleInterrupt();
    }
#endif
    DW1000Ranging.loop();
  }

  const uint32_t now = millis();

#ifdef NODE_ROLE_INITIATOR
  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    updateDisplay(now);
  }
#endif

  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;

#ifdef NODE_ROLE_INITIATOR
    Serial.print("[alive] initiator");
#else
    Serial.print("[alive] responder");
#endif

    Serial.print(" | ranges: ");
    Serial.print(rangeCount);
    Serial.print(" | uwb: ");
    Serial.println(uwbReady ? "ready" : "error");

#ifdef NODE_ROLE_INITIATOR
    if (!uwbReady) printRawSpiDiagnostics();
#endif
  }
}

namespace {
void onNewRange() {
  ++rangeCount;

  DW1000Device* device = DW1000Ranging.getDistantDevice();
  const float rangeM = device->getRange();

#ifdef NODE_ROLE_INITIATOR
  latestRangeM = rangeM;
  lastRangeMs = millis();
  hasRange = true;
#endif

  Serial.print("distance: ");
  Serial.print(rangeM, 2);
  Serial.print(" m\trx power: ");
  Serial.print(device->getRXPower(), 1);
  Serial.println(" dBm");
}

void onNewDevice(DW1000Device* device) {
  Serial.print("peer detected: 0x");
  Serial.println(device->getShortAddress(), HEX);
}

void onInactiveDevice(DW1000Device* device) {
  Serial.print("peer inactive: 0x");
  Serial.println(device->getShortAddress(), HEX);
}

#ifdef NODE_ROLE_INITIATOR
void printRawSpiDiagnostics() {
  constexpr uint8_t modes[] = {SPI_MODE0, SPI_MODE1, SPI_MODE2, SPI_MODE3};

  for (uint8_t mode = 0; mode < 4; ++mode) {
    uint8_t id[4];

    SPI.beginTransaction(SPISettings(500000, MSBFIRST, modes[mode]));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x00);  // Read DEV_ID register 0x00, no sub-address.
    for (uint8_t &value : id) {
      value = SPI.transfer(0x00);
    }
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    const uint32_t rawId =
        static_cast<uint32_t>(id[0]) |
        (static_cast<uint32_t>(id[1]) << 8) |
        (static_cast<uint32_t>(id[2]) << 16) |
        (static_cast<uint32_t>(id[3]) << 24);
    Serial.printf("[raw SPI mode %u] %02X %02X %02X %02X -> 0x%08lX\n",
                  mode, id[0], id[1], id[2], id[3], rawId);
  }
}

void updateDisplay(uint32_t now) {
  if (!oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (!uwbReady) {
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.println("BU01");
    display.println("error");
    display.display();
    return;
  }
  display.setCursor(0, 0);
  display.println("ThrowTrack");

  if (hasRange && now - lastRangeMs <= RANGE_TIMEOUT_MS) {
    display.println("Distance");
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.print(latestRangeM, 2);
    display.println(" m");
  } else if (!hasRange && now <= RANGE_TIMEOUT_MS) {
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.println("Waiting...");
  } else {
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.println("No");
    display.println("responder");
  }

  display.display();
}
#endif
}  // namespace
