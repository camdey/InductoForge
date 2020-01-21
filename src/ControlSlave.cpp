#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <AltSoftSerial.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// AltSoftSerial always uses these pins:
//
// Board          Transmit  Receive   PWM Unusable
// -----          --------  -------   ------------
// Arduino Uno        9         8         10
// Arduino Leonardo   5        13       (none)
// Arduino Mega      46        48       44, 45

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

//------Slave Reader------//
#define masterAddr 0x23
#define slaveAddr 0x24

  const byte VoSensePin = 11;       // digital output pin for VoSense
  const byte fanPin = 3;            // digital pwm output pin for fan control
  const byte ledPin = 13;           // variable for Arduino pin NeoPixel is connected to
  const byte ammeterPin = A2;       // analog input pin for ACS712 current sensor module
  float mlxTemp = 0;                // variable to store the temp value coming from the MLX sensor
  float mlxAmb = 0;                 // variable to store the ambient value coming from the MLX sensor
  float tempActual = 0;             // variable to store temp value with correction factor
  int vosVal = 0  ;                 // pwm value to send to VoSensePin
  int targetTemp = 180;             // variable to store temperature setting value, default 180*C
  int targetMin = 178;              // minimum acceptable temp. range for targetTemp
  int targetMax = 182;              // maximim acceptable temp. range for targetTemp
  int fanVal = 0;                   // pwm value to send to MOSFET gate
  int loopCount = 0;                // variable to altSerial.print diagnostics to phone every 2400ms
  const byte NUM_PIXELS = 12;       // variable for number of NeoPixel LEDs
  uint32_t currentColor;            // current colour array (RGB)
  uint16_t currentPixel = 0;        // what pixel are we operating on
  int currentBrightness = 255;      // variable for storing NeoPixel brightness value
  int rgbProfile = 1;               // variable for storing colorChart colour value
  String colour;                    // string to store name of colour selected from colorChart
  float ammeterVal = 0;             // stores current value (A)
  int relay = 0;
  int relayPin = 2;
  int intRequest = 0;
  int colWipe = 0;
  char buffer[32];
  int bufLen = 10;
  char tempSend[10];
  char ambSend[10];
  char ampSend[10];
  char vosSend[10];
  float prevVal = 0.00;
  float Error = 0;
  float PrevError = 0;
  float Integral = 0;
  float Derivative = 0;
  int Drive = 255;
  int maxDrive = 0;
  float kP = 20.0;                 // Proportional
  float kI = 1.5;                 // Integral
  float kD = 2.0;                 // Derivative
  float lastTemp;
  unsigned long time;
  unsigned long startTime = 0;
  unsigned long endTime = 0;
  boolean flag = true;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, ledPin, NEO_GRB + NEO_KHZ800);

