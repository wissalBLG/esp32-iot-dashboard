#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <RTClib.h>
#include <ArduinoJson.h>

// =====================================================
// 1. WIFI
// =====================================================

const char* WIFI_SSID     = "****";
const char* WIFI_PASSWORD = "*******";
// =====================================================
// 2. SUPABASE
// =====================================================
// Example:
// https://abcdefghijklmnop.supabase.co

const char* SUPABASE_URL = "https://ibzaduavwffveksmoxwm.supabase.co";

// This MUST be your publishable key.
// It starts with sb_publishable_...
const char* SUPABASE_KEY = "sb_publishable_t2oa7YCKVnvlI_cHmT2K3Q_ZHkCLbxY";


// =====================================================
// 3. ESP32-S3 PINS
// =====================================================

#define MOISTURE_PIN 4
#define PIR_PIN      5
#define BUZZER_PIN   6

#define I2C_SDA      8
#define I2C_SCL      9


// =====================================================
// 4. SETTINGS
// =====================================================

// Send sensor information every 30 seconds
const unsigned long SENSOR_INTERVAL = 30000;

// Check for dashboard commands every 10 seconds
const unsigned long COMMAND_INTERVAL = 10000;


// =====================================================
// 5. RTC
// =====================================================

RTC_DS3231 rtc;


// =====================================================
// 6. TIMERS
// =====================================================

unsigned long lastSensorSend = 0;
unsigned long lastCommandCheck = 0;


// =====================================================
// 7. WATER SENSOR CALIBRATION
// =====================================================

// IMPORTANT:
//
// These are starting values.
// We will calibrate them later using your real sensor.
//
// Put the sensor in the LOW-water condition and note
// the analog reading.
//
// Then put it in the HIGH-water condition and note
// the analog reading.
//
// We'll replace these numbers later.

const int WATER_RAW_LOW  = 1000;
const int WATER_RAW_HIGH = 3000;


// =====================================================
// 8. BUZZER
// =====================================================

void beep(int times, int durationMs)
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(durationMs);

    digitalWrite(BUZZER_PIN, LOW);
    delay(durationMs);
  }
}


// =====================================================
// 9. WIFI CONNECTION
// =====================================================

void connectWiFi()
{
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Wi-Fi connection FAILED.");
  }
}


// =====================================================
// 10. READ WATER SENSOR
// =====================================================

int readWaterRaw()
{
  int total = 0;

  // Take several readings to reduce noise
  for (int i = 0; i < 10; i++)
  {
    total += analogRead(MOISTURE_PIN);
    delay(5);
  }

  return total / 10;
}


// =====================================================
// 11. CONVERT SENSOR TO WATER %
// =====================================================

int calculateWaterPercent(int raw)
{
  int percent = map(
    raw,
    WATER_RAW_LOW,
    WATER_RAW_HIGH,
    0,
    100
  );

  percent = constrain(percent, 0, 100);

  return percent;
}


// =====================================================
// 12. GET RTC TIME
// =====================================================

String getDateTime()
{
  DateTime now = rtc.now();

  char buffer[25];

  sprintf(
    buffer,
    "%04d-%02d-%02d %02d:%02d:%02d",
    now.year(),
    now.month(),
    now.day(),
    now.hour(),
    now.minute(),
    now.second()
  );

  return String(buffer);
}


// =====================================================
// 13. SEND SENSOR READING TO SUPABASE
// =====================================================

void sendSensorReading()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    connectWiFi();

    if (WiFi.status() != WL_CONNECTED)
    {
      return;
    }
  }

  int rawWater = readWaterRaw();
  int waterPercent = calculateWaterPercent(rawWater);

  bool motion = digitalRead(PIR_PIN);

  Serial.println();
  Serial.println("----- SENSOR READING -----");

  Serial.print("Water raw: ");
  Serial.println(rawWater);

  Serial.print("Water %: ");
  Serial.println(waterPercent);

  Serial.print("Motion: ");
  Serial.println(motion ? "YES" : "NO");

  Serial.print("RTC: ");
  Serial.println(getDateTime());


  // ---------------------------------------------------
  // Supabase REST URL
  // ---------------------------------------------------

  String url = String(SUPABASE_URL) +
               "/rest/v1/sensor_readings";


  HTTPClient http;

  http.begin(url);

  // Current Supabase API key
  
http.addHeader("apikey", SUPABASE_KEY);
http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
http.addHeader("Content-Type", "application/json");
http.addHeader("Prefer", "return=minimal");


  // ---------------------------------------------------
  // JSON
  // ---------------------------------------------------

  String json = "{";

  json += "\"water_level\":";
  json += String(waterPercent);

  json += ",";
  json += "\"motion\":";
  json += motion ? "true" : "false";
json += ",";
json += "\"reading_time\":\"";
json += getDateTime();
json += "\"";
  json += "}";


  Serial.print("Sending: ");
  Serial.println(json);


  int httpCode = http.POST(json);


  Serial.print("Supabase HTTP status: ");
  Serial.println(httpCode);


  if (httpCode >= 200 && httpCode < 300)
  {
    Serial.println("SUCCESS: Sensor data uploaded!");
  }
  else
  {
    Serial.println("ERROR uploading sensor data.");

    String response = http.getString();

    Serial.println(response);
  }

  http.end();
}


