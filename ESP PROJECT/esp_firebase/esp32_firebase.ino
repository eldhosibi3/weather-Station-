#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

/* 1. Define your WiFi Credentials */
#define WIFI_SSID "KFON-SIBI"
#define WIFI_PASSWORD "kfon@23346"

/* 2. Define your Firebase API Key and Database URL */
// Go to Firebase Console -> Project Settings -> General -> Web API Key
#define API_KEY "AIzaSyAd6VVyvOS9XBhlxy3mc_0OO-hrODnVjn4"

// Go to Firebase Console -> Realtime Database -> Copy the URL
#define DATABASE_URL "https://roommonitor-69a5f-default-rtdb.asia-southeast1.firebasedatabase.app" 

/* 3. Define DHT Sensor Setup */
// For ESP32, GPIO 4 is a very common and safe pin for DHT sensors
#define DHTPIN 4       
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
// Timer to send data every 10 seconds
unsigned long timerDelay = 10000;

bool signupOK = false;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up anonymously to access rules where read/write = true */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Auth Successful");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Read sensor data
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print("°C | Humidity: ");
    Serial.print(h);
    Serial.println("%");

    // 1. Keep updating the single values for easy viewing in Firebase console
    Firebase.RTDB.setFloat(&fbdo, "sensor/temperature", t);
    Firebase.RTDB.setFloat(&fbdo, "sensor/humidity", h);
    Firebase.RTDB.setTimestamp(&fbdo, "sensor/last_updated");
    Firebase.RTDB.setInt(&fbdo, "sensor/uptime", millis() / 1000); // Uptime in seconds

    // 2. Push a new historical record to the 'sensor/history' list
    FirebaseJson json;
    json.set("temp", t);
    json.set("hum", h);
    json.set("timestamp/.sv", "timestamp"); // Firebase Server Timestamp
    
    if (Firebase.RTDB.pushJSON(&fbdo, "sensor/history", &json)) {
      Serial.println("PASSED - History record saved to Firebase");
    } else {
      Serial.println("FAILED - " + fbdo.errorReason());
    }
  }
}

