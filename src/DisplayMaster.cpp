#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <SPI.h>
#include <TouchScreen.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Wire.h>


// The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET 10

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

#define TS_MINX 150
#define TS_MINY 150
#define TS_MAXX 920
#define TS_MAXY 920

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Assign human-readable names to some common 16-bit colour values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define SPRING  0x26D3
#define TEAL    0x05F8
#define SALMON  0xFBED
#define ORANGE  0xFC00

#define SD_CS 10     // Set the chip select line to whatever you use (10 doesnt conflict with the library)

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
#define PENRADIUS 3

byte pageNum = 5;
byte relay = 0;
byte rgbProfile = 1;
byte targetTemp = 180;
byte fanSpeed = 0;
byte ledPower = 255;
byte relayPrev = 2;
byte ledPrev = 2;
boolean colWheel = false;
boolean backFill = true;
int graphPixel = 0;
int tempAmb = 1;
int tempActual = 1;
int vosVal = 1;
int prevTemp = 0;
int prevAmb = 0;
int prevVos = 0;
int bufLen = 10;
int yadj = 0;
float ammeterVal = 1.00;
float prevAmp = 0;
unsigned long time;
char colour[12] = "Red";
char valSend[10];
char buffer[32];
int x1 = -10;
int x2 = 0;
int x3 = 0;
int y1 = 200;
int y2 = 140;
int wb = 0;
int wt = 0;
byte graph = 2;
unsigned long elapsed, remainder, finish;
unsigned long start = 0;
float m;
float s;
int mPrev = 1;
int sPrev;

//------Master Sender------//
#define masterAddr 0x23
#define slaveAddr 0x24
#define ypos 160

void setup() {
  Serial.begin(9600);
  Wire.begin(masterAddr);
  TWBR = 12;
  tft.begin(0x9325);
  tft.setRotation(3);
  tft.setFont(&FreeSans18pt7b);
  tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
  homeDisplay();
}


void loop() {
  #define MINPRESSURE 10
  #define MAXPRESSURE 1000
  time = millis();

  if (pageNum == 5  && time % 50 == 0) {
    slaveRequest();
    homeDisplay();
  }

  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    int x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    int y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);

    switch(pageNum) {
      case 1:
        powerTouch(x, y);
        break;
      case 2:
        ledTouch(x, y);
        break;
      case 3:
        tempTouch(x, y);
        break;
      case 4:
        fanTouch(x, y);
        break;
      case 5:
        homeTouch(x, y);
        break;
    }
  }
  if (pageNum == 5 && tempActual > (targetTemp-15) && tempActual < (targetTemp+15) && time % 50 == 0 && graph == 0){
    yadj = (targetTemp-tempActual)*3;
    tft.drawLine(graphPixel, ypos, graphPixel, (ypos+yadj), SALMON);
    graphPixel++;

    if (graphPixel > 320){
      graphPixel = -1;
    }
    tft.fillRect(graphPixel+1, 110, 5, 100, BLACK);
    tft.drawLine(graphPixel-5, 160, graphPixel-1, 160, WHITE);
  }


  if (pageNum == 5 && time % 50 == 0 && graph == 1) {
    int pwmWidth = map(vosVal, 0, 255, 40, 0);
    wb = 40-pwmWidth;
    wt = pwmWidth;

    if (x3 < 320) {
      x1 = x3;
    } else x1 = -10;

    x2 = x1+wb;
    x3 = x2+wt;
    tft.fillRect(x1+1, y2, 40, 65, BLACK);
    if (wb != 40 && wt != 40) {
      tft.drawLine(x1, y1, x2, y1, WHITE);
      tft.drawLine(x2, y1, x2, y2, WHITE);
      tft.drawLine(x2, y2, x3, y2, WHITE);
      tft.drawLine(x3, y2, x3, y1, WHITE);
    } else if (wb == 40) {
      tft.drawFastHLine(0, y1, 320, WHITE);
    } else if (wt == 40) {
      tft.drawFastHLine(0, y2, 320, WHITE);
    }
  }

  if (pageNum == 5 && time % 50 == 0 && graph == 2 ) {
    stopWatch();
  }
}


