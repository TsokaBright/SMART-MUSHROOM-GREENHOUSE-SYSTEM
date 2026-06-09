#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <DHT.h>

MCUFRIEND_kbv tft;

// ================= TOUCH PINS =================
// These values work for 2.8" TFT with resistive touch
const int XP = 6, XM = A2, YP = A1, YM = 7;
const int TS_LEFT = 907, TS_RT = 136, TS_TOP = 942, TS_BOT = 139;
#define MINPRESSURE 10
#define MAXPRESSURE 1000

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// ================= DHT SENSOR PINS =================
// Five sensors across 5 mushroom growing zones
#define DHTPIN_1 34
#define DHTPIN_2 35
#define DHTPIN_3 36
#define DHTPIN_4 37
#define DHTPIN_5 38

#define DHTTYPE_22 DHT22
#define DHTTYPE_11 DHT11

// Zone 1,3,4 use DHT22 (better accuracy), Zone 2 and 5 use DHT11
DHT dht1(DHTPIN_1, DHTTYPE_22);
DHT dht2(DHTPIN_2, DHTTYPE_11);
DHT dht3(DHTPIN_3, DHTTYPE_22);
DHT dht4(DHTPIN_4, DHTTYPE_22);
DHT dht5(DHTPIN_5, DHTTYPE_11);

// ================= SOIL MOISTURE PINS =================
// Analog inputs for moisture sensors in each zone
#define MOISTURE_PIN_1 A10
#define MOISTURE_PIN_2 A11
#define MOISTURE_PIN_3 A12
#define MOISTURE_PIN_4 A13
#define MOISTURE_PIN_5 A14

// ================= CO2 SENSOR PIN =================
// Single CO2 sensor for the whole greenhouse
#define CO2_PIN A15

// ================= RELAY PINS =================
// Digital outputs for controlling equipment
#define RELAY_HEATER       22
#define RELAY_PELTIER      23    // Cooling element
#define RELAY_TEMP_FAN     24    // Fan for temperature distribution
#define RELAY_VENT_FAN     25    // Ventilation fan for CO2 control
#define RELAY_HUMIDIFIER_1 26
#define RELAY_HUMIDIFIER_2 27
#define RELAY_HUMIDIFIER_3 28
#define RELAY_HUMIDIFIER_4 29
#define RELAY_HUMIDIFIER_5 30

// Array for easier loop access to humidifier relays
const int humidifierPins[5] = {RELAY_HUMIDIFIER_1, RELAY_HUMIDIFIER_2, RELAY_HUMIDIFIER_3, RELAY_HUMIDIFIER_4, RELAY_HUMIDIFIER_5};

// ================= COMMUNICATION WITH ESP32 =================
// Serial1 is used to send data to ESP32 for cloud monitoring
HardwareSerial &esp32 = Serial1;

// ================= BUTTON OBJECTS =================
// All touch buttons used throughout the interface
Adafruit_GFX_Button btnButton;
Adafruit_GFX_Button btnOyster;
Adafruit_GFX_Button btnCustom;
Adafruit_GFX_Button btnButtonSpawn;
Adafruit_GFX_Button btnButtonFruiting;
Adafruit_GFX_Button btnButtonBack;
Adafruit_GFX_Button btnButtonStop;
Adafruit_GFX_Button btnOysterGrowing;
Adafruit_GFX_Button btnOysterFruiting;
Adafruit_GFX_Button btnOysterBack;
Adafruit_GFX_Button btnOysterStop;
Adafruit_GFX_Button btnReadingsBack;
Adafruit_GFX_Button btnReadingsStop;
Adafruit_GFX_Button btnCustomStart;
Adafruit_GFX_Button btnCustomBack;
Adafruit_GFX_Button btnCustomStop;
Adafruit_GFX_Button btnNum[10];
Adafruit_GFX_Button btnNumEnter;
Adafruit_GFX_Button btnNumDelete;
Adafruit_GFX_Button btnCustomNumBack;

// Touch coordinates storage
int pixel_x, pixel_y;

// Color definitions for TFT display
#define BLACK   0x0000
#define NAVY    0x000F
#define BLUE    0x001F
#define GREEN   0x07E0
#define CYAN    0x07FF
#define RED     0xF800
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFD20
#define LIME    0x87E0
#define DARKCYAN 0x03EF
#define GRAY    0x8410
#define TEAL    0x0410
#define PURPLE  0x780F

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Screen state enumeration for navigation
enum ScreenState {
  MAIN_MENU,
  BUTTON_MENU,
  OYSTER_MENU,
  LIVE_READINGS,
  CUSTOM_MENU,
  CUSTOM_TEMP,
  CUSTOM_CO2,
  CUSTOM_MOISTURE,
  CUSTOM_HUMIDITY
};

