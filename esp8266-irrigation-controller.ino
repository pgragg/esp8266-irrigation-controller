//
// Irrigation controller with bare JSON and URL parser.
//
// WIFI based on ESP8266_Shield_Demo.h SparkFun ESP8266 AT library - Demo
// Jim Lindblom @ SparkFun Electronics
// Original Creation Date: July 16, 2015 https://github.com/sparkfun/SparkFun_ESP8266_AT_Arduino_Library
//
// Copyright (c) Nick Strauss strauss@positive-internet.com  -- Sat Apr 30 16:20:23 PDT 2016
// All use subject to the GPL GNU GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
// See http://www.gnu.org/licenses/gpl.txt
// Distributed as-is; no warranty is given.
//

//////////////////////
// Library Includes //
//////////////////////
// SoftwareSerial is required (even you don't intend on
// using it).
#include <SoftwareSerial.h> 
#include <SparkFunESP8266WiFi.h>
#define DEBUG 1
/*
 * GPIO relays to solenoids
 */
#define RELAY_A     12
#define RELAY_B     13
#define LINK_LIGHT  13
/*
 * IRRIGATOR STATE
 */
enum { IRRIGATE_UNK = 0,
       IRRIGATE_ON  = 1,
       IRRIGATE_OFF = 2,
};

enum { IRRIGATOR_UNK = 0,
       IRRIGATOR_A = 1,
       IRRIGATOR_B = 2
};
int irrigators[3] = {IRRIGATE_OFF};

/*
 * URL 
 */
enum { ISGET, ISPOST, ISPUT};
int urlType;

/*
 *  WiFi Network Definitions
 */
// Replace these two character strings with the name and
// password of your WiFi network.
const char mySSID[] = "bullwinkle";
const char myPSK[] = "anypassword";

char json[128] = {0}, uson[128]={0};

/*
 * ESP8266Server definition
 */
ESP8266Server server = ESP8266Server(80);

/*
 * HTTP Strings
 */
const String htmlHeader = "HTTP/1.1 200 OK\n"
                          "Connection: close\n"
                          "Content-Type: application/json\n\n";
 
// required Arduino
void setup() 
{
  // initialize digital pins 12 & 13 as an output.
  pinMode(RELAY_A, OUTPUT);
  pinMode(LINK_LIGHT, OUTPUT);
//   digitalWrite(LINK_LIGHT, LOW);  

  // Serial Monitor is used for control
  // debug information.
  Serial.begin(9600);
  serialTrigger(F("Press any key to begin."));

  // initializeESP8266() verifies communication with the WiFi
  // shield, and sets it up.
  initializeESP8266();

  // connectESP8266() connects to the defined WiFi network.
  connectESP8266();

  // displayConnectInfo prints the Shield's local IP
  // and the network it's connected to.
  displayConnectInfo();
  
  serialTrigger(F("Press any key to test server."));
  serverSetup();
}

void loop() 
{
  serverDemo();
  maintain_relays();
}

void initializeESP8266()
{
  // esp8266.begin() verifies that the ESP8266 is operational
  // and sets it up for the rest of the sketch.
  // It returns either true or false -- indicating whether
  // communication was successul or not.
  // true
  int test = esp8266.begin();
  if (test != true)
  {
    Serial.println(F("Error talking to ESP8266."));
    errorLoop(test);
  }
  Serial.println(F("ESP8266 Shield Present"));
}

