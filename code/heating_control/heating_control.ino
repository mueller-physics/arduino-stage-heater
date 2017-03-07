#include <OneWire.h>	    // for the temp. sensor
#include <LedControl.h>	    // for the LED display


// ------------------------------------
// Variables that control the temperature regulation
// ------------------------------------

float tempTarget = 37;      // target temperature
float tempDiffFullHeat = 5; // temperature difference at which heater goes to full power 
float tempDiffCutoff = 2;   // temperature difference at which heater turns off completely
byte  startPower = 20;	    // power the heater will use when entering temperature range
byte  averagePoints = 15;   // over how many measurements (1 per second) to average

// ------------------------------------
// Pin and address settings
// ------------------------------------

// initialize the pins the LED is connected to
LedControl lc1=LedControl(8,9,13,1); 

// set the pin the temp. sensor is connected to
OneWire ds(11);  

// set the address of the temp sensor (TODO: auto-detect on start-up)
byte addr[8] = { 0x28, 0x6B, 0x23, 0x5B, 0x8, 0x0, 0x0, 0xFA };
//byte addr[8] =  { 0x28,  0x6E, 0x4B, 0x5A, 0x08 , 0x00, 0x00, 0xB1 }; 


// ------------------------------------
// Runtime variables
// ------------------------------------

float	tempCur = 0;	    // current temp, from sensor
byte	heatPower = 0;	    // heating power, 0..255 (PWM)

int counter=0;		    // counting for n averages
int displayCounter=0;	    // counting between display modes
boolean outOfRange = false; // heater was out of range

float	tempAverage =0;	    // temp average over last n measurements
float	tempDerivative =0;  // temp derivative over last n measurements

// ------------------------------------
// Setup function
// ------------------------------------
     
void setup(void) {
      
    Serial.begin(9600);
      
    // output pin for the heater
    pinMode(6, OUTPUT);   

    // input pin for the switches
    pinMode( A0, INPUT_PULLUP);
    pinMode( A1, INPUT_PULLUP);

    // setup of the display
    lc1.shutdown(0,false);
    lc1.setIntensity(0, 1);

    // set the starting value for heat PWM
    heatPower = startPower;
    analogWrite(6, heatPower);
}
     

// ------------------------------------
// Display output functions
// ------------------------------------
     

// update the current temperature
void writeCurrent() {
    lc1.setDigit(0,0, (int)(tempCur*10)%10 , false);
    lc1.setDigit(0,1, (int)(tempCur)%10 , true);
    lc1.setDigit(0,2, (int)(tempCur/10)%10 , false);
}
          

// update the target temperature display
void writeTarget() {
    lc1.setDigit(0,4, (int)(tempTarget*10)%10 , false);
    lc1.setDigit(0,5, (int)(tempTarget)%10 , true);
    lc1.setDigit(0,6, (int)(tempTarget/10)%10 , false);
}

// update the current heating power display
void writeHeat() {
      
    byte power = (byte)(heatPower*100./255.); //(short)(heatPower*100)/256;
      
    lc1.setDigit(0,5, (power)%10 , false);
    lc1.setDigit(0,6, (power/10)%10 , false);
    lc1.setDigit(0,7, (power/100)%10 , false);
    lc1.setChar(0,4,'h',false);
}


// ------------------------------------
// Change the power, avoiding overflows
// ------------------------------------

void changeHeatPower( float p ) {
    float newHeatPower = p + heatPower;
    heatPower = (byte)newHeatPower;
    if ( newHeatPower >= 255 ) heatPower = 255;
    if ( newHeatPower <= 0 )   heatPower = 0;
    analogWrite(6, heatPower);
}


// ------------------------------------
// Main loop
// ------------------------------------
    

// true as soon as tempCur exceeded (tempTarget+tempDiffCutoff)
// until tempCut below tempTarget
boolean wasOverheated = false; 
     
