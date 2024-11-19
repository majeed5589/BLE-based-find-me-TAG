#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define DEVICE_NAME "tv_remote"  // Change this to "book_pad", etc., as needed
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define LED_PIN 2  // Built-in LED pin on most ESP32 boards

#define DEEP_SLEEP_SECONDS 15      // Sleep duration in seconds
#define ADVERTISING_DURATION 4000  // Advertising duration in milliseconds

bool deviceConnected = false;
bool findCommandReceived = false;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

unsigned long advertisingStartTime = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    pServer->startAdvertising();  // Restart advertising
    advertisingStartTime = millis();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    // Convert std::string to Arduino String
    String rxValue = String(pCharacteristic->getValue().c_str());

    if (rxValue.length() > 0) {
      Serial.print("Received Value: ");
      Serial.println(rxValue);

      // Check for "find" command
      if (rxValue == "find") {
        findCommandReceived = true;
        digitalWrite(LED_PIN, HIGH);  // Turn on LED
        Serial.println("LED turned ON");
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Ensure LED is off

  // Initialize BLE
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue(DEVICE_NAME);

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Helps with iPhone connections
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");

  advertisingStartTime = millis();
}

void loop() {
  // Check if advertising duration has passed without a connection
  if (!deviceConnected && (millis() - advertisingStartTime >= ADVERTISING_DURATION)) {
    Serial.println("No device connected. Going to sleep.");
    BLEDevice::deinit();

    // Configure the wake-up source as timer
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SECONDS * 1000000ULL);
    Serial.println("Entering deep sleep...");
    Serial.flush();
    esp_deep_sleep_start();
  }

  // If the find command was received, keep the LED on for 5 seconds
  if (findCommandReceived) {
    delay(5000);
    digitalWrite(LED_PIN, LOW);  // Turn off LED
    findCommandReceived = false;
    Serial.println("LED turned OFF");

    // After handling the command, go to sleep
    BLEDevice::deinit();
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SECONDS * 1000000ULL);
    Serial.println("Entering deep sleep...");
    Serial.flush();
    esp_deep_sleep_start();
  }

  delay(10);  // Short delay to prevent watchdog timer resets
}
