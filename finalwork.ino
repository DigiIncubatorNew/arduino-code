   /*import necessary libraries*/
#include <SoftwareSerial.h>
#include <dht11.h>
#include <Wire.h>
#include <String.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include "ds3231.h"

const int stepsPerRevolution=200;

Stepper myStepper=Stepper(stepsPerRevolution,8,9,10,11);


//Egg type declarations
struct myEgg{
  float tempLimit;
  float humLimit;
  int  daysInc;
 };

 boolean eggwait=true;
float tempLimit;
float humLimit;
int   daysInc;
String userPhoneNumber;

  //setup different types of  eggs
    myEgg chickEgg={37.6,65,21};
    myEgg quailEgg={37.1,68,18};
    myEgg duckEgg={37.4,67,28};

//timing declarations
struct ts t; 

//lcd
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address


SoftwareSerial mSerial(3,2); //TX,Rx
  
//dht11 declarations
#define DHT11PIN 4
dht11 DHT11;
float temp;
float hum;
float prevtemp=0;
float prevhum=0;  
float lastSMStime=0;

//relay declaration
int relayPin=7;

//reset Arduino function
void (* resetFunc)(void)=0;

//network declarations
  String textMessage;
//setup components
void setup() {
  //general setup
  Serial.begin(9600);
  Wire.begin();  

//setup motor
myStepper.setSpeed(15);
  //lcd setup
    lcd.begin(20,4); 

  //relay setup
   pinMode(relayPin,OUTPUT);

  //handle timing setup
  DS3231_init(DS3231_CONTROL_INTCN);
  t.hour=0; 
  t.min=0;
  t.sec=0;
  t.mday=1;
  DS3231_set(t); 
  
  //handle dht11 data setup
    Serial.println("Welcome To DigiIncubator");
    prevtemp=0;
    prevhum=0;

  //handle network setup
    mSerial.begin(9600);  
    Serial.println("Initializing...");
    delay(1000);
    mSerial.println("AT");
    delay(500);
    //check network connectivity
    
    updateSerial();
    delay(500);
    mSerial.println("AT+CMGF=1");
    delay(500);
    updateSerial();
    delay(500);
    mSerial.println("AT+CNMI=1,2,0,0,0");
    updateSerial();
    delay(3000);
    //Wait For Mobile Application to send Egg Type
    awaitEggType();
    
}

void loop() {
  DS3231_get(&t); 
  //handleRelay Activities
 handleRelay();
 if(t.mday<daysInc){
  // run timing function
  timinghandler();
  
  //run handle sensor data
  handleDhtdata();
  
  }
  else{
    //this is done automatically
    Serial.println("Stop turning");
    //Send  SMS to user telling him Incubation day is up
    delay(1000);
    sendSMS("Hello, Please Your Eggs have been fully Incubated.");
    
    }

 

}


//handle timing and turning
void timinghandler(){
  delay(1000);
  printdate();
  
     if(t.min==59 && t.sec==59){
          delay(2000);
          DS3231_get(&t); 
          int currentsec=t.sec+2;
          t.min=t.min;
          t.hour=t.hour; 
          t.min= t.min;
          t.sec=0;
         
          while(t.sec<5){
            Serial.println("Turn Egg 5 sec");
            //turn motor
            myStepper.step(200);
            Serial.print("Sec: ");
            Serial.println(t.sec);
            DS3231_get(&t); 
            }
           Serial.println(t.sec);
          t.sec=t.sec+currentsec;
          t.min=t.min;
          t.hour=t.hour; 
          t.min= t.min;
          DS3231_set(t);
  }
}
//print timing data
void printdate(){
    Serial.print("Date : ");
    Serial.print(t.mday);
    Serial.print("/");
    Serial.print(t.mon);
    Serial.print("/");
    Serial.print(t.year);
    Serial.print("\t Hour : ");
    Serial.print(t.hour);
    Serial.print(":");
    Serial.print(t.min);
    Serial.print(".");
    Serial.println(t.sec);
  }



//handle sensor data
void handleDhtdata(){
  delay(1000);
  updateSerial();
  readdht();

  //check whether the humidity is low or okay then send sms to user if low
  if(lastSMStime<t.min){
    lastSMStime=t.min+10;
    if(hum<humLimit){
      sendSMS("Please your humidity is low. Please add water to the plate");
      } 
    }
    
  //check whether data changed from last reading
  if(prevtemp!=temp){
    //show to LCD
    displaDetails();
      prevtemp=temp;
      prevhum=hum;
      //Log latest changes to cloud
      Serial.println("Log data to cloud");
      logDataToCloud();
      printdht();
    }
  }

 void printdht(){
      Serial.print("Humidity (%): ");
      Serial.println(hum);
      Serial.print("Temperature (C): ");
      Serial.println(temp);
      delay(2000);
   }

  void readdht(){
      int chk = DHT11.read(DHT11PIN);
      temp=(float)DHT11.temperature;
      hum=(float)DHT11.humidity;
      delay(200);
    }


//handle network requests and responses
 void updateSerial(){
  while(Serial.available()){ 
      mSerial.write(Serial.read());
    }
      
    while(mSerial.available()){
      Serial.write(mSerial.read());
    }
  }

