#include <EEPROM.h>

#include <BufferedAnalog.h>
#include <FlexiTimer2.h>
#include <TimerOne.h>

#include <LiquidCrystal.h>

const unsigned long U_LONG_MAX = 4294967295;

/***********************************************************
* Sweep intervals
************************************************************/
const unsigned long MPPT_SWEEP_INTERVAL = 5L * 60L * 1000L;
const unsigned long NO_CHARGE_SWEEP_INTERVAL = 60L * 1000L;

const unsigned long DISPLAY_REFRESH_INTERVAL = 150; //milliseconds
/**********************************************************
* ADJUST ACCORDING TO BATTERY AND PANEL SPECS
***********************************************************/
//battery AH rating at C20 (20h discharge time)
const float BATTERY_AH_C20 = 60;
const float BATTERY_DEAD_VOLTAGE = 10; // volts at output when the charger considers battery dead -> no charging

const float V_ABS = 14.2; //absorb voltage, bulk charging stops when voltage at the battery reaches this value, charger goes to ABSORB phase
const float V_ABS_TOLERANCE = 0.2; //tolerance on V_ABS following, voltage can be between V_ABS - V_ABS_TOLERANCE and V_ABS + V_ABS_TOLERANCE
const float I_CUTOFF = 0.5; //current where charging stops
const float V_IN_MIN = 15;  //minimum input voltage, charger only starts checking states when voltage goes above this value, at night time it will be mostly in NO_CHARGE_STASTE
const float V_CONSUMER_CUTOFF = 12;

// state of charge table, must be an 10 size array starting from lowest volts corresponding to upper limit of 0% interval, last corresponds to 100%
const float SOC_TABLE[10] = {11.31, 11.58, 11.75, 11.9, 12.06, 12.20, 12.32, 12.42, 12.5, 12.7};

//fan
const int FAN_PIN = 2;
const float FAN_ON_POW = 70;
const float FAN_OFF_POW = 50;

const int NO_CHARGE_STATE = 0;
const int MPPT_CHARGE_STATE = 1; //MPPT state with bulk battery charging, stops when state time expires or voltages reaches ABSORB voltage
const int ABSORB_CHARGE_STATE = 2;


//char* chargeMessage = null;
int chargeState;
unsigned long lastSweepTime = 0;

/**********************************************************
* DO NOT CHANGE
***********************************************************/

const float I_OUTPUT_MAX = 18; //maximum current (20 Amp fuse)
const float V_OUT_MAX = 16; //maximum output voltage
float I_BATTERY_MAX = min(BATTERY_AH_C20 / 6, I_OUTPUT_MAX);

const int PWM_FREQ = 30000;

/**********************************************************
* DISPLAY
***********************************************************/
const byte DISPLAY_VOLT_AMPS = 0;
const byte DISPLAY_POWER = 1;
const byte DISPLAY_STATE_EFF = 2;
const byte MENU_SET_CURRENT = 3;

const byte displayStates[] = {DISPLAY_VOLT_AMPS, DISPLAY_POWER, DISPLAY_STATE_EFF, MENU_SET_CURRENT};
int displayState = DISPLAY_VOLT_AMPS;

//rs, enable, d4, d5, d6, d7
LiquidCrystal lcd(6, 7, 8, 10, 11, 12);
const unsigned long lcdSleepDelay = 60; //seconds
boolean lcdOn = false;
long displayRefreshTime;


//sensors, pwm
int pwmMaxDuty = 1023;

const float i1023 = 1.0 / 1023.0;
float internalVcc = 5.12; //previously measured internally, but prooved unreliable, external measured and set
float vInFactor = ((2.3 + 22.8 + 3.29) / 3.29) * i1023;
float vOutFactor = ((10 + 3.33) / 3.33) * i1023;

const int consumerPin = 1;
const int pwmPin = 9;
const int vInPin = A0;
const int iInPin = A1;
const int vOutPin = A3;
const int iOutPin = A2;
const int button1Pin = 3;
const int button2Pin = 5;
const int button3Pin = 4;
const int lcdBacklightPin = 13;


