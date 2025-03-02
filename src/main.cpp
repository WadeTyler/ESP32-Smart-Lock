/*
 * Project: Smart Lock System
 * Author: Tyler Wade
 * Date: 02/27/2025
 *
 * Description: This project consists of a smart lock door system using an ESP32-Wrover.
 * It features a keypad for locking and unlocking, a lock/unlock button, LED indicator,
 * buzzer sound, WiFi and Bluetooth functionality to lock/unlock.
 *
 */

/*
 * PIN LAYOUT
 *
 * LED_LOCKED 2
 * BUTTON_LOCK 0
 * ACTIVE_BUZZER 12
 * KEYPAD_ROWS = {21, 19, 18, 5}
 * KEYPAD_COLS = {25, 26, 27}
*/


#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>

#include <environment.h>
#include <Keypad.h>
#include <OledDisplay.h>

// ----------------------------- OLED DISPLAY -----------------------------

OledDisplay oled;

// ----------------------------- SOUND -----------------------------
boolean canPlaySound = true;

// ----------------------------- LOCK -----------------------------

boolean isLocked = false;
int lastLockButtonState = HIGH;

// Store current output state
String outputLockState = "Unlocked";

void playLockSound() {
    if (!canPlaySound) return;
    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(500);
    digitalWrite(ACTIVE_BUZZER, LOW);
}

void playUnlockSound() {
    if (!canPlaySound) return;
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
        digitalWrite(LOCK, HIGH);
        Serial.println("Locked");
        outputLockState = "Locked";
        playLockSound();
        oled.setMessage("LOCKED");
        delay(1000);
        oled.setMessage("SMART\nLOCK");
    } else {
        digitalWrite(LED_LOCKED, LOW);
        digitalWrite(LOCK, LOW);
        Serial.println("Unlocked");
        outputLockState = "Unlocked";
        playUnlockSound();
        oled.setMessage("UNLOCKED");
        delay(1000);
        oled.setMessage("SMART\nLOCK");
    }
}

// ----------------------------- KEYPAD -----------------------------

#define ROW_NUM 4
#define COL_NUM 4


byte pin_rows[ROW_NUM] = {32, 19, 18, 5};
byte pin_columns[COL_NUM] = {25, 26, 27};

char keys[ROW_NUM][COL_NUM] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_columns, ROW_NUM, COL_NUM);

String keypadInput = "";

long lastInputTime = millis();
long maxTimeSinceInput = 6000;

void playKeypadSound() {
    if (!canPlaySound) return;
    digitalWrite(ACTIVE_BUZZER, HIGH);
    delay(100);
    digitalWrite(ACTIVE_BUZZER, LOW);
}

void processKey(char key) {
    // Check if key is enter key button ("#")
    if (key == '#') {
        // Check if code matches and unlock if so
        if (keypadInput.toInt() == UNLOCK_CODE && isLocked) {
            Serial.println("Correct Code");
            toggleLocked();
        }

        // If code does not match or the door is not locked
        else {
            Serial.println("Incorrect Code");
            playLockSound();
            oled.setMessage("WRONG\nCODE");
            delay(1000);
            oled.setMessage("SMART\nLOCK");
        }

        keypadInput = "";
    }

    // Check if key is lock button ("*")
    else if (key == '*') {
        if (!isLocked) {
            toggleLocked();
        }
        keypadInput = "";
    }

    // Otherwise add to input string
    else {
        keypadInput += key;
        oled.setMessage(keypadInput);
        playKeypadSound();
        lastInputTime = millis();
    }
}

void checkTimeSinceInput() {
    if (millis() - lastInputTime > maxTimeSinceInput) {
        keypadInput = "";
        oled.setMessage("SMART\nLOCK");
    }
}


// ----------------------------- WIFI -----------------------------

WiFiServer server(80);

// Variable to store the HTTP request
String header;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

void setupWiFI() {
    Serial.println("Connecting to WiFi network");
    WiFi.encryptionType(WIFI_AUTH_WPA2_PSK);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("Connected to WiFi");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
}