// Track current screen and previous screen for back navigation
ScreenState currentScreen = MAIN_MENU;
ScreenState previousScreen = MAIN_MENU;

// System control parameters
float tMin = 22, tMax = 27;                    // Temperature range
float hMin[5] = {90, 90, 90, 90, 90};         // Humidity minimum per zone
float hMax[5] = {95, 95, 95, 95, 95};         // Humidity maximum per zone
float co2Min = 5000, co2Max = 10000;          // CO2 range (ppm)
float mMin[5] = {65, 65, 65, 65, 65};         // Soil moisture minimum per zone
float mMax[5] = {70, 70, 70, 70, 70};         // Soil moisture maximum per zone

// Current sensor readings
float currentTemp[5] = {0, 0, 0, 0, 0};
float currentHum[5] = {0, 0, 0, 0, 0};
float currentMoisture[5] = {0, 0, 0, 0, 0};
bool humidifierState[5] = {false, false, false, false, false};

// System status flags
bool systemArmed = false;                      // System active/inactive
unsigned long lastSensorRead = 0;              // Last sensor read timestamp
unsigned long lastESP32Send = 0;               // Last data send timestamp
String currentMode = "IDLE";                  // Current operation mode

// Custom configuration storage
int customTempMin = 0, customTempMax = 0;
int customCo2Min = 0, customCo2Max = 0;
int customMoistureMin[5] = {0,0,0,0,0};
int customMoistureMax[5] = {0,0,0,0,0};
int customHumidityMin[5] = {0,0,0,0,0};
int customHumidityMax[5] = {0,0,0,0,0};
int currentSensorIndex = 0;
String inputBuffer = "";
int inputStage = 0;

// Function prototypes
bool getTouch(void);
void waitForTouchRelease(void);
void fullScreenClear(void);
void drawTitleBar(String title);
void goToMainMenu();
void goToButtonMenu();
void goToOysterMenu();
void goToLiveReadings();
void goToCustomMenu();
void goToPreviousScreen();
void goToCustomTemp();
void goToCustomCo2();
void goToCustomMoisture();
void goToCustomHumidity();
void drawMainMenu();
void drawButtonMenu();
void drawOysterMenu();
void drawLiveReadings();
void drawCustomMenu();
void drawNumericKeypad(String title, String prompt);
void updateKeypadValueDisplay();
void updateLiveReadingsDisplay();
void readAllSensors();
void controlTemperature();
void controlCO2();
void controlHumidifiers();
void turnAllOff();
void sendToESP32(String cmd);
void sendDataToESP32();
void activateSystem(String cmd, float tMinVal, float tMaxVal, float co2MinVal, float co2MaxVal, float mVal, float hMinVal, float hMaxVal, String modeName);
void resetSystem();
void processCustomInput();

// ================= SCREEN FUNCTIONS =================
// Clears entire display before drawing new screen
void fullScreenClear() {
  tft.fillScreen(BLACK);
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
  delay(30);
}

// Draws colored title bar at top of each screen
void drawTitleBar(String title) {
  tft.fillRect(0, 0, SCREEN_WIDTH, 28, NAVY);
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor((SCREEN_WIDTH - (title.length() * 6)) / 2, 10);
  tft.print(title);
  tft.drawFastHLine(0, 28, SCREEN_WIDTH, CYAN);
}

// Navigation functions - each clears screen, stores previous screen, draws new content
void goToMainMenu() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = MAIN_MENU;
  drawMainMenu();
}

void goToButtonMenu() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = BUTTON_MENU;
  drawButtonMenu();
}

void goToOysterMenu() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = OYSTER_MENU;
  drawOysterMenu();
}

void goToLiveReadings() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = LIVE_READINGS;
  drawLiveReadings();
}

void goToCustomMenu() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = CUSTOM_MENU;
  drawCustomMenu();
}