void homeDisplay() {
  int refresh = 1;

  if (backFill == true) {
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    refresh = 0;
    tft.setFont(&FreeMono12pt7b);
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 30);
    tft.println(F("Ambient "));
    tft.setCursor(10, 55);
    tft.println(F("Target    "));
    tft.setCursor(10, 80);
    tft.println(F("Amps  "));
    tft.setCursor(10, 105);
    tft.println(F("Fan      %"));
    tft.setCursor(190, 80);
    tft.println(F("VoSense"));
    tft.setCursor(110, 55);
    tft.println(targetTemp);
   // tft.setCursor(10, 230);
   // tft.println(F("Power:"));
    tft.setCursor(55, 230);
    tft.println(F("/"));
    if (ledPower == 0){
      tft.setTextColor(SALMON);
    } else if (ledPower > 0){
      tft.setTextColor(SPRING);
    }
    tft.setCursor(10, 230);
    tft.println(F("LED"));
    if (relay == 0){
      tft.setTextColor(SALMON);
    } else if (relay == 1){
      tft.setTextColor(SPRING);
    }
    tft.setCursor(70, 230);
    tft.println(F("Coil"));
    tft.setTextColor(WHITE);
    if (fanSpeed < 100 && fanSpeed > 0) {
      tft.setCursor(105, 105);
    } else if (fanSpeed  == 100) {
      tft.setCursor(90, 105);
    } else if (fanSpeed == 0) {
      tft.setCursor(120, 105);
    }
    tft.println(fanSpeed);

    backFill = false;
  }

  if (tempActual != prevTemp && tempActual > 1 && tempActual < 300 || refresh == 0) {
    tft.fillRect(180, 0, 120, 60, BLACK);
    tft.setFont(&FreeSans18pt7b);
    tft.setTextSize(2);
    tft.setCursor(180, 55);
    tft.setTextColor(SALMON);
    tft.println(tempActual);
    tft.setTextSize(1);
    tft.setFont(&FreeMono12pt7b);
    tft.setTextColor(WHITE);
    prevTemp = tempActual;
  }
  if (tempAmb != prevAmb || refresh == 0) {
    tft.fillRect(125, 10, 55, 25, BLACK);
    tft.setCursor(125, 30);
    tft.println(tempAmb);
    prevAmb = tempAmb;
  }
  if (ammeterVal != prevAmp || refresh == 0) {
    tft.fillRect(90, 60, 70, 25, BLACK);
    tft.setCursor(90, 80);
    tft.println(ammeterVal);
    prevAmp = ammeterVal;
  }
  if (vosVal != prevVos || refresh == 0) {
    tft.fillRect(220, 85, 60, 25, BLACK);
    tft.setCursor(220, 105);
    tft.println(vosVal);
    prevVos = vosVal;
  }
  if (graph == 0) {
    tft.fillRect(240, 200, 80, 40, BLACK);
    tft.setCursor(240, 230);
    tft.println("Graph");
  }
  if (graph == 1) {
    tft.fillRect(240, 200, 80, 40, BLACK);
    tft.setCursor(240, 230);
    tft.println("PWM");
  }
  if (graph == 2) {
    tft.fillRect(240, 200, 80, 40, BLACK);
    tft.setCursor(240, 230);
    tft.println("Timer");
    tft.setCursor(230, 160);
    tft.println("Reset");
  }
  refresh == 1;
}


