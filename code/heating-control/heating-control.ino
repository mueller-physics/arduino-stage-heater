#include <OneWire.h>	    // for the temp. sensor
#include <LedControl.h>	    // for the LED display


float tempTurnOff = -0.5;   // cutoff temp difference for heat
byte sustainPower = 45;     // power to sustain temperature

float tempCur = 0;          // current temp, from sensor
float tempTarget = 37;      // target temperature
float tempDiffFullHeat = 5; // temperature difference at which heater goes to full power 

byte heatPower = 0;	    // heating power, 0..255 (PWM)

// initialize the pins the LED is connected to
LedControl lc1=LedControl(8,9,13,1); 

// set the pin the temp. sensor is connected to
OneWire ds(11);  
     
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
}
     

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

    
int counter=0;

     
void loop(void) {

    // addr of temp sensor
    byte addr[8] = { 0x28, 0x6B, 0x23, 0x5B, 0x8, 0x0, 0x0, 0xFA };
    //byte addr[8] =  { 0x28,  0x6E, 0x4B, 0x5A, 0x08 , 0x00, 0x00, 0xB1 }; 

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
            counter=1;
        }
        if (!digitalRead( A1 )) {
            tempTarget+=0.1;
            lc1.clearDisplay(0);
            writeCurrent();
            writeTarget();
            counter=1;
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
      
      
    // convert to temp      
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
    Serial.println(heatPower);
   
    // rewrite the display
    lc1.clearDisplay(0);
    writeCurrent();
    counter++;
    
    if (counter%2)
      writeHeat(); 
      else
      writeTarget();
   
    // TODO: this really needs a proper way to set the
    // heating power dynamically, probably by looking at
    // the last 5 seconds average and derivative. Will add that
    // soon
 
    // reset the heater
    if (diff>=(tempDiffFullHeat)) {
      heatPower=255;
    } else if (diff<=tempTurnOff) {
      heatPower=0; 
    } else if (diff<0) {
      heatPower = sustainPower;
    } else {
      heatPower = (byte)((diff/tempDiffFullHeat)*(255-sustainPower))+sustainPower ;
    }
    
    
    analogWrite(6, heatPower);
    
}