int duty = 0;
boolean consumerEnabled = false;
boolean maxCurrentChanged = false;

float iIn;
float vIn;
float iOut;
float vOut;
float mpp;

BufferedAnalog iOutRaw(iOutPin);
BufferedAnalog vOutRaw(vOutPin);

BufferedAnalog iInRaw(iInPin);
BufferedAnalog vInRaw(vInPin);

int button1Val = HIGH; // button1
int button2Val = HIGH; // button2
int button3Val = HIGH; // button3

// the setup routine runs once when you press reset:
void setup() {
  
  pinMode(pwmPin, OUTPUT);
  digitalWrite(pwmPin, 0);
  
  pinMode(lcdBacklightPin, OUTPUT);
  
  pinMode(button1Pin, INPUT);
  pinMode(button2Pin, INPUT);
  pinMode(button3Pin, INPUT);
  
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
  
  //pullup
  button1Val = HIGH;
  
  lcd.begin(16, 2);
  //at start the LCD should be on
  lcdWakeUp();
  
  Timer1.initialize(1000000 / PWM_FREQ);
  Timer1.stop();      //stop the counter
  Timer1.restart();   //set the clock to zero

  lcd.print("FREQ ");
  lcd.print((PWM_FREQ/1000));
  lcd.print("kHz");
  lcd.setCursor(0, 1);
  lcd.print("MAX ");
  lcd.print(pwmMaxDuty);
  delay(1500);
   
  lcd.clear();
  lcd.print("VCC INT:");
  lcdPrintFloat(internalVcc, 2);
  delay(1500);
  
  lcd.clear();
  lcd.print("I MAX:");
  
  readMaxCurrent();
  
  lcdPrintFloat(I_BATTERY_MAX, 1);
  delay(1500);
    
  lastSweepTime = 0;
  setNoChargeState();
}

void readMaxCurrent() {
  int ival = EEPROM.read(0);
  if (ival > 0) {
    float value = ival;  
    I_BATTERY_MAX = value / 10;
  }
}

void writeMaxCurrent() {
  if (maxCurrentChanged) {
    float value = I_BATTERY_MAX * 10;
    int ival = value;
    EEPROM.write(0, ival);
    maxCurrentChanged = false;
  }
}

void lcdPrintFloat(float val, byte precision){
  //lcdPrintSerialFloat(val, precision);
  // prints val on a ver 0012 text lcd with number of decimal places determine by precision
  // precision is a number from 0 to 6 indicating the desired decimial places
  // example: lcdPrintFloat( 3.1415, 2); // prints 3.14 (two decimal places)

  if(val < 0.0){
    lcd.print('-');
    val = -val;
  }

  long integral = (long)val;
  lcd.print (integral, DEC);  //prints the integral part
  if( precision > 0) {
    lcd.print("."); // print the decimal point
    unsigned long frac;
    unsigned long mult = 1;
    byte padding = precision -1;
    while(precision--)
        mult *= 10;
    
    frac = (val - integral) * mult;
    
    unsigned long frac1 = frac;
    while( frac1 /= 10 )
       padding--;
    while(  padding--)
       lcd.print("0");
    lcd.print(frac, DEC) ;
  }
}

/************************************************************************
* Powers up the lcd display after it has been turned off
*************************************************************************/
void lcdWakeUp() {
    if (lcdOn == false){
      digitalWrite(lcdBacklightPin, HIGH);      
      lcd.begin(16, 2);
      lcd.clear();
      
      lcdOn = true;
      //register timer to call lcd sleep method
      FlexiTimer2::set(lcdSleepDelay * 1000L, lcdSleep);
      FlexiTimer2::start();
    } else {
          //just push the timer event
          FlexiTimer2::stop();
          FlexiTimer2::start();
    }
}

void lcdSleep() {
    displayState = displayStates[0];
    lcd.noDisplay();
    digitalWrite(lcdBacklightPin, LOW);
    lcdOn = false;
    FlexiTimer2::stop();
}


