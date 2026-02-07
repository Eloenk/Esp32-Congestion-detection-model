/*
 * TLC5947 LED Driver - Simple Test Code
 * 
 * Tests all 12 traffic light LEDs (4 lanes × 3 colors)
 * Cycles through each channel with full brightness
 * 
 * Hardware:
 * - ESP32 DevKit
 * - TLC5947 24-channel PWM driver
 * - 12 LEDs (Red, Yellow, Green for 4 lanes)
 * 
 * Wiring:
 * ESP32 GPIO 13 (MOSI) → TLC5947 DIN
 * ESP32 GPIO 14 (SCK)  → TLC5947 CLK
 * ESP32 GPIO 15       → TLC5947 LAT
 * ESP32 5V            → TLC5947 VCC
 * ESP32 GND           → TLC5947 GND
 */

#include <Adafruit_TLC5947.h>

// TLC5947 Configuration
#define TLC_NUM_BOARDS 1    // Using 1 chip (24 channels)
#define TLC_DATA_PIN 13     // DIN (MOSI)
#define TLC_CLOCK_PIN 14    // CLK (SCK)
#define TLC_LATCH_PIN 15    // LAT

// Create TLC5947 object
Adafruit_TLC5947 tlc = Adafruit_TLC5947(TLC_NUM_BOARDS, TLC_CLOCK_PIN, TLC_DATA_PIN, TLC_LATCH_PIN);

// LED Brightness (0-4095 for 12-bit PWM)
#define BRIGHTNESS_FULL 4095
#define BRIGHTNESS_DIM  1024
#define BRIGHTNESS_OFF  0

// Channel mapping for 4 lanes
// Lane ONE: Channels 0, 1, 2 (Red, Yellow, Green)
// Lane TWO: Channels 3, 4, 5 (Red, Yellow, Green)
// Lane THREE: Channels 6, 7, 8 (Red, Yellow, Green)
// Lane FOUR: Channels 9, 10, 11 (Red, Yellow, Green)

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  TLC5947 LED Driver Test                  ║");
  Serial.println("║  Testing 12 Traffic Light Channels        ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  // Initialize TLC5947
  Serial.println("🔧 Initializing TLC5947...");
  tlc.begin();
  
  // Turn off all LEDs initially
  for (int i = 0; i < 24; i++) {
    tlc.setPWM(i, BRIGHTNESS_OFF);
  }
  tlc.write();
  
  Serial.println("✅ TLC5947 initialized successfully!");
  Serial.println("\n📍 Channel Mapping:");
  Serial.println("   Lane ONE:   Ch0=Red, Ch1=Yellow, Ch2=Green");
  Serial.println("   Lane TWO:   Ch3=Red, Ch4=Yellow, Ch5=Green");
  Serial.println("   Lane THREE: Ch6=Red, Ch7=Yellow, Ch8=Green");
  Serial.println("   Lane FOUR:  Ch9=Red, Ch10=Yellow, Ch11=Green");
  Serial.println("\n🔄 Starting sequential test...\n");
  
  delay(2000);
}

void loop() {
  // Test Mode 1: Sequential channel test (one at a time)
  sequentialTest();
  
  delay(1000);
  
  // Test Mode 2: All reds, then all yellows, then all greens
  colorGroupTest();
  
  delay(1000);
  
  // Test Mode 3: Traffic light sequence per lane
  trafficSequenceTest();
  
  delay(1000);
  
  // Test Mode 4: Brightness fade test
  brightnessTest();
  
  delay(2000);
}

// Test each channel one by one
void sequentialTest() {
  Serial.println("═══ Test 1: Sequential Channel Test ═══");
  
  for (int channel = 0; channel < 12; channel++) {
    // Turn on current channel
    tlc.setPWM(channel, BRIGHTNESS_FULL);
    tlc.write();
    
    // Print which LED is on
    int lane = channel / 3;
    int color = channel % 3;
    
    Serial.print("Channel ");
    Serial.print(channel);
    Serial.print(": Lane ");
    
    switch(lane) {
      case 0: Serial.print("ONE   "); break;
      case 1: Serial.print("TWO   "); break;
      case 2: Serial.print("THREE "); break;
      case 3: Serial.print("FOUR  "); break;
    }
    
    switch(color) {
      case 0: Serial.println("RED    ✓"); break;
      case 1: Serial.println("YELLOW ✓"); break;
      case 2: Serial.println("GREEN  ✓"); break;
    }
    
    delay(500);
    
    // Turn off current channel
    tlc.setPWM(channel, BRIGHTNESS_OFF);
    tlc.write();
    delay(200);
  }
  
  Serial.println();
}

