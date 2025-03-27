#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"

//Cau hinh man hinh OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BUFFER_SIZE 100

//Chan GPIO cho OLED
#define OLED_CLK 18
#define OLED_MOSI 23
#define OLED_RESET 4
#define OLED_DC 17
#define OLED_CS 5

//Gia tri doc lan cuoi cua cam bien de khong bi tre data 
int32_t lastReadTime = 0;

MAX30105 particleSensor;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int32_t bufferLength = BUFFER_SIZE;

//Tham so de tinh SpO2
int32_t spo2, heartBeat; 
int8_t validSpO2, validHeartBeat;

//Tham so de tinh nhip tim
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE]; 
byte ratespot = 0;
long lastBeat = 0; //Thoi gian lan cuoi nhip tim duoc ghi nhan

float beatsPerMinute;
int beatAvg = 0;

//Ham kiem tra du lieu hop le 
bool checkData(){
  validHeartBeat = (beatsPerMinute >= 30 && beatsPerMinute <= 200);
  validSpO2 = (spo2 > 70 && spo2 <= 100);
  return validHeartBeat && validSpO2;
}

//Hien thi dau 3 cham nhap nhay
void display3Dots(){
  int dotsCount = 0;
  static unsigned long lastUpdateTime = 0; //Luu thoi gian cap nhat lan cuoi
  const unsigned long updateInterval = 500; //Thoi gian nhap nhay

  if(millis() - lastUpdateTime >= updateInterval){
    lastUpdateTime = millis();

    display.fillRect(50, 50, 30, 10, BLACK);
    if(dotsCount == 0){
      display.setCursor(50, 50);
      display.print(".");
    }else if(dotsCount == 1){
      display.setCursor(55, 50);
      display.print("..");
    }else if(dotsCount == 2){
      display.setCursor(60, 50);
      display.print("...");
    }

    dotsCount = (dotsCount + 1) % 3;
    display.display();
  }
}

  //Ham doc du lieu Red va IR 100 mau dau tien de tim vung du lieu 
  void first100Samples(){
    display.clearDisplay();
    display.setCursor(10, 5);
    display.print(F("- Pulse Oximeter - "));
    display.setCursor(10, 20);
    display.print(F("-- Put finger on --"));
    display.setCursor(0, 30);
    display.print(F("First 100 samples..."));
    display.setCursor(0, 50);
    display.print(F("---------------------"));
    display.display();

    for(byte i = 0; i < bufferLength; i++){
      if(millis() - lastReadTime >= 10){
        while(!particleSensor.available() == false){
        particleSensor.check();
      }
        lastReadTime = millis();
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample();
      }

      if(checkForBeat(irBuffer[i]) == true){
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = (60 / (delta / 1000.0));

        if(beatsPerMinute <= 200 && beatsPerMinute >= 30){
          rates[ratespot++] = (byte)beatsPerMinute; //Luu gia tri vao mang
          ratespot %= RATE_SIZE; 

          //Lay trung binh cac gia tri doc duoc
          for(byte x = 0; x < RATE_SIZE; x++){
            beatAvg += rates[x];
          }
          beatAvg /= RATE_SIZE;
        }
      }
      //Hien thi du lieu buffer cua RED va IR (chua hien thi nhip tim va SpO2 vi dang doc du lieu de tinh toan)
      Serial.print(F("Red: "));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", IR: "));
      Serial.println(irBuffer[i], DEC);
    }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSpO2, &heartBeat, &validHeartBeat);

  //Hien thi ket qua sau khi thu thap va tinh toan du lieu
  display.clearDisplay();
  display.setCursor(10, 5);
  display.print(F("- Pulse Oximeter -"));
  if(checkData()){
    display.setCursor(0, 20);
    display.print(F("Nhip tim: "));
    display.setCursor(70, 20);
    display.print(beatsPerMinute, 2);
    display.print(F("bpm"));

    display.setCursor(0, 35);
    display.print(F("Avg: "));
    display.setCursor(70, 35);
    display.print(beatAvg, 0);
    display.print(F("bpm"));

    display.setCursor(0, 50);
    display.print(F("SpO2: "));
    display.setCursor(70, 50);
    display.print(spo2, DEC);
    display.print(F(" %"));
    }else{
      display.setCursor(25, 30);
      display.print(F("Invalid Data !"));
    }
    display.display();
}

