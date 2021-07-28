//https://github.com/adafruit/Adafruit_ADXL345
//https://github.com/jarzebski/Arduino-DS3231
//https://github.com/adafruit/Adafruit_SSD1306
//https://github.com/Marzogh/SPIMemory
//https://github.com/adafruit/Adafruit-GFX-Library
//https://github.com/adafruit/Adafruit_BusIO

#define VERSION "ver-3"

#include <Adafruit_SSD1306.h>
#define OLED_RESET    4
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 //用64的話會記憶體不足
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <Adafruit_ADXL345_U.h>
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345); //assign ID to our accelerometer

float fInputX, fInputY, fInputZ;
int iPlotValue;
float UBX, UBY, UBZ;
float LBX, LBY, LBZ;

#include <DS3231.h>
DS3231 RTC;
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

void setup() {
  Serial.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.println(F("SSD1306 Ready"));
  displayWriteOnce(VERSION, "Init...");
  delay(1000);  
  
  RTC.begin();
  Serial.println(F("RTC Ready"));
  
  pinMode(RELAY, OUTPUT); 
  digitalWrite(RELAY, LOW);
  
  flash.begin(MB(128));
  Serial.println(F("flash Ready"));
  gAdr = findIdxOfFlash();
  delay(100);  

  if(!accel.begin()){ //begin accel, check if it's working   
    displayWriteOnce("ADXL345", "Error!") ;
    while(1);
  }
  accel.setRange(ADXL345_RANGE_8_G);  
  Serial.println(F("ADXL345 Ready"));

  //取得最大最小值
  getUpperAndLowerBound();

  initialScreen(); 
  display.display();
}

void displayWriteOnce(char* msg1, char* msg2){
    display.clearDisplay();
    display.setTextSize(2); 
    display.setTextColor(SSD1306_WHITE); 
    display.setCursor(0,0); 
    display.println(msg1);
    display.println(msg2);
    display.display();
}

void getUpperAndLowerBound(){
  // 放棄前五筆資料
  for(int i=0; i<5; i++) { getSensorValue(); }

  //Init
  UBX = -9999.0;  UBY = -9999.0;  UBZ = -9999.0;
  LBX =  9999.0;  LBY =  9999.0;  LBZ =  9999.0;

  for(int i=0; i<100; i++){
    getSensorValue();
    if(fInputX > UBX) UBX = fInputX;
    if(fInputY > UBY) UBY = fInputY;
    if(fInputZ > UBZ) UBZ = fInputZ;
    if(fInputX < LBX) LBX = fInputX;
    if(fInputY < LBY) LBY = fInputY;
    if(fInputZ < LBZ) LBZ = fInputZ;   
    Serial.print("UBX:"); Serial.print(UBX);  Serial.print("  LBX:"); Serial.println(LBX); 
    Serial.print("UBY:"); Serial.print(UBY);  Serial.print("  LBY:"); Serial.println(LBY); 
    Serial.print("UBZ:"); Serial.print(UBZ);  Serial.print("  LBZ:"); Serial.println(LBZ);  
  }

  UBX += 0.60 * (UBX-LBX);
  LBX -= 0.60 * (UBX-LBX);
  UBY += 0.60 * (UBY-LBY);
  LBY -= 0.60 * (UBY-LBY);
  UBZ += 0.60 * (UBZ-LBZ);
  LBZ -= 0.60 * (UBZ-LBZ);  
  Serial.print("UBX:"); Serial.print(UBX);  Serial.print("  LBX:"); Serial.println(LBX); 
  Serial.print("UBY:"); Serial.print(UBY);  Serial.print("  LBY:"); Serial.println(LBY); 
  Serial.print("UBZ:"); Serial.print(UBZ);  Serial.print("  LBZ:"); Serial.println(LBZ);   
}

void loop() {   
  getSensorValue();
 
  if( fInputX > UBX || fInputX < LBX ){      
    Serial.print("! X:"); Serial.println(fInputX); 
    relay = HIGH;
  }
  if( fInputY > UBY || fInputY < LBY ){
    Serial.print("! Y:"); Serial.println(fInputY); 
    relay = HIGH;
  }
  if( fInputZ > UBZ || fInputZ < LBZ ){
    Serial.print("! Z:"); Serial.println(fInputZ); 
    relay = HIGH;
  }

  if(relay == HIGH && digitalRead(RELAY) == LOW){
    writeToFlash("ALARM!"); 
    digitalWrite(RELAY, HIGH);
  }

  if(iNowXPosition>4){
    display.drawLine(iNowXPosition-1 , iLastY, 
                     iNowXPosition , iPlotValue, WHITE);     
  }
  else{
    display.drawPixel(iNowXPosition, iPlotValue, WHITE);
  }
  iLastY = iPlotValue;
  
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
  fInputX = event.acceleration.x;
  fInputY = event.acceleration.y;
  fInputZ = event.acceleration.z;
  iPlotValue = map(fInputZ, -5, 5, 0, H);
  if(iPlotValue>H) iPlotValue = H;
  if(iPlotValue<0) iPlotValue = 0;
}

void getRealTime(){
  RTCDateTime dt = RTC.getDateTime();
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
  char strInputX[7], strInputY[7], strInputZ[7];
  dtostrf(fInputX,5,2,strInputX);
  dtostrf(fInputY,5,2,strInputY);
  dtostrf(fInputZ,5,2,strInputZ); 
  
  getRealTime();  
  char text[ARRSZ]; 
  sprintf(text, "%s %s,%s,%s", caRealTime, strInputX, strInputY, strInputZ);
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
//    myPrintHex(gAdr);
//    Serial.print(F(" W: ")); Serial.println(str);    
//    Serial.print(textAdr); 
    
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