void homeTouch(int x, int y) {
  if ((200 < y && y < 230) && (0 < x && x < 170)) {
    pageNum = 1;
    backFill = true;
    graphPixel = 0;
    mPrev = 99;
    powerDisplay();

  } else if ((0 < y && y < 60) && (180 < x && x < 300)) {
    pageNum = 2;
    backFill = true;
    graphPixel = 0;
    mPrev = 99;
    ledDisplay();

  } else if ((30 < y && y < 60) && (0 < x && x < 160)) {
    pageNum = 3;
    backFill = true;
    graphPixel = 0;
    mPrev = 99;
    tempDisplay();
  } else if ((80 < y && y < 120) && (0 < x && x < 160)) {
    pageNum = 4;
    backFill = true;
    graphPixel = 0;
    mPrev = 99;
    fanDisplay();
  } else if ((200 < y && y < 240) && (280 < x && x < 320)) {
    graph++;
    if (graph > 2) {
      graph = 0;
    }
    graphPixel = 0;
    x3 = 320;
    mPrev = 99;
    tft.fillRect(0, 115, 320, 90, BLACK);
  } else if ((145 < y && y < 170) && (230 < x && x < 320) && graph == 2) {
    start = millis();
  }
}


void powerDisplay() {
  if (backFill == true){
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    backFill = false;
  }
  tft.setTextSize(1);
  tft.setFont(&FreeSans18pt7b);
  if (relay == 0) {
    tft.setCursor(70, 80);
    tft.setTextColor(SALMON);
    tft.println(F("OFF"));
    tft.setCursor(190, 80);
    tft.setTextColor(WHITE);
    tft.println(F("ON"));
  }
  if (relay == 1) {
    tft.setCursor(70, 80);
    tft.setTextColor(WHITE);
    tft.println(F("OFF"));
    tft.setCursor(190, 80);
    tft.setTextColor(SPRING);
    tft.println(F("ON"));
  }

  if (ledPower == 0) {
    tft.setCursor(70, 185);
    tft.setTextColor(SALMON);
    tft.println(F("OFF"));
    tft.setCursor(190, 185);
    tft.setTextColor(WHITE);
    tft.println(F("ON"));
  }
  if (ledPower > 0) {
    tft.setCursor(70, 185);
    tft.setTextColor(WHITE);
    tft.println(F("OFF"));
    tft.setCursor(190, 185);
    tft.setTextColor(SPRING);
    tft.println(F("ON"));
  }

  tft.setCursor(160, 80);
  tft.setTextColor(WHITE);
  tft.println(F("/"));
  tft.setCursor(160, 185);
  tft.println(F("/"));
  tft.setTextSize(1);
  tft.setFont(&FreeMono12pt7b);
  tft.setCursor(10, 230);
  tft.setTextColor(WHITE);
  tft.println(F("Back"));
  tft.setCursor(40, 30);
  tft.println(F("Induction Circuit"));
  tft.setCursor(100, 135);
  tft.println(F("LED Ring"));
  tft.setFont(&FreeSans18pt7b);
}


void powerTouch(int x, int y) {
  relayPrev = relay;
  ledPrev = ledPower;
  char relayChar[] = "r";
  char ledChar[] = "l";

  if ((50 < y && y < 90) && (70 < x && x < 160)) {
    relay = 0;
    powerDisplay();
  } else if ((50 < y && y < 90) && (190 < x && x < 270)) {
    relay = 1;
    powerDisplay();
  } else if ((155 < y && y < 195) && (70 < x && x < 160)) {
    ledPower = 0;
    powerDisplay();
  } else if ((155 < y && y < 195) && (190 < x && x < 250)) {
    ledPower = 255;
    powerDisplay();
  } else if ((210 < y && y < 240) && (10 < x && x < 80)) {
    backFill = true;
    pageNum = 5;
    homeDisplay();
  }

  if (relayPrev != relay) {
    I2CString(relay, relayChar);
  }
  if (ledPrev != ledPower) {
    I2CString(ledPower, ledChar);
  }
}