// Returns to previous screen using stored state
void goToPreviousScreen() {
  waitForTouchRelease();
  fullScreenClear();
  ScreenState targetScreen = previousScreen;
  previousScreen = currentScreen;
  currentScreen = targetScreen;
  
  switch (currentScreen) {
    case MAIN_MENU: drawMainMenu(); break;
    case BUTTON_MENU: drawButtonMenu(); break;
    case OYSTER_MENU: drawOysterMenu(); break;
    case CUSTOM_MENU: drawCustomMenu(); break;
    case LIVE_READINGS: drawLiveReadings(); break;
    default: drawMainMenu(); currentScreen = MAIN_MENU; break;
  }
}

// Custom setup navigation functions
void goToCustomTemp() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = CUSTOM_TEMP;
  inputBuffer = "";
  inputStage = 0;
  currentSensorIndex = 0;
  drawNumericKeypad("SET TEMP", "Min Temp (C):");
}

void goToCustomCo2() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = CUSTOM_CO2;
  inputBuffer = "";
  inputStage = 0;
  drawNumericKeypad("SET CO2", "Min CO2 (ppm):");
}

void goToCustomMoisture() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = CUSTOM_MOISTURE;
  inputBuffer = "";
  inputStage = 0;
  drawNumericKeypad("SET SOIL", "S" + String(currentSensorIndex + 1) + " Min (%):");
}

void goToCustomHumidity() {
  waitForTouchRelease();
  fullScreenClear();
  previousScreen = currentScreen;
  currentScreen = CUSTOM_HUMIDITY;
  inputBuffer = "";
  inputStage = 0;
  drawNumericKeypad("SET HUMIDITY", "Z" + String(currentSensorIndex + 1) + " Max (%):");
}

// ================= TOUCH FUNCTIONS =================
// Reads touch screen and converts to pixel coordinates
bool getTouch(void) {
  TSPoint p = ts.getPoint();
  pinMode(YP, OUTPUT);
  pinMode(XM, OUTPUT);
  digitalWrite(YP, HIGH);
  digitalWrite(XM, HIGH);
  
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    pixel_x = map(p.x, TS_LEFT, TS_RT, 0, tft.width());
    pixel_y = map(p.y, TS_TOP, TS_BOT, 0, tft.height());
    return true;
  }
  return false;
}

// Waits for user to lift finger from screen
void waitForTouchRelease(void) {
  while (getTouch()) delay(10);
  delay(100);
}

// ================= RELAY CONTROL =================
// Sets relay state (LOW = off, HIGH = on for most modules)
void setRelay(int pin, bool turnOn) {
  digitalWrite(pin, turnOn ? HIGH : LOW);
}

// Emergency stop - turns everything off
void turnAllOff() {
  setRelay(RELAY_TEMP_FAN, false);
  setRelay(RELAY_VENT_FAN, false);
  setRelay(RELAY_HEATER, false);
  setRelay(RELAY_PELTIER, false);
  for (int i = 0; i < 5; i++) {
    setRelay(humidifierPins[i], false);
    humidifierState[i] = false;
  }
}

// ================= READ SENSORS =================
// Reads all DHT, moisture, and CO2 sensors
void readAllSensors() {
  currentTemp[0] = dht1.readTemperature();
  currentHum[0] = dht1.readHumidity();
  currentTemp[1] = dht2.readTemperature();
  currentHum[1] = dht2.readHumidity();
  currentTemp[2] = dht3.readTemperature();
  currentHum[2] = dht3.readHumidity();
  currentTemp[3] = dht4.readTemperature();
  currentHum[3] = dht4.readHumidity();
  currentTemp[4] = dht5.readTemperature();
  currentHum[4] = dht5.readHumidity();
  
  // Handle failed reads (return 0 instead of NaN)
  for (int i = 0; i < 5; i++) {
    if (isnan(currentTemp[i])) currentTemp[i] = 0;
    if (isnan(currentHum[i])) currentHum[i] = 0;
  }
  
  // Convert analog readings (0-1023) to percentage (0-100)
  currentMoisture[0] = constrain(map(analogRead(MOISTURE_PIN_1), 0, 1023, 100, 0), 0, 100);
  currentMoisture[1] = constrain(map(analogRead(MOISTURE_PIN_2), 0, 1023, 100, 0), 0, 100);
  currentMoisture[2] = constrain(map(analogRead(MOISTURE_PIN_3), 0, 1023, 100, 0), 0, 100);
  currentMoisture[3] = constrain(map(analogRead(MOISTURE_PIN_4), 0, 1023, 100, 0), 0, 100);
  currentMoisture[4] = constrain(map(analogRead(MOISTURE_PIN_5), 0, 1023, 100, 0), 0, 100);
}