// Test all LEDs of same color together
void colorGroupTest() {
  Serial.println("═══ Test 2: Color Group Test ═══");
  
  // All REDS
  Serial.println("All RED LEDs ON (Channels 0, 3, 6, 9)");
  tlc.setPWM(0, BRIGHTNESS_FULL);   // Lane ONE Red
  tlc.setPWM(3, BRIGHTNESS_FULL);   // Lane TWO Red
  tlc.setPWM(6, BRIGHTNESS_FULL);   // Lane THREE Red
  tlc.setPWM(9, BRIGHTNESS_FULL);   // Lane FOUR Red
  tlc.write();
  delay(1500);
  
  // Turn off reds
  tlc.setPWM(0, BRIGHTNESS_OFF);
  tlc.setPWM(3, BRIGHTNESS_OFF);
  tlc.setPWM(6, BRIGHTNESS_OFF);
  tlc.setPWM(9, BRIGHTNESS_OFF);
  tlc.write();
  delay(300);
  
  // All YELLOWS
  Serial.println("All YELLOW LEDs ON (Channels 1, 4, 7, 10)");
  tlc.setPWM(1, BRIGHTNESS_FULL);   // Lane ONE Yellow
  tlc.setPWM(4, BRIGHTNESS_FULL);   // Lane TWO Yellow
  tlc.setPWM(7, BRIGHTNESS_FULL);   // Lane THREE Yellow
  tlc.setPWM(10, BRIGHTNESS_FULL);  // Lane FOUR Yellow
  tlc.write();
  delay(1500);
  
  // Turn off yellows
  tlc.setPWM(1, BRIGHTNESS_OFF);
  tlc.setPWM(4, BRIGHTNESS_OFF);
  tlc.setPWM(7, BRIGHTNESS_OFF);
  tlc.setPWM(10, BRIGHTNESS_OFF);
  tlc.write();
  delay(300);
  
  // All GREENS
  Serial.println("All GREEN LEDs ON (Channels 2, 5, 8, 11)");
  tlc.setPWM(2, BRIGHTNESS_FULL);   // Lane ONE Green
  tlc.setPWM(5, BRIGHTNESS_FULL);   // Lane TWO Green
  tlc.setPWM(8, BRIGHTNESS_FULL);   // Lane THREE Green
  tlc.setPWM(11, BRIGHTNESS_FULL);  // Lane FOUR Green
  tlc.write();
  delay(1500);
  
  // Turn off greens
  tlc.setPWM(2, BRIGHTNESS_OFF);
  tlc.setPWM(5, BRIGHTNESS_OFF);
  tlc.setPWM(8, BRIGHTNESS_OFF);
  tlc.setPWM(11, BRIGHTNESS_OFF);
  tlc.write();
  delay(300);
  
  Serial.println();
}

// Simulate traffic light sequence for each lane
void trafficSequenceTest() {
  Serial.println("═══ Test 3: Traffic Light Sequence ═══");
  
  for (int lane = 0; lane < 4; lane++) {
    int redChannel = lane * 3;
    int yellowChannel = lane * 3 + 1;
    int greenChannel = lane * 3 + 2;
    
    Serial.print("Lane ");
    switch(lane) {
      case 0: Serial.print("ONE   "); break;
      case 1: Serial.print("TWO   "); break;
      case 2: Serial.print("THREE "); break;
      case 3: Serial.print("FOUR  "); break;
    }
    Serial.println(": RED → YELLOW → GREEN → YELLOW → RED");
    
    // RED
    tlc.setPWM(redChannel, BRIGHTNESS_FULL);
    tlc.write();
    delay(1000);
    
    // YELLOW (transition)
    tlc.setPWM(redChannel, BRIGHTNESS_OFF);
    tlc.setPWM(yellowChannel, BRIGHTNESS_FULL);
    tlc.write();
    delay(500);
    
    // GREEN
    tlc.setPWM(yellowChannel, BRIGHTNESS_OFF);
    tlc.setPWM(greenChannel, BRIGHTNESS_FULL);
    tlc.write();
    delay(1000);
    
    // YELLOW (transition back)
    tlc.setPWM(greenChannel, BRIGHTNESS_OFF);
    tlc.setPWM(yellowChannel, BRIGHTNESS_FULL);
    tlc.write();
    delay(500);
    
    // Back to RED
    tlc.setPWM(yellowChannel, BRIGHTNESS_OFF);
    tlc.setPWM(redChannel, BRIGHTNESS_FULL);
    tlc.write();
    delay(300);
    
    // Turn off
    tlc.setPWM(redChannel, BRIGHTNESS_OFF);
    tlc.write();
    delay(200);
  }
  
  Serial.println();
}

// Test PWM brightness levels
void brightnessTest() {
  Serial.println("═══ Test 4: PWM Brightness Test ═══");
  Serial.println("Fading Lane ONE Red LED (Channel 0)");
  
  // Fade UP
  Serial.print("Fade UP: ");
  for (int brightness = 0; brightness <= 4095; brightness += 256) {
    tlc.setPWM(0, brightness);
    tlc.write();
    Serial.print("█");
    delay(50);
  }
  Serial.println(" ✓");
  
  delay(500);
  
  // Fade DOWN
  Serial.print("Fade DOWN: ");
  for (int brightness = 4095; brightness >= 0; brightness -= 256) {
    tlc.setPWM(0, brightness);
    tlc.write();
    Serial.print("█");
    delay(50);
  }
  Serial.println(" ✓");
  
  Serial.println("PWM test complete!\n");
}