/**************************************
* IN
**************************************/
float getIIn() {  
  //float offset = - 0.25; //A
  int count = iInRaw.count();
  float result = 0;
  float i148 = 1 / 0.1485;
  float f1 = internalVcc * i1023 * i148;
  float f2 = internalVcc * 0.5 * i148;
  for (int i=0; i<count; i++){
      float value = iInRaw.get(i);
      value = value  * f1 - f2;     
      if (value < 0) {
          value = 0;
      }
      result += (value * value);
  }
  result = result / (float)count;
  result = sqrt(result);
  return result;  
}

float getVIn() {  
  
  int count = vInRaw.count();
  
  float result = 0;
  float vccFact = internalVcc * vInFactor;
  for (int i = 0; i < count; i++) {
      float value = vInRaw.get(i) * vccFact;
      if (value < 0) {
        value = 0;
      }
      result += (value * value);
  }
  result = result / (float)count;
  result = sqrt(result);
  return result;  
}

/**************************************
* OUT
**************************************/
float getIOut() {  
  int count = iOutRaw.count();
  float result = 0;
  float i133= 7.518797;//1.0 / 0.133;
  float f1 = i1023 * internalVcc * i133;
  float f2 = 0.516 * i133;
  for (int i=0; i < count; i++) {
      float value = iOutRaw.get(i) * f1 - f2;
      //value = (value * i1023 * internalVcc - 0.513) / 0.133;
      if (value < 0) {
        value = 0;
      }
      result += (value * value);
  }
  result = result / (float)count;
  result = sqrt(result);
  return result;
}

float getVOut() {  
  int count = vOutRaw.count();
  float result = 0;
  float f = internalVcc * vOutFactor;
  for (int i=0; i < count; i++){
      float value = vOutRaw.get(i) * f;
      //value = (value * internalVcc * vOutFactor);
      if (value < 0) {
          value = 0;
      }
      result += (value * value);
  }
  result = result / (float)count;
  result = sqrt(result);
  return result;
}

void displayVoltsAmps() {  
   lcd.clear();
   lcd.print("IN  ");
   lcdPrintFloat(vIn, 1);
   lcd.print("V");
   lcd.setCursor(10, 0);
   lcdPrintFloat(iIn, 2);
   lcd.print("A");
   
   lcd.setCursor(0, 1);
   lcd.print("OUT ");
   lcdPrintFloat(vOut, 1);
   lcd.print("V");
   lcd.setCursor(10, 1);
   lcdPrintFloat(iOut, 2);
   lcd.print("A");   
}

void displayWatts() {
    float pIn = vIn * iIn;
    float pOut = vOut * iOut;
    //conversion efficiency
    int eff = (int)(pOut/pIn * 100);
    
    lcd.clear();
    lcd.print("PIN  ");
    lcdPrintFloat(pIn, 1);
    lcd.print("W");
    lcd.setCursor(0, 1);
    lcd.print("POUT ");
    lcdPrintFloat(pOut, 1);
    lcd.print("W");
}

void displayEff() {
    float pIn = vIn * iIn;
    float pOut = vOut * iOut;
    int eff = 0;
    if (pIn > 0) {
        //conversion efficiency
        eff = (int)(pOut / pIn * 100);
        if (eff<0){
          eff=0;
        }
        if (eff>100){
          eff=100;
        }
    }
    
    lcd.clear();
    
    lcd.print("EFF:");
    lcd.print(eff);
    lcd.print("%");
    lcd.setCursor(8, 0);
    lcd.print("D:");
    lcd.print(duty);

    lcd.setCursor(0, 1);
    
    if (chargeState == NO_CHARGE_STATE) {
        lcd.print("C:OFF");  //CHARGE OFF
    } else if (chargeState == MPPT_CHARGE_STATE) {
        lcd.print("C:MPPT");  //CHARGE BULK
    } else if (chargeState == ABSORB_CHARGE_STATE) {      
        lcd.print("C:ABS"); //CHARGE ABSORB
    }
    
    lcd.setCursor(8, 1);
    if (consumerEnabled) {
      lcd.print("CONS:ON");
    } else {
      lcd.print("CONS:OFF");
    }
    
}

