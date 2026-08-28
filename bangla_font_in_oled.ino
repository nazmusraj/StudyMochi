#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "bitmaps.h" 

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 22, /* data=*/ 21);

int x_pos = 128; 
int gesture_number = 1; 

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  
  Serial.println("System Ready!");
}

void loop() {
  if (Serial.available() > 0) {
    int input_number = Serial.parseInt(); 
    
    // ধরুন আপনার ৫০টি ছবি আছে, তাই শর্ত ১ থেকে ৫০
    if (input_number >= 1 && input_number <= 50) {
      gesture_number = input_number; 
      x_pos = 128; // পজিশন রিসেট
    }
  }

  u8g2.clearBuffer(); 

  int index = gesture_number - 1; 
  
  // ========================================================
  // অটোমেটিক Width বের করা হচ্ছে আমাদের স্ট্রাকচার থেকে
  // ========================================================
  int current_width = all_bitmaps[index].width; 
  const unsigned char* current_image = all_bitmaps[index].array_data;

  // ছবি ড্র করা
  u8g2.drawXBMP(x_pos, 0, current_width, bangla_height, current_image);

  u8g2.sendBuffer(); 
  
  // ডাইনামিক স্ক্রোলিং রিসেট লজিক
  x_pos = x_pos - 2; 
  if (x_pos < -(current_width + 20)) { 
    x_pos = 128;
  }
  
  delay(10); 
}