void ledDisplay() {
  if (backFill == true) {
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    backFill = false;
  }
  tft.setFont(&FreeMono12pt7b);
  tft.setCursor(80, 50);
  tft.setTextColor(WHITE);
  tft.println(F("Random :"));

  if ( colWheel == false) {
    tft.fillRect(195, 20, 50, 40, BLACK); // clear previous value
    tft.setCursor(195, 50);
    tft.setTextColor(SALMON);
    tft.println(F("OFF"));
  }
  else if (colWheel == true) {
    tft.fillRect(195, 20, 50, 40, BLACK);
    tft.setCursor(195, 50);
    tft.setTextColor(SPRING);
    tft.println(F("ON"));
  }

  tft.setCursor(70, 170);
  tft.setTextColor(WHITE);
  tft.println(F("prev"));

  tft.setCursor(190, 170);
  tft.setTextColor(WHITE);
  tft.println(F("next"));

  tft.setCursor(10, 230);
  tft.setTextColor(WHITE);
  tft.println(F("Back"));

  tft.drawRect(110, 210, 200, 20, WHITE);
  int pos = map(ledPower, 0, 255, 0, 198);
  tft.fillRect(111, 211, pos, 18, SALMON);

  tft.fillRect(70, 80, 200, 60, BLACK); // clear previous value
  tft.setFont(&FreeSans18pt7b);
  int len = strlen(colour);
  int xpos = 80+((9-len)*6);
  tft.setCursor(xpos, 120); //70
  tft.setTextColor(WHITE);
  tft.println(colour);

}


void ledTouch(int x, int y) {
  char colChar[] = "c";
  char rgbChar[] = "p";
  char ledChar[] = "l";

  if ((150 < y && y < 190) && (70 < x && x < 130)) {
    rgbProfile--;
    if (rgbProfile < 1) {
      rgbProfile = 18;
    }
    colourChart();
    if (colWheel == true) {
      colWheel = false;
      I2CString(colWheel, colChar);
      delay(5);
    }
    I2CString(rgbProfile, rgbChar);
    ledDisplay();

  } else if ((150 < y && y < 190) && (190 < x && x < 250)) {
    rgbProfile++;
    if (rgbProfile > 18) {
      rgbProfile = 1;
    }
    colourChart();
    if (colWheel == true) {
        colWheel = false;
        I2CString(colWheel, colChar);
        delay(5);
    }
    I2CString(rgbProfile, rgbChar);
    ledDisplay();
  } else if ((25 < y && y < 65) && (185 < x && x < 250)) {
    colWheel = !colWheel;
    I2CString(colWheel, colChar);
    ledDisplay();
  } else if ((210 < y && y < 240) && (110 < x && x < 310)) {
    tft.drawRect(110, 210, 200, 20, WHITE);
    int x2 = map(x, 110, 310, 0, 199);
    tft.fillRect(x, 211, (309-x), 18, BLACK);
    tft.fillRect(111, 211, x2, 18, SALMON);
    ledPower = map(x, 110, 310, 0, 255);
    I2CString(ledPower, ledChar);
  } else if ((200 < y && y < 230) && (10 < x && x < 70)) {
    pageNum = 5;
    backFill = true;
    I2CString(rgbProfile, rgbChar);
    homeDisplay();
  }
}

void tempDisplay() {
  tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
  tft.setFont(&FreeSans18pt7b);
  tft.setTextSize(2);
  tft.setCursor(30, 135); //70 180
  tft.setTextColor(WHITE);
  tft.println(F("<"));

  tft.setCursor(250, 135); //210 180
  tft.setTextColor(WHITE);
  tft.println(F(">"));

  tft.setCursor(100, 140);
  tft.setTextColor(WHITE);
  tft.println(targetTemp);

  tft.setTextSize(1);
  tft.setFont(&FreeMono12pt7b);
  tft.setCursor(10, 230);
  tft.setTextColor(WHITE);
  tft.println(F("Back"));

  tft.setCursor(5, 40);
  tft.setTextColor(WHITE);
  tft.println(F("Set Target Temperature"));
  tft.setFont(&FreeSans18pt7b);
}

