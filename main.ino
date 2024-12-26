#include <Arduino.h>
#include <TFT_eSPI.h>
#include "TouchCal.h"

// Screen resolution
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define DISP_HOR_RES SCREEN_WIDTH
#define DISP_VER_RES SCREEN_HEIGHT

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS

// Create touch SPI instance on VSPI
SPIClass tsSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

void setup() {
    Serial.begin(38400);
    while (!Serial) { delay(50); }
    Serial.println("Serial.OK");

    // Set rotation before initializing
    tc::TC_ROTATION = 3;
    
    delay(10);
    Serial.println("To touch..");
    
    // Initialize touch SPI and touchscreen
    tsSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(tsSPI);
    ts.setRotation(tc::TC_ROTATION);

    delay(100);
    Serial.println("To TFT..");
    
    // Initialize display
    tft.begin();
    tft.setRotation(tc::TC_ROTATION);
    
    Serial.println("Starting calibration...");
    tc::calibration(&ts, &tft);
    
    // After calibration, fill screen with blue
    tft.fillScreen(TFT_BLUE);
    
    Serial.println("Ready to draw! Touch screen to draw in red");
}

// Variables to store the last point for line drawing
int16_t old_x = 0, old_y = 0;
bool first_touch = true;

void loop() {
    TS_Point p = TS_Point();
    bool pressed = tc::isTouch(&ts, &p);
    
    if (pressed) {
        if (first_touch) {
            // First touch point - just store it
            old_x = p.x;
            old_y = p.y;
            first_touch = false;
            // Draw initial dot
            tft.fillCircle(p.x, p.y, 2, TFT_RED);
        } else {
            // Draw line from old point to new point
            tft.drawLine(old_x, old_y, p.x, p.y, TFT_RED);
            // Update old point
            old_x = p.x;
            old_y = p.y;
        }
        
        Serial.print("touched! x: ");
        Serial.print(p.x);
        Serial.print(", y: ");
        Serial.println(p.y);
    } else {
        // When touch is released, reset first_touch
        first_touch = true;
    }
    
    delay(16); // Small delay for smoother drawing
}