void loop(void) {


    // setup and start the temp sensor
    ds.reset();
    ds.select(addr);
    ds.write(0x44,1);         
    
    // idle for 1000ms, reading out the switches in between
    for (byte i=0; i<10; i++) {
        
       
	if (!digitalRead( A0 )) {
	    tempTarget-=0.1;
            lc1.clearDisplay(0);
            writeCurrent();
            writeTarget();
            counter=0;
	    displayCounter=0;
        }
        if (!digitalRead( A1 )) {
            tempTarget+=0.1;
            lc1.clearDisplay(0);
            writeCurrent();
            writeTarget();
            counter=0;
	    displayCounter=0;
        }
       
        delay(100);
    }
 
    // read out the temp sensor
    byte present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         
    
    byte data[12];
    for ( byte i = 0; i < 9; i++) {           
	data[i] = ds.read();
    }  
      
      
    // convert to float in deg Celsius 
    byte LowByte = data[0];
    byte HighByte = data[1];
    uint32_t TReading = (HighByte << 8) + LowByte;
    byte SignBit = TReading & 0x8000;  // test most sig bit

    tempCur = (TReading*0.06250);

    float diff = - tempCur + tempTarget;

         
    // output to serial
    Serial.print("T_current: ");
    Serial.print( tempCur );
    Serial.print(" T_target: ");
    Serial.print( tempTarget );
    Serial.print(" T_diff: ");
    Serial.print( diff );

    Serial.print(" heatPower: ");
    Serial.print(heatPower);
   
    // rewrite the display
    lc1.clearDisplay(0);
    writeCurrent();
    
    if ((displayCounter++)%2) {
      writeHeat(); 
    } else {
      writeTarget();
    }
    
  
/** Simple feedback loop:
 *
 *  [1] tempCur < ( tempTarget - tempDiffFullHeat ) 
 *	- heat with full power
 *  
 *  [2] tempCur > ( tempTarget + tempDiffCutoff )   
 *	- turn off completely, wait for temperature to drop below target temp
 *
 *  [3] (tempTarget - tempDiffFullHeat ) < tempCur < (tempTarget + tempDiffCutoff)
 *	- average over 5 seconds
 *	- increase or decrease power, depending on
 *	  both average temperature and derivative
 * */

    // 1 - run at full power
    if ( tempCur < (tempTarget - tempDiffFullHeat )) {
	heatPower = 255;
	Serial.println(" # Now running full power");

	// set heater
	counter = 0;
	outOfRange = true;
	analogWrite(6, heatPower);
	return;
    }

    // 2 - turn off completely
    if (( tempCur > (tempTarget + tempDiffCutoff) ) || ( wasOverheated )) {
	heatPower = 0;
	Serial.println(" # Now turning off");
    	
	// keep track of the cooldown, keep heat off until below target temp
	if (tempCur <= tempTarget ) {
	    wasOverheated = false;
	} else {
	    wasOverheated = true;
	}
	
	// set heater
	counter = 0;
	outOfRange = true;
	analogWrite(6, heatPower);
	return;
    }


    // 3 - regulate the heater
    
    // reset on restart
    if (counter==0) {
	tempAverage	= tempCur;
	tempDerivative  = tempCur;
	if (outOfRange) {
	    Serial.println(" # Re-entered regulated range");
	    heatPower = startPower;
	    analogWrite(6, heatPower);
	    outOfRange = false;
	}

	counter++;
	Serial.println(" # Starting average");
	return;
    }

    // average up values
    if (counter<averagePoints) {
	tempAverage += tempCur;
	counter++;
	Serial.println(" # Averaging");
	return;
    }   

    // set new heating power (counter has reached 'averagePoints')
    counter=0;
    tempAverage /= averagePoints;
    tempDerivative = tempCur - tempDerivative;
    
    float diffToTarget = tempAverage - tempTarget;
    float powerChange = 0;

    // output data
    Serial.println(" ");
    Serial.print("T_avr: ");
    Serial.print(tempAverage);
    Serial.print(" T_deriv: ");
    Serial.print(tempDerivative);
    Serial.print(" T_diff: ");
    Serial.print(diffToTarget);
    //Serial.print(" PWR_change: ");
    //Serial.print(powerChange);
    Serial.println(" ");


    float minMoveSpeed = ( fabs(diffToTarget) > .3 )?(0.1):(0);

    // if the temperature inc. by more than 1 deg., greatly change power
    if ( tempDerivative > 1 ) {
	changeHeatPower(-15);
	Serial.println("# temp increased by more than 1 deg, reducing power");
	return;
    } 

    // if the temperature inc. by more than 1 deg., greatly change power
    if ( tempDerivative < -1 ) {
	Serial.println("# temp decreased by more than 1 deg, upping power");
	changeHeatPower(10);
	return;
    } 

    // if temp below target, and also going down, increase power
    if ( diffToTarget < 0 && tempDerivative < minMoveSpeed ) {
	Serial.println("# below target, but temp didn't go up, increasing power");
	changeHeatPower(-5*diffToTarget);
	return;
    }

    // if temp above target, and also going up, decrease power
    if ( diffToTarget > 0 && tempDerivative > -minMoveSpeed ) {
	Serial.println("# above target, but temp didn't go down, decreasing power");
	changeHeatPower(-5*diffToTarget);
	return;
    }

    Serial.println("# All seems o.k., not changing power");
    
}