void tempTouch(int x, int y) {
  char tempChar[] = "t";
  if ((95 < y && y < 145) && (30 < x && x < 100)) {
    targetTemp--;
    targetTemp = constrain(targetTemp, 150, 250);
    tft.fillRect(100, 90, 120, 60, BLACK); // clear previous value
    tft.setCursor(100, 140);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println(targetTemp);
    tft.setTextSize(1);

  } else if ((95 < y && y < 145) && (250 < x && x < 320)) {
    targetTemp++;
    targetTemp = constrain(targetTemp, 150, 250);
    tft.fillRect(100, 90, 120, 60, BLACK); // clear previous value
    tft.setCursor(100, 140);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println(targetTemp);
    tft.setTextSize(1);

  } else if ((200 < y && y < 230) && (10 < x && x < 70)) {
    pageNum = 5;
    backFill = true;
    I2CString(targetTemp, tempChar);
    homeDisplay();
    // send targetTemp via I2C
  }
}

void fanDisplay() {
  if (backFill == true) {
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    backFill = false;
  }

  tft.setFont(&FreeMono12pt7b);
  tft.setCursor(10, 230);
  tft.setTextColor(WHITE);
  tft.println(F("Back"));

  tft.setCursor(65, 40);
  tft.setTextColor(WHITE);
  tft.println(F("Set Fan Speed"));

  tft.setFont(&FreeSans18pt7b);
  tft.setTextSize(2);
  tft.setCursor(70, 180);
  tft.setTextColor(WHITE);
  tft.println(F("<"));

  tft.setCursor(210, 180);
  tft.setTextColor(WHITE);
  tft.println(F(">"));

  tft.fillRect(100, 70, 120, 60, BLACK); // clear previous value
  if (fanSpeed == 100){
    tft.setCursor(100, 120);
  } else if (fanSpeed == 0) {
    tft.setCursor(140, 120);
  } else {
    tft.setCursor(120, 120);
  }
  tft.setTextColor(WHITE);
  tft.println(fanSpeed);
  tft.setTextSize(1);

}

void fanTouch(int x, int y) {
char fanChar[] = "f";
int fanSpeedPWM;
  if ((145 < y && y < 195) && (70 < x && x < 125)) {
    fanSpeed = fanSpeed-10;
    fanSpeed = constrain(fanSpeed, 0, 100);
    fanDisplay();

  } else if ((145 < y && y < 195) && (210 < x && x < 265)) {
    fanSpeed = fanSpeed+10;
    if (fanSpeed > 100) {
      fanSpeed = 0;
    }
    fanSpeed = constrain(fanSpeed, 0, 100);
    fanDisplay();
  } else if ((200 < y && y < 230) && (10 < x && x < 70)) {
    pageNum = 5;
    backFill = true;
    fanSpeedPWM = map(fanSpeed, 0, 100, 0, 255);
    I2CString(fanSpeedPWM, fanChar);
    homeDisplay();
    // send fanSpeed via I2C
  }
}


void I2CString(int v, char c[1]) {
  char valArray[10];
  itoa(v, valArray, 10);
  valSend[0] = c[0];
    for (int i = 1; i < bufLen; i++) {
      valSend[i] = valArray[(i-1)];
    }
  I2CSend();
}


void I2CSend() {
  Wire.beginTransmission(slaveAddr);
  Wire.write(valSend);
  Wire.endTransmission();
}