void collectNextSamples(){
  bool isValid = checkData();
  while(1){
    for(byte i = 25; i < bufferLength; i++){
      memmove(redBuffer, &redBuffer[25], (bufferLength - 25) * sizeof(uint32_t));
      memmove(irBuffer, &irBuffer[25], (bufferLength - 25) * sizeof(uint32_t));
      //redBuffer[i - 25] = redBuffer[i];
      //irBuffer[i - 25] = irBuffer[i];
    }

    //Thu thap 25 mau tin hieu moi vao 25 mau vua moi dump kia
    for(byte i = 75; i < bufferLength; i++){
      while(!particleSensor.available() == false){
        particleSensor.check();
      }
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR(); 
      particleSensor.nextSample();

      if(checkForBeat(irBuffer[i]) == true){
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = (60 / (delta / 1000.0));
          
        if(beatsPerMinute < 200 && beatsPerMinute > 20){
          rates[ratespot++] = (byte)beatsPerMinute; //Luu gia tri vao mang
          ratespot %= RATE_SIZE; 

          //Lay trung binh cac gia tri doc duoc
          for(byte x = 0; x < RATE_SIZE; x++){
            beatAvg += rates[x];
          }
          beatAvg /= RATE_SIZE;
        }
      }
      if(checkData()){
        Serial.print(F("Red: ")); 
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", IR: "));
        Serial.print(irBuffer[i], DEC);

        Serial.print(F(", Nhip tim: "));
        Serial.print(beatsPerMinute, 2);

        Serial.print(F(", HBvalid: "));
        Serial.print(validHeartBeat, DEC);

        Serial.print(F(", SpO2: "));
        Serial.print(spo2, DEC);

        Serial.print(F(", SPO2Valid: "));
        Serial.println(validSpO2, DEC);
      }else{
        Serial.print(F("Red: "));
        Serial.print(redBuffer[i], DEC);
        Serial.print(F(", IR: "));
        Serial.print(irBuffer[i], DEC);

        Serial.println(F(", Invalid Data !"));
      }
    }

    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSpO2, &heartBeat, &validHeartBeat); 

    //Hien thi ket qua sau khi thu thap va tinh toan du lieu
    display.clearDisplay();
    display.setCursor(10, 5);
    display.print(F("- Pulse Oximeter -"));

    if(checkData()){
      display.setCursor(0, 20);
      display.print(F("Nhip tim: "));
      display.setCursor(70, 20);
      display.print(beatsPerMinute, 2);
      display.print(F("bpm"));

      display.setCursor(0, 35);
      display.print(F("Avg: "));
      display.setCursor(70, 35);
      display.print(beatAvg, 0);
      display.print(F("bpm"));

      display.setCursor(0, 50);
      display.print(F("SpO2: "));
      display.setCursor(70, 50);
      display.print(spo2, DEC);
      display.print(F(" %"));

    }else{
      display.setCursor(25, 30);
      display.print(F("Invalid Data !"));
    }
    display.display();
  }
}

void setup(){
  Serial.begin(115200);

  //Khoi tao man hinh SSD1306
  if(!display.begin(SSD1306_SWITCHCAPVCC)){
    Serial.println(F("SSD1306 khoi tao that bai..."));
    for(;;); //Dung chuong trinh neu khong khoi tao duoc man hinh 
  }
  //Hien thi man hinh khoi tao 
  display.display();
  delay(100);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  //Khoi tao MAX30102 qua I2C
  if(!particleSensor.begin(Wire, I2C_SPEED_FAST)){
    Serial.println(F("Ket noi voi MAX30102 that bai..."));
    for(;;);
  }else{
    Serial.println(F("Ket noi voi MAX30102 thanh cong !"));
  }
    
  //Cau hinh cam bien MAX30102
  byte ledBrightness = 70;  //Do sang LED (0-255)
  byte sampleAverage = 4;   //So mau trung binh: 1, 2, 4, 8, 16, 32
  byte ledMode = 2;         //Che do LED: 1 = Red, 2 = Red + IR
  byte sampleRate = 100;    //So lan lay mau trong 1s
  int pulseWidth = 411;     //Do rong xung: 69, 118, 215, 411
  int adcRange = 4096;      //Dai ADC: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  //Hien thi 1 lan
  display.clearDisplay();
  display.setCursor(20,5);
  display.print(F("PULSE OXIMETER"));
  display.setCursor(40,30);
  display.print(F("by PHUC"));
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop(){
  first100Samples();
  collectNextSamples();
}