void checkWiFiInput() {
    WiFiClient client = server.available();

    if (client) {
        currentTime = millis();
        previousTime = currentTime;
        Serial.println("new wifi client");
        String currentLine = "";
        String pinCode = "";
        bool isPostRequest = false;

        // Read the whole request, including body
        String request = "";
        while (client.connected() && currentTime - previousTime <= timeoutTime) {
            currentTime = millis();
            if (client.available()) {
                char c = client.read();
                request += c;

                // Check if this is a POST request
                if (currentLine.startsWith("POST")) {
                    isPostRequest = true;
                }

                if (c == '\n') {
                    if (currentLine.length() == 0) {
                        // End of headers

                        // If POST, wait a bit more to get the body
                        if (isPostRequest) {
                            delay(100); // Give time for the body to arrive
                            while (client.available()) {
                                request += (char)client.read();
                            }
                        }

                        // Try to find pin in the complete request
                        int pinIndex = request.indexOf("pin=");
                        if (pinIndex >= 0) {
                            pinIndex += 4; // Skip "pin="
                            while (pinIndex < request.length() &&
                                   request[pinIndex] != '&' &&
                                   request[pinIndex] != '\r' &&
                                   request[pinIndex] != '\n') {
                                pinCode += request[pinIndex];
                                pinIndex++;
                            }
                        }

                        Serial.print("Request URL: ");
                        Serial.println(request.substring(0, request.indexOf('\n')));
                        Serial.print("PIN Code: ");
                        Serial.println(pinCode);

                        // Verify PIN code (replace "1234" with your desired PIN)
                        bool isValidPin = pinCode.toInt() == UNLOCK_CODE;

                        // Rest of your code for HTML response
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println("");

                        // Handle lock/unlock with PIN verification
                        if (request.indexOf("/lock") >= 0) {
                            if (!isLocked && (pinCode.length() == 0 || isValidPin)) {
                                toggleLocked();
                            }
                        }
                        else if (request.indexOf("/unlock") >= 0) {
                            if (isLocked && (pinCode.length() == 0 || isValidPin)) {
                                toggleLocked();
                            }
                        }

                        // Your existing HTML code with forms
                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<link rel=\"icon\" href=\"data:,\">");

                        // CSS styling
                        client.println("<style>");
                        client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                        client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
                        client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
                        client.println(".button2 {background-color: #555555;}");
                        client.println("input[type=number] {width: 100px; padding: 12px 20px; margin: 8px 0; box-sizing: border-box;}");
                        client.println("</style></head>");

                        // Body
                        client.println("<body><h1>Smart Lock</h1>");
                        client.println("<p>Lock State: " + outputLockState + "</p>");

                        // Add message if PIN was incorrect
                        if (pinCode.length() > 0 && !isValidPin) {
                            client.println("<p style='color:red'>Invalid PIN code!</p>");
                        }

                        // Forms with PIN input field
                        if (isLocked) {
                            client.println("<form action=\"/unlock\" method=\"post\">");
                            client.println("<input type=\"number\" name=\"pin\" placeholder=\"Enter PIN\" required>");
                            client.println("<p><button type=\"submit\" class=\"button\">Unlock</button></p>");
                            client.println("</form>");
                        } else {
                            client.println("<form action=\"/lock\" method=\"post\">");
                            client.println("<input type=\"number\" name=\"pin\" placeholder=\"Enter PIN\" required>");
                            client.println("<p><button type=\"submit\" class=\"button button2\">Lock</button></p>");
                            client.println("</form>");
                        }

                        client.println("</body></html>");
                        client.println();
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }

        // Clear the header variable
        // Close connection
        client.stop();
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}

void checkWiFiStatus() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Reconnecting to wifi...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

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

void checkBluetoothInput(int lockButtonState, long now) {
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
}




// ----------------------------- SETUP AND LOOP -----------------------------
void setup() {
    // Pin Modes
    pinMode(LED_LOCKED, OUTPUT);
    pinMode(BUTTON_LOCK, INPUT);
    pinMode(ACTIVE_BUZZER, OUTPUT);
    pinMode(LOCK, OUTPUT);
    digitalWrite(LOCK, LOW); // Unlock the lock

    Serial.begin(115200);
    setupBLE(BLE_NAME);
    setupWiFI();
    oled.init();
    oled.setMessage("SMART\nLOCK");
    Serial.println("Setup Complete");
}

void loop() {
    // Check wifi status
    checkWiFiStatus();

    long now = millis();
    int lockButtonState = digitalRead(BUTTON_LOCK);

    // Keypad input
    char key = keypad.getKey();
    if (key) {
        processKey(key);
    }



    // Lock button change
    if (lockButtonState == LOW && lastLockButtonState == HIGH) {
        toggleLocked();
    }

    // Check bluetooth input and handle it
    checkBluetoothInput(lockButtonState, now);

    // Check WiFi
    checkWiFiInput();

    // Update lastLockButtonState
    lastLockButtonState = lockButtonState;

    checkTimeSinceInput();
}
