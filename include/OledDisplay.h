//
// Created by Tyler Wade on 3/2/25.
//

#include <Adafruit_SSD1306.h>

#ifndef OLEDDISPLAY_H
#define OLEDDISPLAY_H

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

class OledDisplay {
public:
    // Constructor
    OledDisplay() {}

    void init() {
        // Initialize oled object
        if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            Serial.println("Failed to initialize display");
            for (;;); // Loop forever
        }

        // Clear the buffer
        display.clearDisplay();

        // Display Text
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 28);
        display.print("SMART LOCK");
        display.display();
    }

    void setMessage(String message) {

        int textSize = message.length() > 20 ? 1 : message.length() > 10 ? 2 : 3;

        display.clearDisplay();
        display.setTextSize(textSize);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.print(message);
        display.display();
    }
};


#endif //OLEDDISPLAY_H
