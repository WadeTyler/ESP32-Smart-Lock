#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <environment.h>

/*
 * Wiring Color Codes
 *
 * RED - LOCK BUTTON AND LIGHT SYSTEM
 * BLUE - ACTIVE BUZZER
 */

// ----------------------------- BLUETOOTH -----------------------------
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
uint8_t txValue = 0;
long lastMsg = 0;
String rxload = "Client Connected\n";

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            rxload="";
            for (int i = 0; i < rxValue.length(); i++) {
                rxload += (char)rxValue[i];
            }
        }
    }
};

void setupBLE(String BLEName) {
    const char *ble_name = BLEName.c_str();
    BLEDevice::init(ble_name);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX,BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
    pCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();
    pServer->getAdvertising()->start();
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setStaticPIN(BLE_STATIC_PIN);
    Serial.println("Waiting a client connection to notify...");
}

// ----------------------------- LOCK -----------------------------



boolean isLocked = false;
int lastLockButtonState = HIGH;

void playLockSound() {
    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(500);
    digitalWrite(ACTIVE_BUZZER, LOW);
}

void playUnlockSound() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(ACTIVE_BUZZER, HIGH);
        delay(100);
        digitalWrite(ACTIVE_BUZZER, LOW);
        delay(100);
    }
}

void toggleLocked() {
    isLocked = !isLocked;

    if (isLocked) {
        // Turn light on
        digitalWrite(LED_LOCKED, HIGH);
        Serial.println("Locked");
        playLockSound();
    } else {
        digitalWrite(LED_LOCKED, LOW);
        Serial.println("Unlocked");
        playUnlockSound();
    }
}

// ----------------------------- SETUP AND LOOP -----------------------------
void setup() {
    pinMode(LED_LOCKED, OUTPUT);
    pinMode(BUTTON_LOCK, INPUT);
    pinMode(ACTIVE_BUZZER, OUTPUT);
    Serial.begin(115200);
    setupBLE(BLE_NAME);
    Serial.println("Setup Complete");
}

void loop() {
    long now = millis();
    int lockButtonState = digitalRead(BUTTON_LOCK);

    // Lock button change
    if (lockButtonState == LOW && lastLockButtonState == HIGH) {
        toggleLocked();
    }

    // Check for serial msg
    if (now - lastMsg > 1000) {

        if (deviceConnected && rxload.length() > 0) {
            Serial.println(rxload);

            // Check if is unlockCode
            if (rxload.toInt() == UNLOCK_CODE && lockButtonState == HIGH) {
                toggleLocked();
                lastLockButtonState = LOW;
            }

            rxload = "";
        }

        if (Serial.available() > 0) {
            String str = Serial.readString();
            const char *newValue = str.c_str();
            pCharacteristic->setValue(newValue);
            pCharacteristic->notify();
        }

        lastMsg = now;
    }

    // Update lastLockButtonState
    lastLockButtonState = lockButtonState;
}

