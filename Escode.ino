#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#define SS_PIN 5 // ESP32 pin GPIO5
#define RST_PIN 4 // ESP32 pin GPIO4
#define RELAY_PIN 32 // ESP32 pin GPIO32 controls the solenoid lock via the relay
#define PIR_PIN 14 // ESP32 pin GPIO14 for PIR sensor
#define DHT_PIN 15 // ESP32 pin GPIO15 for DHT11 sensor
#define OLED_SDA 21 // Define the OLED SDA pin
#define OLED_SCL 22 // Define the OLED SCL pin
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// HEADER FILES
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT11);
const char *ssid = "ESP32-DoorLock";
const char *password = "123456789";
WebServer server(80);
byte authorizedUID[4] = {0xF3, 0x07, 0x96, 0xF4};
bool accessGranted = false;
bool doorClosed = false;
unsigned long unlockTime = 0;
float temperature = 0.0;
float humidity = 0.0;

void readDHTSensor() {
 temperature = dht.readTemperature();
 humidity = dht.readHumidity();
}
void setup() {
 Serial.begin(115200);
 SPI.begin();
 rfid.PCD_Init();
 pinMode(RELAY_PIN, OUTPUT);
 digitalWrite(RELAY_PIN, LOW);
 pinMode(PIR_PIN, INPUT);
 pinMode(33, OUTPUT);
 Wire.begin(OLED_SDA, OLED_SCL);
 
 if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
 Serial.println(F("SSD1306 allocation failed"));
 for (;;) {}
 }
 
 dht.begin();
 
 display.clearDisplay();
 display.setTextSize(1);
 display.setTextColor(SSD1306_WHITE);
 display.setCursor(0, 0);
 display.println("Please tap your card to unlock the door");
 display.display();
 Serial.println("Starting SoftAP");
 WiFi.softAP(ssid, password);
 delay(100);
 IPAddress IP = WiFi.softAPIP();
 Serial.print("AP IP address: ");
 Serial.println(IP);
 server.on("/", HTTP_GET, []() {
 readDHTSensor();
 
 String response = "<!DOCTYPE html><html><head><title>Door Lock</title>";
 response += "<style>";
 response += "body { font-family: Arial, sans-serif; text-align: center; }";
 response += "h1 { margin-top: 50px; }";
 response += "#doorStatus { font-size: 24px; margin-bottom: 20px; }";
 response += ".button { display: inline-block; padding: 15px 30px; font-size: 20px; cursor: pointer; 
text-decoration: none; background-color: #4CAF50; color: white; border: none; border-radius: 5px; 
margin: 10px; }";
 response += ".button:hover { background-color: #45a049; }";
 response += "</style></head><body>";
 response += "<meta http-equiv='refresh' content='2'>";
 response += "<h1>Welcome to the Door Lock Web Server!</h1>";
 response += "<h2 id='doorStatus'>Door Status: ";
 if (accessGranted) {
 response += "Open</h2>";
 } else {
 response += "Closed</h2>";
 }
 response += "<p>Temperature: " + String(temperature) + " °C</p>";
 response += "<p>Humidity: " + String(humidity) + " %</p>";
 response += "<button class='button' onclick='unlock()'>Unlock Door</button>";
 response += "<button class='button' onclick='closeDoor()'>Close Door</button>";
 response += "<script>";
 response += "function unlock() { fetch('/unlock').then(response => { if (response.ok) { 
document.getElementById('doorStatus').innerText = 'Door Status: Open'; } }); }";
 response += "function closeDoor() { fetch('/close').then(response => { if (response.ok) { 
document.getElementById('doorStatus').innerText = 'Door Status: Closed'; } }); }";
 response += "function updateDoorStatus() { fetch('/status').then(response => 
response.text()).then(data => { document.getElementById('doorStatus').innerText = 'Door Status: ' 
+ data; }); }";
 response += "setInterval(updateDoorStatus, 1000);";
 response += "</script></body></html>";
 server.send(200, "text/html", response);
 });
 server.on("/unlock", HTTP_GET, []() {
if (!accessGranted) {
 accessGranted = true;
 unlockTime = millis();
 digitalWrite(RELAY_PIN, LOW);
 displayMessage("Access Granted");
 }
 server.send(200, "text/plain", "Unlock request processed");
 });
 server.on("/close", HTTP_GET, []() {
 if (accessGranted) {
 accessGranted = false;
 digitalWrite(RELAY_PIN, HIGH);
 displayMessage("Door Closed");
 doorClosed = true;
 }
 server.send(200, "text/plain", "Close request processed");
 });
 server.on("/status", HTTP_GET, []() {
 readDHTSensor();
 
 String status;
 status += accessGranted ? "Open" : "Closed";
 server.send(200, "text/plain", status);
 });
 server.begin();
}
void loop() {
 server.handleClient();
 if (accessGranted && (millis() - unlockTime >= 8000)) {
 accessGranted = false;
 digitalWrite(RELAY_PIN, HIGH);
 displayMessage("Please tap your card to unlock the door");
 }

 if (rfid.PICC_IsNewCardPresent()) {
 if (rfid.PICC_ReadCardSerial()) {
 if (compareUID()) {
 accessGranted = true;
 unlockTime = millis();
 digitalWrite(RELAY_PIN, LOW);
 displayMessage("Access Granted");
 } else {
 accessGranted = false;
 displayMessage("Access Denied");
 }
 rfid.PICC_HaltA();
 rfid.PCD_StopCrypto1();
 }
 }
 if (accessGranted && digitalRead(PIR_PIN) == HIGH) {
 digitalWrite(33, LOW); // Turn on the light
 Serial.println("Motion detected");
 } else {
 digitalWrite(33, HIGH); // Turn off the light
 }
 if (doorClosed && !accessGranted) {
 displayMessage("Please tap your card");
 doorClosed = false;
 }
}
bool compareUID() {
 for (int i = 0; i < 4; i++) {
 if (rfid.uid.uidByte[i] != authorizedUID[i]) {
 return false;
 }
 }
 return true;
}
void displayMessage(String message) {
 display.clearDisplay();
 display.setCursor(0, 0);
 display.println(message);
 display.display();
}