void readSensorsCycle() {
  for (int i = 0; i < BUFFER_SIZE; i++){
      readSensors();
  }
  getSensorValues();
}


void readSensors() {
  delayMicroseconds(100);
  iOutRaw.read();
  delayMicroseconds(100);
  iInRaw.read();
  delayMicroseconds(100);
  vOutRaw.read();
  delayMicroseconds(100);
  vInRaw.read();  
}

void getSensorValues(){
  iIn = getIIn();
  vIn = getVIn();

  iOut = getIOut();
  vOut = getVOut();
}

void onKey1() {
   if (lcdOn == false) {
     lcdWakeUp();       
   } else {
       writeMaxCurrent();
       lcdWakeUp();
       displayState ++;
       if (displayState >= sizeof(displayStates)) {
          displayState = 0;
       }
   }
   displayInfo();
}

void onKey2() {
  if (displayState == MENU_SET_CURRENT) {
      lcdWakeUp();
      I_BATTERY_MAX -= 0.2;
      if (I_BATTERY_MAX < 1) {
          I_BATTERY_MAX = 1;
      }
      maxCurrentChanged = true;
  } 
}


void onKey3() {
  if (displayState == MENU_SET_CURRENT) {
      lcdWakeUp();
      I_BATTERY_MAX += 0.2;
      if (I_BATTERY_MAX > I_OUTPUT_MAX) {
          I_BATTERY_MAX = I_OUTPUT_MAX;
      }
      maxCurrentChanged = true;
  }    
}

void readKeys() {
  
  int newVal = digitalRead(button1Pin);
  //button pressed
  if (button1Val == HIGH && newVal==LOW) {
    onKey1();
  }
  button1Val = newVal;
  
  newVal = digitalRead(button2Pin);
  if (button2Val == HIGH && newVal==LOW) {
    onKey2();
  }
  button2Val = newVal; 
  
  newVal = digitalRead(button3Pin);  
  if (button3Val == HIGH && newVal==LOW) {
    onKey3();
  }
  button3Val = newVal;
}


void displaySOC() {
    lcd.clear();
    lcd.print("BATTERY ...%");
    //make sure duty is zero (no charging), discharge should also be stopped
    if (duty != 0){
        Timer1.setPwmDuty(pwmPin, 0);
        delay(10000);
    }
    readSensorsCycle();
    
    int soc = 10;
    for (int i=0; i<10; i++){
      if (vOut <= SOC_TABLE[i]){
        soc = i;
        break;
      }
    }
    
    lcd.clear();
    lcd.print("BATTERY ");
    lcd.print(soc * 10);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("VOLTS ");
    lcdPrintFloat(vOut, 2);
    delay(5000);
    //restore duty
    Timer1.setPwmDuty(pwmPin, duty);
}

void displaySetCurrent() {
    lcd.clear();
    lcd.print("I MAX:");
    lcdPrintFloat(I_BATTERY_MAX, 1);
}

void displayInfo() {
    if (lcdOn) {
        getSensorValues();
        
        if (displayState == DISPLAY_VOLT_AMPS) {
            displayVoltsAmps();
        } else if (displayState == DISPLAY_POWER) {
            displayWatts();
        } else if (displayState == DISPLAY_STATE_EFF) {
            displayEff();
        } else if (displayState == MENU_SET_CURRENT) {
            displaySetCurrent();
        }
    }
}

/*
* Determine state based on current output current and voltage
*/
int getNextChargeState() {
    int nextState = chargeState;
    if (iOut < I_CUTOFF || vIn < V_IN_MIN || vOut >= V_OUT_MAX) {
        nextState = NO_CHARGE_STATE;
    } else if (vOut >= V_ABS) {
        nextState = ABSORB_CHARGE_STATE;        
    } else if (vOut < V_ABS - V_ABS_TOLERANCE) {
        nextState = MPPT_CHARGE_STATE;
    }
    
    return nextState;
}