// ================= CONTROL FUNCTIONS =================
// Controls heater, cooler, and circulation fan based on average temperature
void controlTemperature() {
  if (!systemArmed) return;
  
  float avgTemp = 0;
  int count = 0;
  for (int i = 0; i < 5; i++) {
    if (currentTemp[i] > 0) {
      avgTemp += currentTemp[i];
      count++;
    }
  }
  if (count > 0) {
    avgTemp /= count;
    if (avgTemp < tMin) {
      setRelay(RELAY_HEATER, true);
      setRelay(RELAY_PELTIER, false);
      setRelay(RELAY_TEMP_FAN, true);
    } else if (avgTemp > tMax) {
      setRelay(RELAY_HEATER, false);
      setRelay(RELAY_PELTIER, true);
      setRelay(RELAY_TEMP_FAN, true);
    } else {
      setRelay(RELAY_HEATER, false);
      setRelay(RELAY_PELTIER, false);
      setRelay(RELAY_TEMP_FAN, false);
    }
  }
}

// Turns on ventilation fan when CO2 exceeds maximum
void controlCO2() {
  if (!systemArmed) return;
  int co2 = map(analogRead(CO2_PIN), 0, 1023, 0, 5000);
  setRelay(RELAY_VENT_FAN, co2 > co2Max);
}

// Controls individual zone humidifiers based on humidity readings
void controlHumidifiers() {
  if (!systemArmed) return;
  for (int i = 0; i < 5; i++) {
    if (currentHum[i] > 0) {
      humidifierState[i] = (currentHum[i] < hMin[i]);
      setRelay(humidifierPins[i], humidifierState[i]);
    }
  }
}

// ================= ESP32 COMMUNICATION =================
// Sends command to ESP32 for cloud logging
void sendToESP32(String cmd) {
  esp32.println(cmd);
  Serial.println("Sent to ESP32: " + cmd);
  delay(10);
}

// Sends all sensor data to ESP32
void sendDataToESP32() {
  if (!systemArmed) return;
  
  int co2 = map(analogRead(CO2_PIN), 0, 1023, 0, 5000);
  
  String data = String(currentTemp[0], 1) + "," +
                String(currentTemp[1], 1) + "," +
                String(currentTemp[2], 1) + "," +
                String(currentTemp[3], 1) + "," +
                String(currentTemp[4], 1) + "," +
                String(currentHum[0], 0) + "," +
                String(currentHum[1], 0) + "," +
                String(currentHum[2], 0) + "," +
                String(currentHum[3], 0) + "," +
                String(currentHum[4], 0) + "," +
                String(co2) + "," +
                String(currentMoisture[0], 0) + "," +
                String(currentMoisture[1], 0) + "," +
                String(currentMoisture[2], 0) + "," +
                String(currentMoisture[3], 0) + "," +
                String(currentMoisture[4], 0);
  
  data.replace(" ", "");
  
  esp32.println("DATA:" + data);
  Serial.println("Data sent to ESP32");
  delay(10);
}

// Activates system with specified parameters
void activateSystem(String cmd, float tMinVal, float tMaxVal, float co2MinVal, float co2MaxVal, float mVal, float hMinVal, float hMaxVal, String modeName) {
  sendToESP32("MODE:" + cmd);
  currentMode = modeName;
  tMin = tMinVal;
  tMax = tMaxVal;
  co2Min = co2MinVal;
  co2Max = co2MaxVal;
  for (int i = 0; i < 5; i++) {
    mMin[i] = mVal;
    mMax[i] = mVal + 5;
    hMin[i] = hMinVal;
    hMax[i] = hMaxVal;
  }
  systemArmed = true;
  Serial.println("System ACTIVE - " + modeName);
}

// Stops all operations and returns to main menu
void resetSystem() {
  turnAllOff();
  systemArmed = false;
  currentMode = "IDLE";
  sendToESP32("STOP");
  sendToESP32("RESET");
  goToMainMenu();
  Serial.println("System STOPPED");
}

// ================= DRAW SCREENS =================
// Main menu with three options
void drawMainMenu() {
  tft.fillScreen(TEAL);
  drawTitleBar("SMART MUSHROOM GREENHOUSE SYSTEM");
  
  btnButton.initButton(&tft, 160, 80, 200, 45, WHITE, BLUE, WHITE, "BUTTON", 2);
  btnOyster.initButton(&tft, 160, 135, 200, 45, WHITE, GREEN, BLACK, "OYSTER", 2);
  btnCustom.initButton(&tft, 160, 190, 200, 45, WHITE, ORANGE, BLACK, "CUSTOM", 2);
  
  btnButton.drawButton(false);
  btnOyster.drawButton(false);
  btnCustom.drawButton(false);
}

