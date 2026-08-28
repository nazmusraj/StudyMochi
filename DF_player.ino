#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

// ESP32 এর নিজস্ব Hardware Serial 2
HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;

const int RX_PIN = 26; // ESP32 RX -> DFPlayer TX
const int TX_PIN = 27; // ESP32 TX -> DFPlayer RX

void setup() {
  Serial.begin(115200);
  delay(3000); 

  Serial.println(F("\n--- System Starting ---"));
  
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  Serial.println(F("Initializing DFPlayer..."));

  // ফোর্স স্টার্ট (DFPlayer এর উত্তরের অপেক্ষা না করে সরাসরি শুরু)
  if (!myDFPlayer.begin(mySerial, false, true)) {
    Serial.println(F("Unable to begin. Check connections!"));
    while(true);
  }
  
  Serial.println(F("DFPlayer Mini online."));
  
  // ভলিউম ম্যাক্সিমাম (৩০) করে দেওয়া হলো
  myDFPlayer.volume(30);  
  
  Serial.println(F("Ready! Type 1, 2, or 3 in Serial Monitor and press Enter..."));
}

// ==========================================
// অডিও প্লে করার ফাংশন
// ==========================================
// ==========================================
// অডিও প্লে করার ফাংশন
// ==========================================
void playVoice(int trackNumber) {
  Serial.print("Command Received! Playing Audio No: ");
  Serial.println(trackNumber);
  
  // আগের লাইনটি (এটি মুছে দিন):
  // myDFPlayer.playMp3Folder(trackNumber); 
  
  // নতুন লাইন (এটি বসান):
  myDFPlayer.play(trackNumber); 
}
// ==========================================

void loop() {
  // সিরিয়াল মনিটরে আপনি কিছু লিখেছেন কিনা তা চেক করবে
  if (Serial.available() > 0) {
    // আপনার লেখাটাকে নাম্বারে কনভার্ট করবে
    int inputNumber = Serial.parseInt(); 
    
    // নাম্বারটি 0 এর চেয়ে বড় হলে অডিও প্লে করবে
    if (inputNumber > 0) {
       playVoice(inputNumber);
    }
  }
}