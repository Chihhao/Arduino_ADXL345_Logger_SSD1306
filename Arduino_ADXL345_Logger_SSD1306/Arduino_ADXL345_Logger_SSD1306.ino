//https://github.com/adafruit/Adafruit_ADXL345
//https://github.com/jarzebski/Arduino-DS3231
//https://github.com/adafruit/Adafruit_SSD1306
//https://github.com/Marzogh/SPIMemory
//https://github.com/adafruit/Adafruit-GFX-Library
//https://github.com/adafruit/Adafruit_BusIO

#include <Adafruit_SSD1306.h>
#define OLED_RESET    4
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 //用64的話會記憶體不足
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <Adafruit_ADXL345_U.h>
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345); //assign ID to our accelerometer

float gX, gY, gZ;
float gValue;

#include <DS3231.h>
DS3231 clock;
RTCDateTime dt;
char caRealTime[16];

#include <SPIMemory.h>
SPIFlash flash;
#define CAPA 134217728   //128M
#define ARRSZ 32
uint32_t gAdr=0;
char textAdr[ARRSZ];
#define TIME_INTERVAL 200  //每0.2秒記錄一筆資料
unsigned long lastWriteTime = 0;  

int iNowXPosition = 4;
int iLastY=0;

#define RELAY 3
bool relay = LOW;
float AVG_X = 1.167449;
float STDEV_X = 0.474222;
float UPPERBOUND_X = AVG_X + 3.1*STDEV_X;
float LOWERBOUND_X = AVG_X - 3.1*STDEV_X;

float AVG_Y = 11.71858;
float STDEV_Y = 0.496097;
float UPPERBOUND_Y = AVG_Y + 3.1*STDEV_Y;
float LOWERBOUND_Y = AVG_Y - 3.1*STDEV_Y;

float AVG_Z = -1.41633;
float STDEV_Z = 0.667364;
float UPPERBOUND_Z = AVG_Z + 3.1*STDEV_Z;
float LOWERBOUND_Z = AVG_Z - 3.1*STDEV_Z;


void setup() {
  Serial.begin(115200);
  clock.begin();

  pinMode(RELAY, OUTPUT); 
  digitalWrite(RELAY, LOW);
  
  flash.begin(MB(128));
  gAdr = findIdxOfFlash();
  delay(100);  

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  /*display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE); 
  display.setCursor(0,0); 
  display.println(F("START"));
  display.display();*/
  
  //delay(5000);  
  
  initialScreen(); 
  display.display();

  if(!accel.begin()){ //begin accel, check if it's working
    display.write("No ADXL345 detected");
    while(1);
  }
  accel.setRange(ADXL345_RANGE_8_G);  

}

void loop() { 
  
  getSensorValue();
  
  //if(relay==LOW){
    if(gX>UPPERBOUND_X || gX<LOWERBOUND_X){      
      digitalWrite(RELAY, HIGH);
      Serial.print("! X:"); Serial.println(gX); 
      writeToFlash("ALARM!");  
      relay=HIGH;
    }
    if(gY>UPPERBOUND_Y || gY<LOWERBOUND_Y){
      digitalWrite(RELAY, HIGH);
      Serial.print("! Y:"); Serial.println(gY); 
      writeToFlash("ALARM!"); 
      relay=HIGH;
    }
    if(gZ>UPPERBOUND_Z || gZ<LOWERBOUND_Z){
      digitalWrite(RELAY, HIGH);
      Serial.print("! Z:"); Serial.println(gZ); 
      writeToFlash("ALARM!"); 
      relay=HIGH;
    }
  //}


  if(iNowXPosition>4){
    display.drawLine(iNowXPosition-1 , iLastY, 
                     iNowXPosition , gValue, WHITE);     
  }
  else{
    display.drawPixel(iNowXPosition, gValue, WHITE);
  }
  iLastY = gValue;
  
  iNowXPosition++;  
  if(iNowXPosition>=128){
    iNowXPosition = 4;
    eraseVLine(iNowXPosition, 5);
    //initialScreen();
  }
  else{
    eraseVLine(iNowXPosition, 5);
  }
  
  display.display();  
  
  if(millis() - lastWriteTime > TIME_INTERVAL || 
     millis() < lastWriteTime){
       lastWriteTime = millis();
       writeLog();  
  }
  
}

void eraseVLine(int x, int w){
  int H = SCREEN_HEIGHT-1;
  int MH = SCREEN_HEIGHT/2;
  for(int i=0; i<w; i++){
    //display.drawFastVLine(x+i, 0, MH, BLACK);
    //display.drawFastVLine(x+i, MH+1, MH, BLACK);
    display.drawFastVLine(x+i, 0, H+1, BLACK);
  }
}