// Button mushroom preset menu
void drawButtonMenu() {
  tft.fillScreen(BLUE);
  drawTitleBar("BUTTON MUSHROOM");
  
  btnButtonSpawn.initButton(&tft, 160, 70, 200, 40, WHITE, GREEN, BLACK, "SPAWN", 2);
  btnButtonFruiting.initButton(&tft, 160, 120, 200, 40, WHITE, YELLOW, BLACK, "FRUITING", 2);
  btnButtonStop.initButton(&tft, 90, 200, 100, 35, WHITE, ORANGE, BLACK, "STOP", 2);
  btnButtonBack.initButton(&tft, 230, 200, 100, 35, WHITE, RED, WHITE, "BACK", 2);
  
  btnButtonSpawn.drawButton(false);
  btnButtonFruiting.drawButton(false);
  btnButtonStop.drawButton(false);
  btnButtonBack.drawButton(false);
}

// Oyster mushroom preset menu
void drawOysterMenu() {
  tft.fillScreen(DARKCYAN);
  drawTitleBar("OYSTER MUSHROOM");
  
  btnOysterGrowing.initButton(&tft, 160, 70, 200, 40, WHITE, GREEN, BLACK, "GROWING", 2);
  btnOysterFruiting.initButton(&tft, 160, 120, 200, 40, WHITE, YELLOW, BLACK, "FRUITING", 2);
  btnOysterStop.initButton(&tft, 90, 200, 100, 35, WHITE, ORANGE, BLACK, "STOP", 2);
  btnOysterBack.initButton(&tft, 230, 200, 100, 35, WHITE, RED, WHITE, "BACK", 2);
  
  btnOysterGrowing.drawButton(false);
  btnOysterFruiting.drawButton(false);
  btnOysterStop.drawButton(false);
  btnOysterBack.drawButton(false);
}

// Live sensor readings display
void drawLiveReadings() {
  tft.fillScreen(BLACK);
  drawTitleBar("LIVE READINGS");
  
  tft.drawRoundRect(10, 38, 300, 125, 5, CYAN);
  tft.setTextSize(1);
  
  // Zone 1-5 temperature and humidity
  tft.setTextColor(CYAN);
  tft.setCursor(20, 52); tft.print("Z1(DHT22):");
  tft.setCursor(180, 52); tft.setTextColor(WHITE); tft.print("--C --%");
  
  tft.setTextColor(CYAN);
  tft.setCursor(20, 68); tft.print("Z2(DHT11):");
  tft.setCursor(180, 68); tft.setTextColor(WHITE); tft.print("--C --%");
  
  tft.setTextColor(CYAN);
  tft.setCursor(20, 84); tft.print("Z3(DHT22):");
  tft.setCursor(180, 84); tft.setTextColor(WHITE); tft.print("--C --%");
  
  tft.setTextColor(CYAN);
  tft.setCursor(20, 100); tft.print("Z4(DHT22):");
  tft.setCursor(180, 100); tft.setTextColor(WHITE); tft.print("--C --%");
  
  tft.setTextColor(CYAN);
  tft.setCursor(20, 116); tft.print("Z5(DHT11):");
  tft.setCursor(180, 116); tft.setTextColor(WHITE); tft.print("--C --%");
  
  // CO2 reading
  tft.setTextColor(YELLOW);
  tft.setCursor(20, 136); tft.print("CO2:");
  tft.setCursor(80, 136); tft.setTextColor(WHITE); tft.print("--- ppm");
  
  // Soil moisture readings for all zones
  tft.setTextColor(LIME);
  tft.setCursor(20, 152); tft.print("SOIL:");
  tft.setCursor(80, 152); tft.setTextColor(WHITE);
  tft.print("1:--% 2:--% 3:--% 4:--% 5:--%");
  
  // Control buttons
  btnReadingsStop.initButton(&tft, 90, 200, 100, 35, WHITE, ORANGE, BLACK, "STOP", 2);
  btnReadingsBack.initButton(&tft, 230, 200, 100, 35, WHITE, RED, WHITE, "BACK", 2);
  btnReadingsStop.drawButton(false);
  btnReadingsBack.drawButton(false);
}