//float mpp = 0;
float mppSensitivity = 1; //when power changes this much compared to previous value consider the change
int mpptStep = 10;

const float I_PROP_RESPONSE = 10;
const float V_PROP_RESPONSE = 20;

boolean ivPropControl() {
    float dutyResponse = 0;
    int iDutyResponse = 0;
    
    if (iOut > I_BATTERY_MAX) {
        float err = iOut - I_BATTERY_MAX;
        dutyResponse = err * I_PROP_RESPONSE;        
    }
    
    if (vOut > V_ABS + V_ABS_TOLERANCE) {
        float err = vOut - (V_ABS + V_ABS_TOLERANCE);
        dutyResponse = err * V_PROP_RESPONSE;
    }
    
    if (duty > 0 && dutyResponse >= 1){
         iDutyResponse = dutyResponse;
         duty -= iDutyResponse;
         if (duty < 0) {
           duty = 0;  
         }
         Timer1.setPwmDuty(pwmPin, duty);
         return true;
    }
    return false;
}

void mpptControl() {
    readSensorsCycle();
    //check limits on current and voltage and determine next state
    int nextChargeState = getNextChargeState();
    if (nextChargeState == MPPT_CHARGE_STATE) {
      boolean limit = ivPropControl();
      if (limit) {
        mpptStep = -abs(mpptStep);
      } else {
          float pIn = vIn * iIn;
          if (pIn < mpp - mppSensitivity) {
              mpptStep = -mpptStep;
              mpp = pIn;
          } else if ( pIn > mpp ) {
              mpp = pIn;
          }
      }      
    } else if (nextChargeState == NO_CHARGE_STATE) {
       setNoChargeState();
       return;
    } else if (nextChargeState == ABSORB_CHARGE_STATE) {
       setAbsChargeState();
       return;
    }
    
    duty += mpptStep;
    //clamp
    if (duty <= 0) {
        duty = 0;
    }
    if (duty > pwmMaxDuty) {
        duty = pwmMaxDuty;
    }
    Timer1.setPwmDuty(pwmPin, duty);
    //give it time to stabilize
    delay(10);
}

/*
* Tries to maintain VOUT as close as possible to V_ABS but not pass it
*/
void absControl() {
  readSensorsCycle();
  int nextChargeState = getNextChargeState();
  if (nextChargeState == NO_CHARGE_STATE) {
     setNoChargeState();
     return;
  } else if (nextChargeState == MPPT_CHARGE_STATE) {
     setMpptChargeState();
     return;
  }
  //safety precaution
  boolean limit = ivPropControl();
  if (limit) {
      return;
  }
  if (vOut < V_ABS) {
      if (duty < pwmMaxDuty){
        duty ++;
        Timer1.setPwmDuty(pwmPin, duty);
      }
  }
}

void sweep(int startDuty, int endDuty, int increment) {
    float pMax = 0;
    float iMax = 0;

     //increment duty, read input power and keep the maximum power point duty
    for (int d = startDuty; d < endDuty; d += increment){
         Timer1.setPwmDuty(pwmPin, d);
         //give it time to settle
         delay(20);        
         readSensorsCycle();

         if (lcdOn) {
             lcd.clear();
             lcd.setCursor(0, 0);
             lcd.print("SCAN ");
             lcd.print(duty);
             lcd.print(" ");
             lcd.print(d);
             lcd.setCursor(0, 1);
             lcd.print("Vi ");
             lcdPrintFloat(vIn, 1);
             lcd.setCursor(8, 1);
             lcd.print("Ii ");
             lcdPrintFloat(iIn, 1);
         }
         
         if (vIn < V_IN_MIN || vOut >= V_OUT_MAX) {
             break;
         } else if (vOut >= V_ABS + V_ABS_TOLERANCE) {
             break;
         } else if (iOut >= I_BATTERY_MAX) {
             break;
         }
         
         float p = vIn * iIn;
         if (p > pMax){
             duty = d;
             pMax = p;
         }
         if (iOut > iMax){
             iMax = iOut;
         }
    }
    
    mpp = pMax;
    Timer1.pwm(pwmPin, duty);
    
    delay(50);
    readSensorsCycle();
    
    setChargeState(getNextChargeState());
    
}