void slaveRequest() {
  int i = 0;
  Wire.requestFrom(slaveAddr, bufLen);
  while(Wire.available() > 0) {
    buffer[i] = Wire.read();
    i++;
  }
  if (buffer[0] == 't') {
    buffer[0] = '0';
    tempActual = atoi(buffer);
    memset(buffer, 0, 32);
  } else if (buffer[0] == 'a') {
    buffer[0] = '0';
    tempAmb = atoi(buffer);
    memset(buffer, 0, 32);
  } else if (buffer[0] == 'c') {
    buffer[0] = '0';
    int ammVal = atoi(buffer);
    ammeterVal = float(ammVal)/100.00;
    memset(buffer, 0, 32);
  } else if (buffer[0] == 'v') {
    buffer[0] = '0';
    vosVal = atoi(buffer);
    memset(buffer, 0, 32);
  }
}


void colourChart() {
  if (rgbProfile == 1) {
    char Red[] = "Red";
    strncpy(colour, Red, 12);
  } else if (rgbProfile == 2) {
    char DarkRed[] = "Dark Red";
    strncpy(colour, DarkRed, 12);
  } else if (rgbProfile == 3) {
    char Orange[] = "Orange";
    strncpy(colour, Orange, 12);
  } else if (rgbProfile == 4) {
    char Gold[] = "Gold";
    strncpy(colour, Gold, 12);
  } else if (rgbProfile == 5) {
    char Yellow[] = "Yellow";
    strncpy(colour, Yellow, 12);
  } else if (rgbProfile == 6) {
    char Lime[] = "Lime";
    strncpy(colour, Lime, 12);
  } else if (rgbProfile == 7) {
    char Apple[] = "Apple";
    strncpy(colour, Apple, 12);
  } else if (rgbProfile == 8) {
    char Green[] = "Green";
    strncpy(colour, Green, 12);
  } else if (rgbProfile == 9) {
    char Spring[] = "Spring";
    strncpy(colour, Spring, 12);
  } else if (rgbProfile == 10) {
    char Turquoise[] = "Turquoise";
    strncpy(colour, Turquoise, 12);
  } else if (rgbProfile == 11) {
    char Cyan[] = "Cyan";
    strncpy(colour, Cyan, 12);
  } else if (rgbProfile == 12) {
    char SkyBlue[] = "Sky Blue";
    strncpy(colour, SkyBlue, 12);
  } else if (rgbProfile == 13) {
    char Blue[] = "Blue";
    strncpy(colour, Blue, 12);
  } else if (rgbProfile == 14) {
    char Purple[] = "Purple";
    strncpy(colour, Purple, 12);
  } else if (rgbProfile == 15) {
    char Violet[] = "Violet";
    strncpy(colour, Violet, 12);
  } else if (rgbProfile == 16) {
    char Pink[] = "Pink";
    strncpy(colour, Pink, 12);
  } else if (rgbProfile == 17) {
    char Rose[] = "Rose";
    strncpy(colour, Rose, 12);
  } else if (rgbProfile == 18) {
    char White[] = "White";
    strncpy(colour, White, 12);
  }
}


void stopWatch() {
  finish = millis();
  elapsed = finish-start;
  m = int(elapsed / 60000);
  remainder = elapsed % 60000;
  s = int(remainder / 1000);

  tft.setFont(&FreeSans18pt7b);
  tft.setTextSize(2);
  tft.setTextColor(CYAN);
  tft.setCursor(100, 175);
  tft.println(":");

  if (mPrev != m) {
    tft.fillRect(60, 130, 40, 60, BLACK);
    if (m < 10) {
      tft.setCursor(60, 180);
      tft.println(m,0);
    } else if (m > 9) {
      tft.setCursor(20, 180);
      tft.println(m,0);
    }
  }

  if (sPrev != s) {
    tft.fillRect(120, 130, 85, 60, BLACK);
    if (s < 10) {
      tft.setCursor(120, 180);
      tft.println("0");
      tft.setCursor(160, 180);
      tft.println(s,0);
    } else if (s > 9) {
      tft.setCursor(120, 180);
      tft.println(s,0);
    }
  }

  mPrev = m;
  sPrev = s;
  tft.setFont(&FreeMono12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
}