// Custom configuration menu
void drawCustomMenu() {
  tft.fillScreen(PURPLE);
  drawTitleBar("CUSTOM SETUP");
  
  tft.fillRoundRect(20, 42, 280, 100, 8, NAVY);
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(35, 60); tft.print("1. TEMPERATURE RANGE");
  tft.setCursor(35, 82); tft.print("2. CO2 RANGE");
  tft.setCursor(35, 104); tft.print("3. SOIL MOISTURE");
  tft.setCursor(35, 126); tft.print("4. ZONE HUMIDITY");
  
  btnCustomStart.initButton(&tft, 160, 165, 140, 38, WHITE, GREEN, BLACK, "START", 2);
  btnCustomBack.initButton(&tft, 85, 215, 100, 35, WHITE, RED, WHITE, "BACK", 2);
  btnCustomStop.initButton(&tft, 230, 215, 100, 35, WHITE, ORANGE, BLACK, "STOP", 2);
  
  btnCustomStart.drawButton(false);
  btnCustomBack.drawButton(false);
  btnCustomStop.drawButton(false);
}

// Numeric keypad for data entry - 3x4 grid layout
void drawNumericKeypad(String title, String prompt) {
  tft.fillScreen(BLACK);
  drawTitleBar(title);
  
  // Prompt text area
  tft.fillRoundRect(10, 38, 300, 30, 5, GRAY);
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(15, 50);
  tft.print(prompt);
  
  // Current value display
  tft.fillRoundRect(10, 76, 300, 40, 5, NAVY);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 92);
  tft.print("Value: ");
  tft.print(inputBuffer);
  
  // Button dimensions and positions
  int btnW = 70, btnH = 40;
  int startX = 25, startY = 130;
  
  // Row 1: 1 2 3
  btnNum[1].initButton(&tft, startX, startY, btnW, btnH, WHITE, BLUE, WHITE, "1", 2);
  btnNum[2].initButton(&tft, startX + 85, startY, btnW, btnH, WHITE, BLUE, WHITE, "2", 2);
  btnNum[3].initButton(&tft, startX + 170, startY, btnW, btnH, WHITE, BLUE, WHITE, "3", 2);
  
  // Row 2: 4 5 6
  btnNum[4].initButton(&tft, startX, startY + 55, btnW, btnH, WHITE, BLUE, WHITE, "4", 2);
  btnNum[5].initButton(&tft, startX + 85, startY + 55, btnW, btnH, WHITE, BLUE, WHITE, "5", 2);
  btnNum[6].initButton(&tft, startX + 170, startY + 55, btnW, btnH, WHITE, BLUE, WHITE, "6", 2);
  
  // Row 3: 7 8 9
  btnNum[7].initButton(&tft, startX, startY + 110, btnW, btnH, WHITE, BLUE, WHITE, "7", 2);
  btnNum[8].initButton(&tft, startX + 85, startY + 110, btnW, btnH, WHITE, BLUE, WHITE, "8", 2);
  btnNum[9].initButton(&tft, startX + 170, startY + 110, btnW, btnH, WHITE, BLUE, WHITE, "9", 2);
  
  // Row 4: DEL 0 OK
  btnNumDelete.initButton(&tft, startX, startY + 165, 70, 40, WHITE, RED, WHITE, "DEL", 1);
  btnNum[0].initButton(&tft, startX + 85, startY + 165, 70, 40, WHITE, BLUE, WHITE, "0", 2);
  btnNumEnter.initButton(&tft, startX + 170, startY + 165, 70, 40, WHITE, GREEN, BLACK, "OK", 1);
  
  // BACK button
  btnCustomNumBack.initButton(&tft, 30, 215, 80, 30, WHITE, ORANGE, BLACK, "BACK", 2);
  
  // Draw all buttons
  for(int i = 0; i < 10; i++) btnNum[i].drawButton(false);
  btnNumDelete.drawButton(false);
  btnNumEnter.drawButton(false);
  btnCustomNumBack.drawButton(false);
}

// Updates the value display on keypad screen
void updateKeypadValueDisplay() {
  tft.fillRect(120, 86, 180, 20, BLACK);
  tft.setCursor(120, 86);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print(inputBuffer);
}