// =====================================================
// 14. CHECK SUPABASE FOR COMMANDS
// =====================================================

void checkCommands()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  String url = String(SUPABASE_URL) +
               "/rest/v1/commands" +
               "?select=*" +
               "&executed=eq.false" +
               "&order=created_at.asc" +
               "&limit=1";


  HTTPClient http;

  http.begin(url);

  http.addHeader("apikey", SUPABASE_KEY);
http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
http.addHeader("Content-Type", "application/json");
http.addHeader("Prefer", "return=minimal");


  int httpCode = http.GET();


  Serial.print("Command check HTTP status: ");
  Serial.println(httpCode);


  if (httpCode != 200)
  {
    Serial.println("Could not check commands.");

    String response = http.getString();
    Serial.println(response);

    http.end();
    return;
  }


  String response = http.getString();

  Serial.print("Command response: ");
  Serial.println(response);


  // ---------------------------------------------------
  // Parse JSON
  // ---------------------------------------------------

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, response);

  if (error)
  {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());

    http.end();
    return;
  }


  // No commands
  if (doc.size() == 0)
  {
    http.end();
    return;
  }


  JsonObject commandObject = doc[0];

  int commandId = commandObject["id"] | 0;

  String command = commandObject["command"] | "";


  Serial.println();
  Serial.println("================================");
  Serial.println("NEW COMMAND RECEIVED");
  Serial.print("Command: ");
  Serial.println(command);
  Serial.print("ID: ");
  Serial.println(commandId);
  Serial.println("================================");


  http.end();


  // ---------------------------------------------------
  // FEED COMMAND
  // ---------------------------------------------------

  if (command == "feed")
  {
    Serial.println("Executing FEED command...");

    // For now the buzzer indicates the feed action.
    beep(3, 200);

    markCommandExecuted(commandId);
  }


  // ---------------------------------------------------
  // WATER REFILL COMMAND
  // ---------------------------------------------------

  else if (command == "refill_water")
  {
    Serial.println("Executing WATER REFILL command...");

    beep(2, 300);

    markCommandExecuted(commandId);
  }


  else
  {
    Serial.println("Unknown command.");

    markCommandExecuted(commandId);
  }
}


// =====================================================
// 15. MARK COMMAND EXECUTED
// =====================================================

void markCommandExecuted(int commandId)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }


  String url = String(SUPABASE_URL) +
               "/rest/v1/commands" +
               "?id=eq." +
               String(commandId);


  HTTPClient http;

  http.begin(url);

http.addHeader("apikey", SUPABASE_KEY);
http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
http.addHeader("Content-Type", "application/json");
http.addHeader("Prefer", "return=minimal");


  String json = "{";

  json += "\"executed\":true";

  json += "}";


  int httpCode = http.PATCH(json);


  Serial.print("Mark executed HTTP status: ");
  Serial.println(httpCode);


  if (httpCode >= 200 && httpCode < 300)
  {
    Serial.println("Command marked as executed.");
  }
  else
  {
    Serial.println("ERROR marking command executed.");

    String response = http.getString();
    Serial.println(response);
  }


  http.end();
}


// =====================================================
// 16. SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);


  Serial.println();
  Serial.println("======================================");
  Serial.println("       MEADOW WATCH ESP32-S3");
  Serial.println("======================================");


  // Pins
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);


  // Analog input
  analogReadResolution(12);


  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);


  // RTC
  Serial.println("Starting RTC...");

  if (!rtc.begin())
  {
    Serial.println("ERROR: DS3231 RTC not found!");
  }
  else
  {
    Serial.println("DS3231 RTC detected.");

    if (rtc.lostPower())
    {
      Serial.println("RTC lost power.");

      // Set RTC to the time the ESP32 was compiled.
      // This is only a starting point.
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

      Serial.println("RTC time initialized.");
    }

    Serial.print("RTC time: ");
    Serial.println(getDateTime());
  }


  // Wi-Fi
  connectWiFi();
  // Buzzer startup signal
  beep(1, 150);
Serial.println();
Serial.println("System ready.");
// Send first sensor reading immediately
sendSensorReading();
// Check for commands immediately
checkCommands();
// Reset timers
lastSensorSend = millis();
lastCommandCheck = millis();
}


// =====================================================
// 17. MAIN LOOP
// =====================================================

void loop()
{
  unsigned long currentMillis = millis();


  // ---------------------------------------------------
  // Send sensors every 30 seconds
  // ---------------------------------------------------

  if (currentMillis - lastSensorSend >= SENSOR_INTERVAL)
  {
    lastSensorSend = currentMillis;

    sendSensorReading();
  }


  // ---------------------------------------------------
  // Check commands every 10 seconds
  // ---------------------------------------------------

  if (currentMillis - lastCommandCheck >= COMMAND_INTERVAL)
  {
    lastCommandCheck = currentMillis;

    checkCommands();
  }


  // ---------------------------------------------------
  // Keep Wi-Fi alive
  // ---------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi lost.");

    connectWiFi();
  }


  delay(50);
}