void connectESP8266()
{
  // The ESP8266 can be set to one of three modes:
  //  1 - ESP8266_MODE_STA - Station only
  //  2 - ESP8266_MODE_AP - Access point only
  //  3 - ESP8266_MODE_STAAP - Station/AP combo
  // Use esp8266.getMode() to check which mode it's in:
  int retVal = esp8266.getMode();
  if (retVal != ESP8266_MODE_STA)
  { // If it's not in station mode.
    // Use esp8266.setMode([mode]) to set it to a specified
    // mode.
    retVal = esp8266.setMode(ESP8266_MODE_STA);
    if (retVal < 0)
    {
      Serial.println(F("Error setting mode."));
      errorLoop(retVal);
    }
  }
  Serial.println(F("Mode set to station"));

  // esp8266.status() indicates the ESP8266's WiFi connect
  // status.
  // A return value of 1 indicates the device is already
  // connected. 0 indicates disconnected. (Negative values
  // equate to communication errors.)
  retVal = esp8266.status();
  if (retVal <= 0)
  {
    char ATversion[32], SDKversion[32], compileTime[32];
    Serial.print(F("Connecting to "));
    Serial.println(mySSID);
    esp8266.getVersion(ATversion, SDKversion, compileTime);
    Serial.println(ATversion);
    Serial.println(SDKversion);
    Serial.println(compileTime);
    
    esp8266.getAP(compileTime);
    Serial.println(compileTime);
    
    // esp8266.connect([ssid], [psk]) connects the ESP8266
    // to a network.
    // On success the connect function returns a value >0
    // On fail, the function will either return:
    //  -1: TIMEOUT - The library has a set 30s timeout
    //  -3: FAIL - Couldn't connect to network.
    retVal = esp8266.connect(mySSID, myPSK);
    if (retVal < 0)
    {
      Serial.println(F("Error connecting"));
      errorLoop(retVal);
    }
  }
}

void displayConnectInfo()
{
  char connectedSSID[24];
  memset(connectedSSID, 0, 24);
  // esp8266.getAP() can be used to check which AP the
  // ESP8266 is connected to. It returns an error code.
  // The connected AP is returned by reference as a parameter.
  int retVal = esp8266.getAP(connectedSSID);
  if (retVal > 0)
  {
    Serial.print(F("Connected to: "));
    Serial.println(connectedSSID);
  }

  // esp8266.localIP returns an IPAddress variable with the
  // ESP8266's current local IP address.
  IPAddress myIP = esp8266.localIP();
  Serial.print(F("My IP: ")); Serial.println(myIP);
}


void serverSetup()
{
  // begin initializes a ESP8266Server object. It will
  // start a server on the port specified in the object's
  // constructor (in global area)
  server.begin();
  Serial.print(F("Server started! Go to "));
  Serial.println(esp8266.localIP());
  Serial.println();
}