// Updates live readings display with current sensor values
void updateLiveReadingsDisplay() {
  if (currentScreen != LIVE_READINGS) return;
  
  int co2 = map(analogRead(CO2_PIN), 0, 1023, 0, 5000);
  tft.setTextSize(1);
  
  // Update each zone's temperature and humidity
  for (int i = 0; i < 5; i++) {
    tft.fillRect(180, 50 + (i * 16), 100, 12, BLACK);
    tft.setCursor(180, 52 + (i * 16));
    tft.setTextColor(WHITE);
    if (currentTemp[i] > 0) {
      tft.print(currentTemp[i], 1);
      tft.print("C ");
      tft.print(currentHum[i], 0);
      tft.print("%");
    } else {
      tft.print("--C --%");
    }
  }
  
  // Update CO2 reading
  tft.fillRect(80, 134, 100, 12, BLACK);
  tft.setCursor(80, 136);
  tft.setTextColor(WHITE);
  tft.print(co2);
  tft.print(" ppm");
  
  // Update soil moisture readings
  tft.fillRect(65, 150, 240, 12, BLACK);
  tft.setCursor(65, 152);
  tft.setTextColor(WHITE);
  tft.print(String(currentMoisture[0], 0) + "% ");
  tft.print(String(currentMoisture[1], 0) + "% ");
  tft.print(String(currentMoisture[2], 0) + "% ");
  tft.print(String(currentMoisture[3], 0) + "% ");
  tft.print(String(currentMoisture[4], 0) + "%");
}

