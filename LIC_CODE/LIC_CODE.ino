/*************************************************************
  Blynk is a platform with iOS and Android apps to control
  ESP32, Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build mobile and web interfaces for any
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: https://www.blynk.io
    Sketch generator:           https://examples.blynk.cc
    Blynk community:            https://community.blynk.cc
    Follow us:                  https://www.fb.com/blynkapp
                                https://twitter.com/blynk_app

  Blynk library is licensed under MIT license
 *************************************************************
  Blynk.Edgent implements:
  - Blynk.Inject - Dynamic WiFi credentials provisioning
  - Blynk.Air    - Over The Air firmware updates
  - Device state indication using a physical LED
  - Credentials reset using a physical Button
 *************************************************************/

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */
#define BLYNK_TEMPLATE_ID "TMPL3Z0uCYzau"
#define BLYNK_TEMPLATE_NAME "AEGIS GUARD"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
//#define USE_ESP32_DEV_MODULE
//#define USE_ESP32C3_DEV_MODULE
//#define USE_ESP32S2_DEV_KIT
//#define USE_WROVER_BOARD
//#define USE_TTGO_T7
//#define USE_TTGO_T_OI

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "BlynkEdgent.h"

// ---------------- LCD SETUP ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD I2C address 0x27

String message1 = "AEGIS GUARD";
String message2 = "SHIELD ENABLED!";
String preMessages[] = {"ANALOG EMERGENCY", "AND", "GAS INTEGRATED", "SAFETY GUARD"};
int numPreMessages = sizeof(preMessages)/sizeof(preMessages[0]);

// ---------------- SENSOR PINS ----------------
#define IR_PIN       2
#define FLAME_PIN    4
#define MQ2_PIN     34
#define LM35_PIN    35

// ---------------- THRESHOLDS ----------------
#define MQ2_THRESHOLD   2.5     // Volts
#define TEMP_THRESHOLD  50.0    // °C
#define ALERT_REPEAT_INTERVAL 120000UL  // 2 minutes

// ---------------- STATE VARIABLES ----------------
bool gasAlertSent = false;
bool tempAlertSent = false;
bool flameAlertSent = false;
bool intrusionAlertSent = false;

unsigned long gasLastAlert = 0;
unsigned long tempLastAlert = 0;
unsigned long flameLastAlert = 0;
unsigned long intrusionLastAlert = 0;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(IR_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);

  BlynkEdgent.begin();

  // Initialize LCD
  Wire.begin(21, 22);   // ESP32 default SDA=21, SCL=22
  lcd.init();
  lcd.backlight();
  lcd.clear();

  int lcdColumns = 16;
  int msgLength = message1.length();

  // --- Scroll first message ---
  for (int pos = lcdColumns; pos >= -msgLength; pos--) {
    lcd.clear();
    lcd.setCursor(pos >= 0 ? pos : 0, 0);
    lcd.print(message1);
    delay(200);
  }

  // Fix first message on top line
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message1);

  // --- Display pre-messages one by one on second line ---
  for (int i = 0; i < numPreMessages; i++) {
    lcd.setCursor(0, 1);
    lcd.print(preMessages[i]);
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print("                "); // clear line
    delay(200);
  }

  // --- Glitch effect for final message ---
  for (int i = 0; i < 6; i++) {
    lcd.setCursor(0, 1);
    if (i % 2 == 0) lcd.print(message2.substring(0,16));
    else lcd.print("                ");
    delay(150);
  }

  // Show final stable message
  lcd.setCursor(0, 1);
  lcd.print(message2.substring(0, 16));
}

// ---------------- MAIN LOOP ----------------
void loop() {
  BlynkEdgent.run();
  unsigned long currentMillis = millis();

  // ---- Read sensors ----
  int irVal = digitalRead(IR_PIN);
  int flameVal = digitalRead(FLAME_PIN);
  int mq2Raw = analogRead(MQ2_PIN);
  float mq2Voltage = mq2Raw * (3.3 / 4095.0); 
  int lm35Raw = analogRead(LM35_PIN);
  float lm35Voltage = lm35Raw * (3.3 / 4095.0); 
  float tempC = lm35Voltage * 100.0;

  // ---- Send values to Blynk ----
  Blynk.virtualWrite(V0, irVal);
  Blynk.virtualWrite(V1, flameVal);
  Blynk.virtualWrite(V2, mq2Voltage);
  Blynk.virtualWrite(V3, tempC);

  // ---- TRACK ALERT STATUS ----
  String lcdMessage = ""; // message to display on second line

  // GAS ALERT
  if (mq2Voltage > MQ2_THRESHOLD) {
    if (!gasAlertSent || currentMillis - gasLastAlert >= ALERT_REPEAT_INTERVAL) {
      Blynk.logEvent("gas_alert", "⚠️ Gas Detected! MQ2 > 2.5V");
      gasAlertSent = true;
      gasLastAlert = currentMillis;
    }
    lcdMessage = "Gas Detected!";
  } else if (gasAlertSent) {
    Blynk.logEvent("All_safe", "✅Everything back to normal.");
    gasAlertSent = false;
    lcdMessage = "Gas Safe";
  }

  // TEMPERATURE ALERT
  if (tempC > TEMP_THRESHOLD) {
    if (!tempAlertSent || currentMillis - tempLastAlert >= ALERT_REPEAT_INTERVAL) {
      Blynk.logEvent("temp_alert", "🔥 High Temperature Detected! > 50°C");
      tempAlertSent = true;
      tempLastAlert = currentMillis;
    }
    lcdMessage = "Temp High!";
  } else if (tempAlertSent) {
    Blynk.logEvent("All_safe", "✅Everything back to normal.");
    tempAlertSent = false;
    lcdMessage = "Temp Safe";
  }

  // FLAME ALERT
  if (flameVal == HIGH) {
    if (!flameAlertSent || currentMillis - flameLastAlert >= ALERT_REPEAT_INTERVAL) {
      Blynk.logEvent("flame_alert", "🔥 Flame Detected!");
      flameAlertSent = true;
      flameLastAlert = currentMillis;
    }
    lcdMessage = "Flame Detected!";
  } else if (flameAlertSent) {
    Blynk.logEvent("All_safe", "✅Everything back to normal.");
    flameAlertSent = false;
    lcdMessage = "Flame Safe";
  }

  // INTRUSION ALERT
  if (irVal == HIGH) {
    if (!intrusionAlertSent || currentMillis - intrusionLastAlert >= ALERT_REPEAT_INTERVAL) {
      Blynk.logEvent("intrusion_alert", "🚨 Intrusion Detected!");
      intrusionAlertSent = true;
      intrusionLastAlert = currentMillis;
    }
    lcdMessage = "Intrusion!";
  } else if (intrusionAlertSent) {
    Blynk.logEvent("All_safe", "✅Everything back to normal.");
    intrusionAlertSent = false;
    lcdMessage = "All Safe";
  }

  // ---- UPDATE LCD ----
  lcd.setCursor(0, 1);
  lcd.print("                "); // clear line
  lcd.setCursor(0, 1);
  lcd.print(lcdMessage.substring(0,16));

  delay(1000);
}