void serverDemo()
{
  // available() is an ESP8266Server function which will
  // return an ESP8266Client object for printing and reading.
  // available() has one parameter -- a timeout value. This
  // is the number of milliseconds the function waits,
  // checking for a connection.
  ESP8266Client client = server.available(500);
  
  if (client) 
  {
    debug("Client Connected!");
    digitalWrite(LINK_LIGHT, HIGH);
    maintain_relays();
    
    // an http request ends with a blank line
    boolean           currentLineIsBlank = true;
    char             *jptr = (char*)json;
    char             *uptr = (char*)uson;
    boolean           inJSON = false, inURL = false;  
    int               JSONlevel = 0;
    int               irrigator;
    
    while (client.connected()) 
    {   
       if (client.available()) 
      {
 
        char c = client.read();
        maintain_relays();

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
       if (inJSON){
         *jptr++ = c;
         // end JSON and end client
         if (c == '}'){
            int turnOnOff;

           json_scan(irrigator, turnOnOff);
//           String  dbug = json;
//           dbug += "|irrigator";
//           dbug += String(irrigator);
//           dbug += "turnOnOff";
//           dbug += String(turnOnOff);
//           Serial.println(dbug);
 
          // parse json for [irrigatorA|irrigatorB]  : [ON|OFF] 
          client.print(htmlHeader);
          if (turnOnOff == IRRIGATE_ON) irrigators[irrigator] = IRRIGATE_ON;
          else 
          if (turnOnOff == IRRIGATE_OFF) irrigators[irrigator] = IRRIGATE_OFF;

          if (irrigators[irrigator]==IRRIGATE_ON) client.print(F("on\n\n"  )); 
          else if  (irrigators[irrigator]==IRRIGATE_OFF) client.print(F("off\n\n"  )); 
          else client.print(F("unknown\n\n"  )); 
          // turn on/off the irrigation relays
          maintain_relays();
         }
       }
       else if (inURL){
        *uptr++ = c;

        if (c == '\n' || c == '\r'){ 
          if (!url_scan(irrigator)){ Serial.println("bailing out"); break; }
          // Processing as a GET
          if (urlType == ISGET){
             client.print(htmlHeader);
             if (irrigators[irrigator]==IRRIGATE_ON) client.print(F("on\n\n"  )); 
            else if  (irrigators[irrigator]==IRRIGATE_OFF) client.print(F("off\n\n"  )); 
            else client.print(F("unknown\n\n"  )); 
            break;
          }
//          Serial.print(F("finished URL"));
          inURL = false;
        }
//        else Serial.print("'");
       }
       // entering JSON
       if (c == '{' && currentLineIsBlank){
         
         *jptr++ = c;
         inJSON = true;
         inURL = false;
         JSONlevel++;
       }
       else if (c == '+' && currentLineIsBlank)
        {
          inURL = true;
//          Serial.print(F("inURL"));
        }
       else if (c == '\n' && currentLineIsBlank) 
        {
//           Serial.println(F("STANZA"));
        }
         if (c == '\n') 
        {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') 
        {
          currentLineIsBlank = false;
        }
      }
    }

    // give the web browser time to receive the data
    delay(1);
   
    // close the connection:
    client.stop();
    debug("Client disconnected");
    digitalWrite(LINK_LIGHT, LOW);

  }
  
}
void maintain_relays()
{
  // RELAY A
  if (irrigators[IRRIGATOR_A]==IRRIGATE_ON) 
    digitalWrite(RELAY_A, HIGH);
  else digitalWrite(RELAY_A, LOW);  

//  // RELAY B
//  if (irrigators[IRRIGATOR_A]==IRRIGATE_ON) 
//    digitalWrite(RELAY_B, HIGH);
//  else digitalWrite(RELAY_B, LOW);  
}
  
boolean url_scan(int &irrigator)
{
  char            *uptr;
  
//  Serial.println(uson);
  if (strncmp("IPD", uson,3)!=0) return false;
//  Serial.println("IPDOK");
  
  uptr = uson;
  while (uptr){
    if (*uptr++ == ':'){

      if (strncmp(uptr, "GET",3)==0){ uptr+=4; urlType = ISGET; }
      else
      if (strncmp(uptr, "POST", 4)==0){ uptr +=5; urlType = ISPOST; }
      else
      if (strncmp(uptr, "PUT", 2)==0){ uptr +=4; urlType = ISPUT; }
      else return false;

      if (strncmp(uptr, "/irrigatorA", 11)==0) irrigator = IRRIGATOR_A;
      else if (strncmp(uptr, "/irrigatorB",11)==0) irrigator = IRRIGATOR_B;
      else irrigator = IRRIGATOR_UNK;

      break;
    }
  }
//  Serial.println(F("URLSCAN"));
//   Serial.println("irrigator");
//  Serial.println(irrigator);
//  Serial.println("urltype");
//  Serial.println(urlType);
  return true;
}

// parse json for [irrigatorA|irrigatorB]  : [ON|OFF] 
// { "irrigatorA" : "on" }
// { "irrigatorB" : "on" }
void json_scan(int &irrigator, int &turnOnOff)
{
  String      str = json;
  
  if (str.startsWith("irrigatorA",3)) irrigator = IRRIGATOR_A;
  else if (str.startsWith("irrigatorB",3)) irrigator = IRRIGATOR_B;
  else  irrigator = IRRIGATOR_UNK;
  
  if (str.startsWith("on",18)) turnOnOff = 1;
  else if (str.startsWith("off",18)) turnOnOff = 2;
  else if (str.startsWith("query",18)) turnOnOff = 3;
  else turnOnOff = 0;
}

void debug(char *message)
{
#ifdef DEBUG
  Serial.println(message);
#endif
}

// errorLoop prints an error code, then loops forever.
void errorLoop(int error)
{
  Serial.print(F("Error: ")); Serial.println(error);
  Serial.println(F("Looping forever."));
  for (;;)
    ;
}

// serialTrigger prints a message, then waits for something
// to come in from the serial port.
void serialTrigger(String message)
{
  Serial.println();
  Serial.println(message);
  Serial.println();
  while (!Serial.available())
    ;
  while (Serial.available())
    Serial.read();
}