// Handles sequential custom configuration input
void processCustomInput() {
  int value = inputBuffer.toInt();
  inputBuffer = "";
  
  if (currentScreen == CUSTOM_TEMP) {
    if (inputStage == 0) {
      customTempMin = value;
      inputStage = 1;
      drawNumericKeypad("SET TEMP", "Max Temp (C):");
    } else {
      customTempMax = value;
      goToCustomCo2();
    }
  }
  else if (currentScreen == CUSTOM_CO2) {
    if (inputStage == 0) {
      customCo2Min = value;
      inputStage = 1;
      drawNumericKeypad("SET CO2", "Max CO2 (ppm):");
    } else {
      customCo2Max = value;
      currentSensorIndex = 0;
      goToCustomMoisture();
    }
  }
  else if (currentScreen == CUSTOM_MOISTURE) {
    if (inputStage == 0) {
      customMoistureMin[currentSensorIndex] = value;
      inputStage = 1;
      drawNumericKeypad("SET SOIL", "S" + String(currentSensorIndex + 1) + " Max (%):");
    } else {
      customMoistureMax[currentSensorIndex] = value;
      inputStage = 0;
      currentSensorIndex++;
      if (currentSensorIndex < 5) {
        goToCustomMoisture();
      } else {
        currentSensorIndex = 0;
        goToCustomHumidity();
      }
    }
  }
  else if (currentScreen == CUSTOM_HUMIDITY) {
    if (inputStage == 0) {
      customHumidityMin[currentSensorIndex] = value;
      inputStage = 1;
      drawNumericKeypad("SET HUMIDITY", "Z" + String(currentSensorIndex + 1) + " Max (%):");
    } else {
      customHumidityMax[currentSensorIndex] = value;
      inputStage = 0;
      currentSensorIndex++;
      if (currentSensorIndex < 5) {
        goToCustomHumidity();
      } else {
        // Apply all custom settings and activate system
        tMin = customTempMin;
        tMax = customTempMax;
        co2Min = customCo2Min;
        co2Max = customCo2Max;
        for (int i = 0; i < 5; i++) {
          mMin[i] = customMoistureMin[i];
          mMax[i] = customMoistureMax[i];
          hMin[i] = customHumidityMin[i];
          hMax[i] = customHumidityMax[i];
        }
        systemArmed = true;
        currentMode = "CUSTOM";
        sendToESP32("MODE:CUSTOM");
        goToLiveReadings();
      }
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  esp32.begin(9600);
  
  Serial.println("\n==========================================");
  Serial.println("SMART MUSHROOM GREENHOUSE SYSTEM - MEGA");
  Serial.println("==========================================");
  
  // Initialize all DHT sensors
  dht1.begin();
  dht2.begin();
  dht3.begin();
  dht4.begin();
  dht5.begin();
  
  // Configure relay pins as outputs
  pinMode(RELAY_TEMP_FAN, OUTPUT);
  pinMode(RELAY_VENT_FAN, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_PELTIER, OUTPUT);
  for (int i = 0; i < 5; i++) pinMode(humidifierPins[i], OUTPUT);
  
  // Ensure all relays start in OFF state
  turnAllOff();
  
  // Initialize TFT display
  uint16_t ID = tft.readID();
  if (ID == 0xD3D3) ID = 0x9486;
  tft.begin(ID);
  tft.setRotation(1);
  
  // Show main menu on startup
  fullScreenClear();
  drawMainMenu();
  
  Serial.println("System Ready! Waiting for input...");
}

// ================= MAIN LOOP =================
void loop() {
  // Handle system control when armed
  if (systemArmed) {
    unsigned long now = millis();
    
    // Read sensors and control equipment every 10 seconds
    if (now - lastSensorRead >= 10000) {
      lastSensorRead = now;
      readAllSensors();
      controlTemperature();
      controlCO2();
      controlHumidifiers();
      
      // Update display if on live readings screen
      if (currentScreen == LIVE_READINGS) {
        updateLiveReadingsDisplay();
      }
    }
    
    // Send data to ESP32 every 10 seconds
    if (now - lastESP32Send >= 10000) {
      lastESP32Send = now;
      sendDataToESP32();
    }
  }
  
  // Wait for touch input
  if (!getTouch()) {
    delay(50);
    return;
  }
  
  // Handle touch based on current screen
  switch (currentScreen) {
    case MAIN_MENU:
      if (btnButton.contains(pixel_x, pixel_y)) goToButtonMenu();
      else if (btnOyster.contains(pixel_x, pixel_y)) goToOysterMenu();
      else if (btnCustom.contains(pixel_x, pixel_y)) goToCustomMenu();
      break;
      
    case BUTTON_MENU:
      if (btnButtonSpawn.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        activateSystem("BUTTON_SPAWN", 22, 27, 5000, 10000, 65, 90, 95, "BUTTON_SPAWN");
        goToLiveReadings();
      }
      else if (btnButtonFruiting.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        activateSystem("BUTTON_FRUITING", 14, 18, 500, 800, 65, 85, 95, "BUTTON_FRUITING");
        goToLiveReadings();
      }
      else if (btnButtonStop.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        resetSystem();
      }
      else if (btnButtonBack.contains(pixel_x, pixel_y)) goToPreviousScreen();
      break;
      
    case OYSTER_MENU:
      if (btnOysterGrowing.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        activateSystem("OYSTER_GROWING", 20, 30, 5000, 20000, 65, 70, 80, "OYSTER_GROWING");
        goToLiveReadings();
      }
      else if (btnOysterFruiting.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        activateSystem("OYSTER_FRUITING", 18, 28, 500, 800, 60, 80, 95, "OYSTER_FRUITING");
        goToLiveReadings();
      }
      else if (btnOysterStop.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        resetSystem();
      }
      else if (btnOysterBack.contains(pixel_x, pixel_y)) goToPreviousScreen();
      break;
      
    case LIVE_READINGS:
      if (btnReadingsStop.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        resetSystem();
      }
      else if (btnReadingsBack.contains(pixel_x, pixel_y)) goToPreviousScreen();
      break;
      
    case CUSTOM_MENU:
      if (btnCustomStart.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        goToCustomTemp();
      }
      else if (btnCustomBack.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        goToPreviousScreen();
      }
      else if (btnCustomStop.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        resetSystem();
      }
      break;
      
    case CUSTOM_TEMP:
    case CUSTOM_CO2:
    case CUSTOM_MOISTURE:
    case CUSTOM_HUMIDITY:
      // Handle BACK button on keypad
      if(btnCustomNumBack.contains(pixel_x, pixel_y)) {
        waitForTouchRelease();
        goToCustomMenu();
        break;
      }
      
      // Handle number buttons 0-9
      for(int i = 0; i < 10; i++) {
        if(btnNum[i].contains(pixel_x, pixel_y)) {
          waitForTouchRelease();
          inputBuffer += String(i);
          updateKeypadValueDisplay();
          break;
        }
      }
      
      // Handle DELETE button
      if(btnNumDelete.contains(pixel_x, pixel_y) && inputBuffer.length() > 0) {
        waitForTouchRelease();
        inputBuffer.remove(inputBuffer.length() - 1);
        updateKeypadValueDisplay();
      }
      // Handle ENTER/OK button
      else if(btnNumEnter.contains(pixel_x, pixel_y) && inputBuffer.length() > 0) {
        waitForTouchRelease();
        processCustomInput();
      }
      break;
  }
  
  delay(50);
}