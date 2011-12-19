/*

 pee@sig13.net:

 anenmometer code based on some sample code from these sources:
 the Pachube sensor client code from Tim Igoe and the pachube_anemometer_01 code from pachube
 http://www.tigoe.net/pcomp/code/category/arduinowiring/873
 
 and:
 
 http://openenergymonitor.org/emon/
 
 This code is in the public domain

 -pee
 
 */
#include <SPI.h>
#include <Wire.h>
#include <Ethernet.h>

const unsigned long REPORT_INTERVAL = 60000;
const byte SERVER_PORT = 80;
const int REPLY_LENGTH = 250;

const char CONTENT_TYPE[] = "Content-Type: application/json";
const char HOST[] = "Host: s.erkkila.org";
const char CLOSE[] = "Connection: close";
const char CONTENT_LENGTH[] = "Content-Length: ";
const char HTTP_REPORT[] = "PUT /cgi/sensor.cgi HTTP/1.1";


// ethernet shield mac
byte mac[] = { 0x90,0xA2,0xDA,0x00,0x21,0xAC };
const char MAC[] = "90A2DA0021AC";

// destination host
IPAddress ip( 10,1,1,4);

// gateway address
byte gw[] = { 10,1,1,5 };

// network mask for local network
byte mask[]= { 255,255,255,0 };

// server address,note..make me DNS
IPAddress server ( 10,1,1,18 );

// bmp085
#define BMP085_ADDRESS 0x77  // I2C address of BMP085
int ac1;
int ac2; 
int ac3; 
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1; 
int b2;
int mb;
int mc;
int md;
// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5; 
const unsigned char OSS = 0;  // Oversampling Setting

short temperature;
float ct;
long pressure;


unsigned long previousClicksMillis;
unsigned long timer;
float windSpeed;
int lastClicks;
char replyString[REPLY_LENGTH];

// data is stored here in the interrupt routing
unsigned volatile int clicks;

//Client client(server, SERVER_PORT);
EthernetClient client;


void setup() {

  Serial.begin(9600);
  Wire.begin();
  bmp085Calibration();
    
  // start the ethernet connection and serial port:
  Ethernet.begin(mac, ip);

  // give the ethernet module time to boot up:
  delay(1000);

  attachInterrupt(0, clicker, RISING);

  memset(replyString, '\0', REPLY_LENGTH );

  strcat( replyString, "{ \"method\":\"Ping\"," );
  strcat( replyString, "\"mac\":\"" );
  strcat( replyString, MAC );
  strcat( replyString, "\"}" );
    
  report();

  windSpeed = 0;
  lastClicks = 0;
  timer = millis();
  previousClicksMillis = timer;


}

void loop() {

  if ((millis() - timer) > REPORT_INTERVAL) {

    timer = millis();

    updateValues();
    
    //Serial.println("Getting temp");
    temperature = bmp085GetTemperature(bmp085ReadUT());
    ct = temperature * 0.1;
    //Serial.println("Getting pressure");
    pressure = bmp085GetPressure(bmp085ReadUP());
    
    Serial.print("Pressure: ");
    Serial.print(pressure, DEC);
    Serial.println(" Pa");
    //Serial.println();

    memset(replyString, '\0', REPLY_LENGTH );

    strcat( replyString, "{\"method\":\"Report\",");
    strcat( replyString, "\"sensorID\":\"");
    strcat( replyString, MAC );
    strcat( replyString, "\",");
    strcat( replyString, "\"readings\":[{");
    
    strcat( replyString, "\"type\":\"inspeedD2\",\"units\":\"meters/second\",");
    strcat( replyString, "\"value\":\"");
    strcat( replyString, floatString(windSpeed,4) );
    strcat( replyString, "\"");
    
    strcat( replyString, "},");
  
    strcat( replyString, "{\"type\":\"clicks\",\"units\":\"clicks\",");
    strcat( replyString, "\"value\":\"");
       
    char intPartStr[20];
    if (lastClicks < 0 )    {
      lastClicks=lastClicks*-1;
      strcat(intPartStr, "-");     
    }

    itoa(lastClicks, intPartStr, 10);
    strcat( replyString, intPartStr );       
    
    strcat( replyString, "\"");
    strcat( replyString, "}");
    
    strcat( replyString, "]}");
    
    //Serial.println("Sending report0");
    report();
    
    memset(replyString, '\0', REPLY_LENGTH );
    strcat( replyString, "{\"method\":\"Report\",");
    strcat( replyString, "\"sensorID\":\"");
    strcat( replyString, MAC );
    strcat( replyString, "\",");
    strcat( replyString, "\"readings\":[");
    
    strcat( replyString, "{\"type\":\"temperature\",\"units\":\"celcius\",");
    strcat( replyString, "\"value\":\"");
    strcat( replyString, floatString(ct,2) );
    strcat( replyString, "\"");  
    strcat( replyString, "},");
    
    strcat( replyString, "{\"type\":\"pressure\",\"units\":\"Pa\",");
    strcat( replyString, "\"value\":\"");
    
    memset( intPartStr, 0, 20 );
    ltoa( pressure, intPartStr, 10);
    strcat( replyString, intPartStr );
    strcat( replyString, "\"");  
    strcat( replyString, "}");
    
    
    strcat( replyString, "]}");
    
    //Serial.println("Sending report1");
    report();

  }

}

// this method makes a HTTP connection to the server:
void report() {


  //Serial.println("Report called");
  Serial.println(replyString);
  
  if (client.connect(server, SERVER_PORT)) {

    client.println(HTTP_REPORT);

    client.println(HOST);
        
    client.println(CONTENT_TYPE);
    client.println(CLOSE);
    client.print(CONTENT_LENGTH);
   
    int rLength = strlen(replyString);
    
    // this actually prints this content length
    client.println(rLength, DEC);

    client.println();
    client.println(replyString);

  } else {
    //Serial.println("Was not able to connect client");
  }

  client.stop();
  client.flush();
}

void clicker(){
  clicks++; 
}

void updateValues(){ 

  unsigned long now = millis();

  float windCountTime = (now - previousClicksMillis) / 1000.0;

  windSpeed = 1.1176 * ((float) clicks / windCountTime);

  lastClicks = clicks;
  clicks = 0;

  previousClicksMillis = now;

}

//-----------------------------------------------------------------------------------------------
// Converts a double in to a string
//-----------------------------------------------------------------------------------------------
char * floatString(float value, int precision )
{
   char intPartStr[20];
   char decPartStr[20];
   char doubleStr[20] = "";
   
   if (value<0) 
   {
     value=value*-1;
     strcat(doubleStr, "-");     
   }
   
   int intPart = (int)value;
      
   float decPart = (value-intPart)*(pow(10,precision));

   itoa(intPart, intPartStr, 10);
   itoa((int)decPart, decPartStr, 10);
   
   strcat(doubleStr, intPartStr);   
   strcat(doubleStr, ".");
   strcat(doubleStr, decPartStr);
   return doubleStr;
}