void initialScreen(){
  int H = SCREEN_HEIGHT-1;
  int W = SCREEN_WIDTH-1;
  int MH = SCREEN_HEIGHT/2;
  
  display.clearDisplay();

  //水平線
  //display.drawLine(3 , MH, W , MH, WHITE);

  //起點垂直線
  display.drawLine(3 , 0, 3 , H, WHITE);

  //刻度  
  for(int y=MH; y<H; y+=7){
    display.drawFastHLine(0, y, 3, WHITE);
  }  
  for(int y=MH; y>0; y-=7){
    display.drawFastHLine(0, y, 3, WHITE);
  }  
  
}

void getSensorValue(){
  int H = SCREEN_HEIGHT-1;
  sensors_event_t event;
  accel.getEvent(&event);
  gX = event.acceleration.x;
  gY = event.acceleration.y;
  gZ = event.acceleration.z;
  gValue = map(gZ, -5, 5, 0, H);
  if(gValue>H) gValue=H;
  if(gValue<0) gValue=0;
}

void getRealTime(){
  RTCDateTime dt = clock.getDateTime();
  int yy = dt.year-2000;
  int mm = dt.month;
  int dd = dt.day;
  int hh = dt.hour;
  int mn = dt.minute;
  int ss = dt.second;
  sprintf(caRealTime, "%2d%02d%02d%02d%02d%02d", 
          yy, mm, dd, hh, mn, ss);
}

void writeLog(){

  char gValueX[7];
  dtostrf(gX,5,2,gValueX);
  char gValueY[7];
  dtostrf(gY,5,2,gValueY);
  char gValueZ[7];
  dtostrf(gZ,5,2,gValueZ); 
  
  getRealTime();  
  char text[ARRSZ]; 
  sprintf(text, "%s %s,%s,%s", 
          caRealTime, gValueX, gValueY, gValueZ);
  writeToFlash(text);  
}

void writeToFlash(char* str){
  if(gAdr>(CAPA-ARRSZ)){   
    myPrintHex(gAdr);
    Serial.println(F(" reset gAdr=0")); 
    gAdr=0; 
  }
  if(gAdr%4096==0){ erase4K(gAdr); }
  
  if (flash.writeCharArray(gAdr, str, ARRSZ, true)) {
    //myPrintHex(gAdr);
    //Serial.print(F(" W: ")); Serial.println(str);    
    //Serial.print(textAdr); 
    
    /*
    Serial.print(F(" R: ")); 
    if (flash.readCharArray(gAdr, textAdr, ARRSZ)) {
      Serial.println(textAdr); }     
    else{
      Serial.println(F("read fail!")); }    
    */   
      
    gAdr+=ARRSZ;
  }
}

uint32_t findIdxOfFlash(){
  unsigned long adr=0;
  bool gotIndex=false;
  
  for(adr=0; adr<CAPA; adr+=ARRSZ){
    if(flash.readByte(adr)==0xFF){
      gotIndex=true;
      Serial.print(F("findIdx: "));
      myPrintHex(adr); Serial.println();      
      break;
    }
  }
  if(gotIndex==false){
    Serial.print("gotIndex==false");
    adr=0;    
  }
  return adr;  
}

void erase4K(unsigned long addr){
  //myPrintHex(addr);
  if (flash.eraseSector(addr)) {
    //Serial.println(F(" erase 4KB"));
  }
  else {
    Serial.println(F("Erasing sector failed"));
  } 
  //delay(10);
}

void myPrintHex(uint32_t inputInt32){
  if(inputInt32>0x0FFFFFFF){
    Serial.print("0x");
    Serial.print(inputInt32, HEX);
  }
  else if (inputInt32>0x00FFFFFF){
    Serial.print("0x0");
    Serial.print(inputInt32, HEX);
  }
  else if (inputInt32>0x000FFFFF){
    Serial.print("0x00");
    Serial.print(inputInt32, HEX);
  }
  else if (inputInt32>0x0000FFFF){
    Serial.print("0x000");
    Serial.print(inputInt32, HEX);
  }
  else if (inputInt32>0x00000FFF){
    Serial.print("0x0000");
    Serial.print(inputInt32, HEX);
  }
  else if (inputInt32>0x000000FF){
    Serial.print("0x00000");
    Serial.print(inputInt32, HEX);
  }
  else if (inputInt32>0x0000000F){
    Serial.print("0x000000");
    Serial.print(inputInt32, HEX);
  }
  else {
    Serial.print("0x0000000");
    Serial.print(inputInt32, HEX);
  }
  
}