void setup()  {
  Serial.begin(9600);
  Wire.begin(slaveAddr);        // join i2c bus with address #8
  mlx.begin();
  strip.begin();
  Wire.onReceive(receiveEvent); // register event
  Wire.onRequest(requestEvent); // register event
  // change PWM frequency for pin D3 and D11 to 32khz up from 420hz
  setPwmFrequency(3, 1);
  pinMode(VoSensePin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  strip.show(); // Initialize all pixels to 'off'
  // initiate randomSeed on unconnected pin A0
  randomSeed(analogRead(0));
  // start LEDs as red (rgbProfile = 1)
  ColorChart();
}

void loop() {
  loopCount++;
  masterParse();
  ammeterRead();

  time = millis();
  analogWrite(fanPin, fanVal);

  if (colWipe == 1 && time % 35 == 0) {
    colorWipe();
  } else if (colWipe == 0) {
    colorWheel();
  }
  if (relay == 0) {
    digitalWrite(relayPin, LOW);
  } else if (relay == 1) {
    digitalWrite(relayPin, HIGH);
  }


  // read temperature sensor, if above 170 C initate VoSenseMin/Max.
  // allows vosVal to be set to 0 initially to rapidly raise temperature
  mlxTemp = mlx.readObjectTempC();
  mlxAmb = mlx.readAmbientTempC();
  tempActual = tempCorrect(mlxTemp, mlxAmb);

  if (abs(targetTemp - tempActual) < 12 && relay == 1){
   if (time % 100 == 0) {
     PIDControl();
   }
  }
  if ((targetTemp-tempActual) > 12) {
    vosVal = 0;
    if (flag == true) {
      analogWrite(VoSensePin,vosVal);
      flag = false;
    }
  }
  if ((tempActual-targetTemp) > 12) {
    vosVal = 255;
    if (flag == true) {
        analogWrite(VoSensePin,vosVal);
        flag = false;
    }
  }
}


// function for controlling VoSense if current temp is below target temp range
void PIDControl() {
  startTime = millis();
  flag = true;
  float timeDiff = (startTime-endTime)/1000.00;
  Error = targetTemp - tempActual;
  Derivative = (Error - PrevError)/0.1;
    if (vosVal != 255) {
        Integral += (Error*timeDiff);
        if (Integral > 300) {
            Integral = 0;
        }
    }
  Drive = (Error*kP) + (Integral*kI) + (Derivative*kD);
    if (Drive > 255) {
        Drive = 255;
    }
    if (Drive > maxDrive) {
        maxDrive = Drive;
    }
    Drive = constrain(Drive, 0, maxDrive);
    vosVal = map(Drive, 0, maxDrive, 255, 0);
    analogWrite(VoSensePin,vosVal);

  lastTemp = tempActual;
  endTime = startTime;

  Serial.print("Drive: ");
  Serial.println(Drive);

  Serial.print("Proportional: ");
  Serial.println(Error*kP);
  Serial.print("Integral: ");
  Serial.println(Integral*kI);
  Serial.print("Derivative: ");
  Serial.println(Derivative*kD);
  Serial.print("timeDiff: ");
  Serial.println(timeDiff);
}


// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
  for (int i = 0; i < howMany; i++) {
    buffer[i] = Wire.read();
  }
}


void masterParse() {
      if (buffer[0] == 'f') {
          buffer[0] = '0';
          fanVal = atoi(buffer);
          memset(buffer, 0, 32);
      }
      else if (buffer[0] == 't') {
          buffer[0] = '0';
          targetTemp = atoi(buffer);
          memset(buffer, 0, 32);
      }
      else if (buffer[0] == 'l') {
          buffer[0] = '0';
          currentBrightness = atoi(buffer);
          memset(buffer, 0, 32);
      }
      else if (buffer[0] == 'r') {
          buffer[0] = '0';
          relay = atoi(buffer);
          memset(buffer, 0, 32);
      }
      else if (buffer[0] == 'p') {
          buffer[0] = '0';
          rgbProfile = atoi(buffer);
          memset(buffer, 0, 32);
      }
      else if (buffer[0] == 'c') {
          buffer[0] = '0';
          colWipe = atoi(buffer);
          memset(buffer, 0, 32);
      }
      else if (buffer[0] == '0' || buffer[0] == '1' || buffer[0] == '2' || buffer[0] == '3') {
          intRequest = atoi(buffer);
          memset(buffer, 0, 32);
      }

}


// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent() {
    switch (intRequest) {
    case 0:
    tempFunc();
    Wire.write(tempSend);
    break;
    case 1:
    ambFunc();
    Wire.write(ambSend);
    break;
    case 2:
    ampFunc();
    Wire.write(ampSend);
    break;
    case 3:
    vosFunc();
    Wire.write(vosSend);
    break;
  }

  intRequest++;
  if (intRequest > 3) {
      intRequest = 0;
  }
}


void tempFunc() {
  char valArray[10];
  itoa(tempActual, valArray, 10);
  tempSend[0] = 't';

    for (int i = 1; i < bufLen; i++) {
      tempSend[i] = valArray[(i-1)];
    }
}


void ambFunc() {
  char valArray[10];
  int tempAmb = mlxAmb;
  itoa(tempAmb, valArray, 10);
  ambSend[0] = 'a';

    for (int i = 1; i < bufLen; i++) {
      ambSend[i] = valArray[(i-1)];
    }
}


void ampFunc() {
  char valArray[10];
  int ampVal = (ammeterVal*100);
  itoa(ampVal, valArray, 10);
  ampSend[0] = 'c';

    for (int i = 1; i < bufLen; i++) {
      ampSend[i] = valArray[(i-1)];
    }
}