/*
* Sweeps the duty spectrum and tries to determine the best state to be in. 
*/
void sweep() {
    Timer1.restart();
    Timer1.start();
    Timer1.pwm(pwmPin, duty);
    lastSweepTime = millis();
    int coarseSweepInterval = pwmMaxDuty / 25;
    sweep(0, pwmMaxDuty, coarseSweepInterval);
      //refine search
    if (chargeState != NO_CHARGE_STATE){
        int startDuty = duty - coarseSweepInterval;
        int endDuty = duty + coarseSweepInterval;
        if (startDuty < 0) {
            startDuty = 0;
        }
        if (endDuty > pwmMaxDuty){
            endDuty = pwmMaxDuty;
        }
        sweep(startDuty, endDuty, 1);
    }
}

long timeDiff(unsigned long ref) {
  unsigned long current = millis();
  if (ref <= current){
      return current - ref;
  } else {
     return (U_LONG_MAX - ref) + current;
  }
}

void setChargeState(int cs) {
  if (cs==MPPT_CHARGE_STATE) {
      setMpptChargeState();
  } else if (cs==ABSORB_CHARGE_STATE) {
      setAbsChargeState();
  } else if (cs==NO_CHARGE_STATE) {
      setNoChargeState();
  }
}

void setMpptChargeState() {  
  if (chargeState != MPPT_CHARGE_STATE) {
      Timer1.restart();      //stop the counter
      Timer1.start();
      Timer1.pwm(pwmPin, duty);
      chargeState = MPPT_CHARGE_STATE;      
  }
}


void setAbsChargeState() { 
  if (chargeState!=ABSORB_CHARGE_STATE) {
      Timer1.restart();      //stop the counter
      Timer1.start();
      Timer1.pwm(pwmPin, duty);
      chargeState = ABSORB_CHARGE_STATE;
  }
}

void setNoChargeState() {  
   duty = 0;
   Timer1.pwm(pwmPin, duty);
   Timer1.stop();      //stop the counter
   chargeState = NO_CHARGE_STATE;      
}

void updateConsumer() {
  if (vOut < V_CONSUMER_CUTOFF){
    consumerEnabled = false;
  }else {
    consumerEnabled = true;
  }
  if (consumerEnabled) {
      digitalWrite(consumerPin, LOW);
  } else {
      digitalWrite(consumerPin, HIGH);
  }
}

void updateFan() {
    float pIn = vIn * iIn;
    if (pIn > FAN_ON_POW) {
        digitalWrite(FAN_PIN, LOW);
    } else if (pIn < FAN_OFF_POW) {
        digitalWrite(FAN_PIN, HIGH);
    }
}

// the loop routine runs over and over again forever:
void loop() {
    readKeys();
    readSensors();
    getSensorValues();
    updateConsumer();
    updateFan();
    
    if (lcdOn) {
      if (timeDiff(displayRefreshTime) > DISPLAY_REFRESH_INTERVAL){
          displayRefreshTime = millis();
          displayInfo();
      }  
    }
    
    if (vOut < V_OUT_MAX && vIn >= V_IN_MIN) {
            if (chargeState == NO_CHARGE_STATE && vOut > BATTERY_DEAD_VOLTAGE && (timeDiff(lastSweepTime) > NO_CHARGE_SWEEP_INTERVAL)) {
                sweep();
            } else if (chargeState == MPPT_CHARGE_STATE) {
                if (timeDiff(lastSweepTime) > MPPT_SWEEP_INTERVAL) {
                    sweep();
                } else {
                    mpptControl();
                }
            } else if (chargeState == ABSORB_CHARGE_STATE) {
                absControl();
            }
    } else { 
        setNoChargeState();
    }
}