//Generic function for sending sms messages
  void sendSMS(String mystr){
    Serial.println("Sending...");
    delay(1000);
    mSerial.println("AT");
    delay(500);
    updateSerial();
    delay(500);
    mSerial.println("AT+CMGF=1");
    delay(500);
    updateSerial();
    //mSerial.println("AT+CMGS=\"+233553185355\"");
    //Send Message to User Phone Number
    mSerial.println("AT+CMGS=\"" +userPhoneNumber +"\"");

    delay(500);
    updateSerial();
    mSerial.print(mystr);
    delay(500);
    updateSerial();
    mSerial.write(26);
    delay(500);
    }

//handle relay operations
void handleRelay(){
  if(temp<tempLimit){
      Serial.println("Temperature OKAY");
      delay(500);
      digitalWrite(relayPin,HIGH);
    }
    else{
       Serial.println("Temperature Too High");
      printdht();
      digitalWrite(relayPin,LOW);

      }
  delay(500)  ;
  }

//send dht11 data and day of incubation to cloud
 void logDataToCloud(){
  delay(1000);
  
  Serial.println("Sending data to thingspeak channel");
  if (mSerial.available())
    Serial.write(mSerial.read());
 
  mSerial.println("AT");
  delay(1000);
 
  mSerial.println("AT+CPIN?");
  delay(1000);
 
  mSerial.println("AT+CREG?");
  delay(1000);
 
  mSerial.println("AT+CGATT?");
  delay(1000);
 
  mSerial.println("AT+CIPSHUT");
  delay(1000);
 
  mSerial.println("AT+CIPSTATUS");
  delay(2000);
 
  mSerial.println("AT+CIPMUX=0");
  delay(2000);
 
  ShowSerialData();
 
  mSerial.println("AT+CSTT=\"internet\"");//start task and setting the APN,
  delay(1000);
 
  ShowSerialData();
 
  mSerial.println("AT+CIICR");//bring up wireless connection
  delay(3000);
 
  ShowSerialData();
 
  mSerial.println("AT+CIFSR");//get local IP adress
  delay(2000);
 
  ShowSerialData();
 
  mSerial.println("AT+CIPSPRT=0");
  delay(3000);
 
  ShowSerialData();

  //for firebase try port 443 if 80 does not work
  mSerial.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"");//start up the connection
  delay(6000);
 
  ShowSerialData();
 
  mSerial.println("AT+CIPSEND");//begin send data to remote server
  delay(4000);
  ShowSerialData();
  
  String str="GET https://api.thingspeak.com/update?api_key=5B3TQ2FZDUBTOURA&field1=" + String(temp) +"&field2="+String(hum) +"&field3="+ String(t.mday);
  Serial.println(str);
  mSerial.println(str);//begin send data to remote server
  
  delay(4000);
  ShowSerialData();
 
  mSerial.println((char)26);//sending
  delay(5000);//waitting for reply, important! the time is base on the condition of internet 
  mSerial.println();
 
  ShowSerialData();
 
  mSerial.println("AT+CIPSHUT");//close the connection
  delay(100);
  ShowSerialData();

  Serial.println("Data sent to cloud");
  delay(500);
  }

 void ShowSerialData()
{
  while(mSerial.available()!=0)
  Serial.write(mSerial.read());
  delay(5000); 
  
}

//Display Incubator data to Physical user
void displaDetails(){
    lcd.setCursor(0,0);
    lcd.print("***DigiIncubator***");
    lcd.setCursor(0,1);
    String msgtemp="Temperature: "+String(temp)+"C";
    lcd.print(msgtemp);
    lcd.setCursor(0,2);
    String msghum="Humidity: "+String(hum)+"%";
    lcd.print(msghum);
    lcd.setCursor(0,3);
    String msgday;
    if(t.mday<daysInc){
      msgday="Day of incubation:"+String(t.mday);
      }
      else{
        msgday="Eggs fully incubated";
        sendSMS("Eggs fully incubated");
        }
    
    lcd.print(msgday);
  
  }
   void displayWaiting_eggType(String ss){
        lcd.setCursor(0,0);
        lcd.print("***DigiIncubator***");
        lcd.setCursor(0,1);
        lcd.print(ss);
      }

  void awaitEggType(){
    displayWaiting_eggType("Wait For Egg Type...");
    Serial.println("Wait For Egg Type...");
    
    while(eggwait){
    
        while(mSerial.available()>0){
          String eggType=mSerial.readString();
          Serial.println(eggType);
          //Extract important portion from twilio free account message
            delay(500);
            //Here we determine type of egg user is using from sms from user's phone
            if (eggType.indexOf("C_DigiInc")>=0){
              tempLimit=chickEgg.tempLimit;
              humLimit=chickEgg.humLimit;
              daysInc=chickEgg.daysInc;
              Serial.println("Incubating Chicken");
              displayWaiting_eggType("Incubating Chicken");
              eggwait=false;
              break;
            }
           else if (eggType.indexOf("Q_DigiInc")>=0){
              tempLimit=quailEgg.tempLimit;
              humLimit=quailEgg.humLimit;
              daysInc=quailEgg.daysInc;
              Serial.println("Incubating Quail");
              displayWaiting_eggType("Incubating Quail");
              eggwait=false;
              break;
           }
           else if (eggType.indexOf("D_DigiInc")>=0){
              tempLimit=duckEgg.tempLimit;
              humLimit=duckEgg.humLimit;
              daysInc=duckEgg.daysInc;
              Serial.println("Incubating Duck");
              displayWaiting_eggType("Incubating Duck");
              eggwait=false;
              break;
           }
           
           else{
            Serial.println("Invalid Input");
              continue;
           }
          
        delay(500);
      }
    }
  }


   
