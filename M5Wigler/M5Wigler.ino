#include <SPI.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <M5Stack.h>
#include "WiFi.h"


#define ARDUINO_USD_CS 15
#define LOG_FILE_PREFIX "gpslog"
#define MAX_LOG_FILES 100
#define LOG_FILE_SUFFIX "csv"
#define LOG_COLUMN_COUNT 11
#define LOG_RATE 500

char logFileName[13];
int totalNetworks = 0;
unsigned long lastLog = 0;

const String wigleHeaderFileFormat = "WigleWifi-1.4,appRelease=2.26,model=Feather,release=0.0.0,device=arduinoWardriving,display=3fea5e7,board=esp8266,brand=Adafruit";
char * log_col_names[LOG_COLUMN_COUNT] = {
  "MAC", "SSID", "AuthMode", "FirstSeen", "Channel", "RSSI", "Latitude", "Longitude", "AltitudeMeters", "AccuracyMeters", "Type"
};


TinyGPSPlus tinyGPS;
HardwareSerial ss(2);

void setup() {
  M5.begin();
  Serial.begin(115200);
  ss.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (!SD.begin(ARDUINO_USD_CS)) {
    M5.Lcd.setTextSize(3);
    M5.Lcd.println("SD card error");
    M5.Lcd.println("Error initializing SD card.");
    while(true)
      delay(100);
  }
  M5.Lcd.setTextSize(3);
  M5.Lcd.println("SD card OK!");
  delay(500);
  updateFileName();
  printHeader();
}

void loop() {
      if (tinyGPS.location.isValid()) {
        lookForNetworks();
        screenWipe();
        M5.Lcd.print("Nets: ");
        M5.Lcd.println(totalNetworks);
        M5.Lcd.print(" Sats: ");
        M5.Lcd.println(tinyGPS.satellites.value());
        M5.Lcd.print("Lat: ");
        M5.Lcd.println(tinyGPS.location.lat(), 3);
        M5.Lcd.print(" Lng: ");
        M5.Lcd.println(tinyGPS.location.lng(), 3);
        
      } else {
        Serial.print("Location invalid. Satellite count: ");
        Serial.println(tinyGPS.satellites.value());
        screenWipe();
        M5.Lcd.println("GPS not locked");
        M5.Lcd.print("Sats: ");
        M5.Lcd.println(tinyGPS.satellites.value());
        M5.Lcd.print("Charecters Processsed: ");
        M5.Lcd.println(tinyGPS.charsProcessed());
        M5.Lcd.print("Sentences With Fix: ");
        M5.Lcd.println(tinyGPS.sentencesWithFix());
        M5.Lcd.print("Failed Checksum: ");
        M5.Lcd.println(tinyGPS.failedChecksum());
      }
    smartDelay(LOG_RATE);
}

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available())
      tinyGPS.encode(ss.read());
  } while (millis() - start < ms);
}

void screenWipe() {
  M5.Lcd.setTextSize(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
}


void lookForNetworks() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found");
    M5.Lcd.fillScreen(RED);
  } else {
    for (uint8_t i = 0; i <= n; ++i) {
      if ((isOnFile(WiFi.BSSIDstr(i)) == -1) && (WiFi.channel(i) > 0) && (WiFi.channel(i) < 15)) { //Avoid erroneous channels
        totalNetworks++;
        M5.Speaker.tone(900, 1000);//play a tone for each new network found
        File logFile = SD.open(logFileName, FILE_WRITE);
        Serial.print("New network found ");
        Serial.println(WiFi.BSSIDstr(i));
        M5.Lcd.setCursor(0,10);
        M5.Lcd.print("New:");

        Serial.print("Network name: ");
        Serial.println(WiFi.SSID(i));
 
        Serial.print("Signal strength: ");
        Serial.println(WiFi.RSSI(i));
 
        Serial.print("MAC address: ");
        Serial.println(WiFi.BSSIDstr(i));
 
       



        
        String SSID = WiFi.SSID(i);
        M5.Lcd.println(SSID);
        logFile.print(WiFi.BSSIDstr(i));
        logFile.print(',');
        SSID = WiFi.SSID(i);
        // Commas in SSID brokes the csv file padding
        SSID.replace(",", ".");
        logFile.print(SSID);
        logFile.print(',');
        logFile.print(getEncryption(i));
        logFile.print(',');
        logFile.print(tinyGPS.date.year());
        logFile.print('-');
        logFile.print(tinyGPS.date.month());
        logFile.print('-');
        logFile.print(tinyGPS.date.day());
        logFile.print(' ');
        logFile.print(tinyGPS.time.hour());
        logFile.print(':');
        logFile.print(tinyGPS.time.minute());
        logFile.print(':');
        logFile.print(tinyGPS.time.second());
        logFile.print(',');
        logFile.print(WiFi.channel(i));
        logFile.print(',');
        logFile.print(WiFi.RSSI(i));
        logFile.print(',');
        logFile.print(tinyGPS.location.lat(), 6);
        logFile.print(',');
        logFile.print(tinyGPS.location.lng(), 6);
        logFile.print(',');
        logFile.print(tinyGPS.altitude.meters(), 1);
        logFile.print(',');
        logFile.print((tinyGPS.hdop.value(), 1));
        logFile.print(',');
        logFile.print("WIFI");
        logFile.println();
        logFile.close();
      }
    }
  }
}

String getEncryption(uint8_t network) {
  byte encryption = WiFi.encryptionType(network);
  switch (encryption) {
    case 2:
      return "[WPA-PSK-CCMP+TKIP][ESS]";
    case 5:
      return "[WEP][ESS]";
    case 4:
      return "[WPA2-PSK-CCMP+TKIP][ESS]";
    case 7:
      return "[ESS]";
    case 8:
      return "[WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][ESS]";
  }
}

int isOnFile(String mac) {
  File netFile = SD.open(logFileName);
  String currentNetwork;
  if (netFile) {
    while (netFile.available()) {
      currentNetwork = netFile.readStringUntil('\n');
      if (currentNetwork.indexOf(mac) != -1) {
        Serial.println("The network was already found");
        netFile.close();
        return currentNetwork.indexOf(mac);
      }
    }
    netFile.close();
    return currentNetwork.indexOf(mac);
  }
}

void printHeader() {
  File logFile = SD.open(logFileName, FILE_WRITE);
  if (logFile) {
    int i = 0;
    // logFile.println(wigleHeaderFileFormat); // comment out to disable Wigle header
    for (; i < LOG_COLUMN_COUNT; i++) {
      logFile.print(log_col_names[i]);
      if (i < LOG_COLUMN_COUNT - 1)
        logFile.print(',');
      else
        logFile.println();
    }
    logFile.close();
  }
}

void updateFileName() {
  int i = 0;
  for (; i < MAX_LOG_FILES; i++) {
    memset(logFileName, 0, strlen(logFileName));
    sprintf(logFileName, "%s%d.%s", LOG_FILE_PREFIX, i, LOG_FILE_SUFFIX);
    if (!SD.exists(logFileName)) {
      break;
    } else {
      Serial.print(logFileName);
      Serial.println(" exists");
    }
  }
  Serial.print("File name: ");
  Serial.println(logFileName);
}