void vosFunc() {
  char valArray[10];
  itoa(vosVal, valArray, 10);
  vosSend[0] = 'v';

    for (int i = 1; i < bufLen; i++) {
      vosSend[i] = valArray[(i-1)];
    }
}


void colorWipe() {
  if (currentPixel == 0) {
       rgbProfile = random(1,19);
  }
  ColorChart();
  strip.setBrightness(currentBrightness);
  strip.setPixelColor(currentPixel,currentColor);
  strip.show();
  currentPixel++;

  if (currentPixel == NUM_PIXELS) {
    currentPixel = 0;
  }
}


void colorWheel() {
  ColorChart();
  strip.setBrightness(currentBrightness);
  strip.setPixelColor(currentPixel,currentColor);
  strip.show();
  currentPixel++;
  if (currentPixel == NUM_PIXELS) {
    currentPixel = 0;
  }
}


void ammeterRead(){
  const int ammeterRef = 512; // 1024/2 = 512
  float ammeterADC = 0.0; // stores analog value
  const float refVol = 2.5; // 5.0/2 = 2.5V
  float readVol = 0; // stores voltage value calculated from ADC
  const float V_ref = 0.066; // 66mV ref, 2.5+(0.066*amps) = readVol
  float V_comp = 0; // difference between readVol and refVol
    ammeterADC = analogRead(ammeterPin);
    readVol = ((ammeterADC/1024.0)*5.0);
    V_comp = (readVol - refVol);
    ammeterVal = (V_comp/V_ref);
}


float tempCorrect(float tempOb, float tempAm){
  const float ems = 0.283;

  if (tempOb > 1000) {
    return prevVal;
  }
  else {
  tempOb = (tempOb*tempOb);
  tempOb = (tempOb*tempOb);
  tempAm = (tempAm*tempAm);
  tempAm = (tempAm*tempAm);
  float result = sqrt(sqrt((((tempOb - tempAm) / ems) + tempAm)));
  prevVal = result;
  return result;
  }
}


void ColorChart(){

  if (rgbProfile == 1){
    colour = "Red";
    currentColor = strip.Color(255,0,0);
  }
  if (rgbProfile == 2){
    colour = "Dark red";
    currentColor = strip.Color(255,60,0);
  }
  else if (rgbProfile == 3){
    colour = "Orange";
    currentColor = strip.Color(255,100,0);
  }
  else if (rgbProfile == 4){
    colour = "Gold";
    currentColor = strip.Color(255,200,0);
  }
  else if (rgbProfile == 5){
    colour = "Yellow";
    currentColor = strip.Color(255,255,0);
  }
  else if (rgbProfile == 6){
    colour = "Lime";
    currentColor = strip.Color(180,255,0);
  }
  else if (rgbProfile == 7){
    colour = "Apple";
    currentColor = strip.Color(120,255,0);
  }
  else if (rgbProfile == 8){
    colour = "Green";
    currentColor = strip.Color(0,255,0);
  }
  else if (rgbProfile == 9){
    colour = "Spring green";
    currentColor = strip.Color(0,255,100);
  }
  else if (rgbProfile == 10){
    colour = "Turquoise";
    currentColor = strip.Color(0,255,170);
  }
  else if (rgbProfile == 11){
    colour = "Cyan";
    currentColor = strip.Color(0,245,255);
  }
  else if (rgbProfile == 12){
    colour = "Sky blue";
    currentColor = strip.Color(0,100,255);
  }
  else if (rgbProfile == 13){
    colour = "Blue";
    currentColor = strip.Color(0,0,255);
  }
  else if (rgbProfile == 14){
    colour = "Purple";
    currentColor = strip.Color(140,0,255);
  }
  else if (rgbProfile == 15){
    colour = "Violet";
    currentColor = strip.Color(210,0,255);
  }
  else if (rgbProfile == 16){
    colour = "Pink";
    currentColor = strip.Color(255,0,200);
  }
  else if (rgbProfile == 17){
    colour = "Rose";
    currentColor = strip.Color(200,25,25);
  }
  else if (rgbProfile == 18){
    colour = "White";
    currentColor = strip.Color(255,255,255);
  }
}


// function for setting arduino pwm frequency, courtesy of:
// http://playground.arduino.cc/Code/PwmFrequency
void setPwmFrequency(int pin, int divisor) {

  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}
