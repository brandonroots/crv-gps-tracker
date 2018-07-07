 /* This is an example sketch to send battery, temperature, and GPS location data to
 *  the cloud via either HTTP GET and POST requests or via MQTT protocol. In this 
 *  sketch we will send to dweet.io, a free cloud API, as well as to ThingsBoard.io,
 *  a very powerful and free IoT platform that allows you to visualize data on dashboards.
 *  
 *  SETTINGS: You can choose to post only once or to post periodically
 *  by commenting/uncommenting line 57 ("#define samplingRate 30"). When this line is 
 *  commented out the AVR microcontroller and MCP9808 temperature sensor are put to 
 *  sleep to conserve power, but when the line is being used data will be sent to the
 *  cloud periodically. This makes it operate like a GPS tracker!
 *  
 *  PROTOCOL: You can use HTTP GET or POST requests and you can change the URL to pretty
 *  much anything you want. You can also use MQTT to publish data to different feeds
 *  on Adafruit IO. You can also subscribe to Adafruit IO feeds to command the device
 *  to do something! In order to select a protocol, simply uncomment a line in the #define
 *  section below!
 *  
 *  DWEET.IO: To check if the data was successfully sent to dweet, go to
 *  http://dweet.io/get/latest/dweet/for/{IMEI} and the IMEI number is printed at the
 *  beginning of the code but can also be found printed on the SIMCOM module itself.
 *  
 *  IoT Example Getting-Started Tutorial: https://github.com/botletics/SIM7000-LTE-Shield/wiki/GPS-Tracker-Example
 *  GPS Tracker Tutorial Part 1: https://www.instructables.com/id/Arduino-LTE-Shield-GPS-Tracking-Freeboardio/
 *  GPS Tracker Tutorial Part 2: https://www.instructables.com/id/LTE-Arduino-GPS-Tracker-IoT-Dashboard-Part-2/
 *  
 *  Author: Timothy Woo (www.botletics.com)
 *  Github: https://github.com/botletics/SIM7000-LTE-Shield
 *  Last Updated: 6/4/2018
 *  License: GNU GPL v3.0
  */

#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>
#include <JeeLib.h> // Low power functions library
ISR(WDT_vect) { Sleepy::watchdogEvent(); } // Setup the watchdog

// You don't need the following includes if you're not using MQTT
// You can find the Adafruit MQTT library here: https://github.com/adafruit/Adafruit_MQTT_Library
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"

// Define *one* of the following lines:
//#define SIMCOM_2G // SIM800/808/900/908, etc.
//#define SIMCOM_3G // SIM5320
#define SIMCOM_7000  // SIM7000
//#define SIMCOM_7500 // SIM7500

/************************* PIN DEFINITIONS *********************************/
// Default
//#define FONA_RX 2
//#define FONA_TX 3
//#define FONA_RST 4
//#define PWRKEY 5

// For SIM7000 shield
#define FONA_PWRKEY 6
#define FONA_RST 7
//#define FONA_DTR 8 // Connect with solder jumper
//#define FONA_RI 9 // Need to enable via AT commands
#define FONA_TX 10 // Microcontroller RX
#define FONA_RX 11 // Microcontroller TX
//#define T_ALERT 12 // Connect with solder jumper

// For SIM7500 shield
//#define FONA_PWRKEY 6
//#define FONA_RST 7
////#define FONA_DTR 9 // Connect with solder jumper
////#define FONA_RI 8 // Need to enable via AT commands
//#define FONA_TX 11 // Microcontroller RX
//#define FONA_RX 10 // Microcontroller TX
////#define T_ALERT 5 // Connect with solder jumper

#define LED 13 // Just for testing if needed!

// Using SoftwareSerial
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

// Hardware serial is also possible!
//  HardwareSerial *fonaSerial = &Serial1;

// For ESP32 hardware serial use these lines instead
//#include <HardwareSerial.h>
//HardwareSerial MySerial(1);
//HardwareSerial *fonaSerial = &MySerial;

// Use this for 2G modules
#ifdef SIMCOM_2G
  Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
  
// Use this one for 3G modules
#elif defined(SIMCOM_3G)
  Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);
  
// Use this one for LTE CAT-M/NB-IoT modules (like SIM7000)
// Notice how we don't include the reset pin because it's reserved for emergencies on the LTE module!
#elif defined(SIMCOM_7000) || defined(SIMCOM_7500)
  Adafruit_FONA_LTE fona = Adafruit_FONA_LTE();
#endif

// Uncomment *one* of the following protocols you want to use
// to send data to the cloud! Leave the other commented out
#define PROTOCOL_HTTP_GET         // Generic
//#define PROTOCOL_HTTP_POST        // Generic
//#define PROTOCOL_MQTT_AIO         // Adafruit IO
//#define PROTOCOL_MQTT_CLOUDMQTT   // CloudMQTT
//#define PROTOCOL_MQTT_MYSIGNAL    // MySignal.io

#ifdef PROTOCOL_MQTT_AIO
  /************************* MQTT SETUP *********************************/
  // MQTT setup (if you're using it, that is)
  // For Adafruit IO:
  #define AIO_SERVER      "io.adafruit.com"
  #define AIO_SERVERPORT  1883
  #define AIO_USERNAME    "YOUR_AIO_USERNAME"
  #define AIO_KEY         "YOUR_AIO_KEY"

  // Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
  Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
  
  // How many transmission failures in a row we're OK with before reset
  uint8_t txfailures = 0;  
  
  /****************************** MQTT FEEDS ***************************************/
  // Setup feeds for publishing.
  // Notice MQTT paths for Adafruit IO follow the form: <username>/feeds/<feedname>
  // Also notice that the combined lat/long "location" feed requires "/csv" in the name
  // The Adafruit IO map requires this format: sensor_val, lat, long, altitude
  Adafruit_MQTT_Publish feed_location = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/location/csv"); // Group GPS data for AIO map in dashboard
  Adafruit_MQTT_Publish feed_speed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/speed");
  Adafruit_MQTT_Publish feed_head = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/heading");
  Adafruit_MQTT_Publish feed_alt = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/altitude");
  Adafruit_MQTT_Publish feed_temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
  Adafruit_MQTT_Publish feed_voltage = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/voltage");
  
  // Setup a feed called 'command' for subscribing to changes.
  Adafruit_MQTT_Subscribe feed_command = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/command");

#elif defined(PROTOCOL_MQTT_CLOUDMQTT)
  /************************* MQTT SETUP *********************************/
  // For CloudMQTT find these under the "Details" tab:
  #define MQTT_SERVER      "m12.cloudmqtt.com"
  #define MQTT_SERVERPORT  10559
  #define MQTT_USERNAME    "runzsefl"
  #define MQTT_KEY         "Naar74Git2-W"
  
#elif defined(PROTOCOL_MQTT_MYSIGNAL)
  /************************* MQTT SETUP *********************************/
  // For MySignal.io you can find the username is the device token
  // found under the "Devices" tab. No key is needed
  const char* token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpZCI6IjYyNDBhYjFlLTVmY2EtNGNkZC04YjYzLWM2NWI5NGNmMWVkOCJ9.8DBmfQRDFcWAq8zHIN2A0jOg20gIfcBRWnZbVYjCp8g";
  
  #define MQTT_SERVER      "mqtt.mysignal.io"
  #define MQTT_SERVERPORT  1883
  #define MQTT_USERNAME    token
#endif

/****************************** OTHER STUFF ***************************************/
// For sleeping the AVR
#include <avr/sleep.h>
#include <avr/power.h>

// For temperature sensor
#include <Wire.h>
#include "Adafruit_MCP9808.h"

// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

// The following line is used for applications that require repeated data posting, like GPS trackers
// Comment it out if you only want it to post once, not repeatedly every so often
//#define samplingRate 2 // The time in between posts, in minutes - 2 min
#define samplingRate 117 // The time in between posts, in minutes - 2 hrs ish

// The following line can be used to turn off the shield after posting data. This
// could be useful for saving energy for sparse readings but keep in mind that it
// will take longer to get a fix on location after turning back on than if it had
// already been on. Comment out to leave the shield on after it posts data.
#define turnOffShield // Turn off shield after posting data

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
char imei[16] = {0}; // Use this for device ID
char replybuffer[255]; // Large buffer for replies
uint8_t type;
uint16_t battLevel = 0; // Battery level (percentage)
float latitude, longitude, speed_kph, heading, altitude;
uint8_t counter = 0;
uint8_t timeOutCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println(F("SIMCom Module IoT Example"));

  #ifdef LED
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
  #endif
  
  pinMode(FONA_RST, OUTPUT);
  digitalWrite(FONA_RST, HIGH); // Default state

  delay(100); // This helps the temperature sensor code run properly!

  pinMode(FONA_PWRKEY, OUTPUT);
  powerOn(); // Power on the module
  moduleSetup(); // Establishes first-time serial comm and prints IMEI

  tempsensor.wake(); // Wake up the MCP9808 if it was sleeping
  if (!tempsensor.begin()) {
    Serial.println("Couldn't find the MCP9808!");
    while (1);
  }

  // Configure a GPRS APN, username, and password.
  // You might need to do this to access your network's GPRS/data
  // network.  Contact your provider for the exact APN, username,
  // and password values.  Username and password are optional and
  // can be removed, but APN is required.
  //fona.setGPRSNetworkSettings(F("your APN"), F("your username"), F("your password"));
  //fona.setGPRSNetworkShettings(F("m2m.com.attz")); // For AT&T IoT SIM card
  fona.setGPRSNetworkSettings(F("hologram")); // For Hologram developer SIM card
  
  // Optionally configure HTTP gets to follow redirects over SSL.
  // Default is not to follow SSL redirects, however if you uncomment
  // the following line then redirects over SSL will be followed.
  //fona.setHTTPSRedirect(true);

  // Perform first-time GPS/GPRS setup if the shield is going to remain on,
  // otherwise these won't be enabled in loop() and it won't work!
#ifndef turnOffShield
  // Enable GPS
  while (!fona.enableGPS(true)) {
    Serial.println(F("Failed to turn on GPS, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Turned on GPS!"));

  // Disable GPRS just to make sure it was actually off so that we can turn it on
  if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS!"));
  
  // Turn on GPRS
  while (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to enable GPRS, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Enabled GPRS!"));
#endif

#ifdef PROTOCOL_MQTT_AIO
  mqtt.subscribe(&feed_command); // Only if you're using MQTT
#endif

}

void loop() {
  // Connect to cell network and verify connection
  // If unsuccessful, keep retrying every 2s until a connection is made
  while (!netStatus()) {
    Serial.println(F("Failed to connect to cell network, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Connected to cell network!"));

  // Measure battery level
  // Note: on the LTE shield this won't be accurate because the SIM7000
  // is supplied by a regulated 3.6V, not directly from the battery. You
  // can use the Arduino and a voltage divider to measure the battery voltage
  // and use that instead, but for now we will use the function below
  // only for testing.
  battLevel = readVcc(); // Get voltage in mV

  // Measure temperature
  tempsensor.wake(); // Wake up the MCP9808 if it was sleeping
  float tempC = tempsensor.readTempC();
  float tempF = tempC * 9.0 / 5.0 + 32;
  Serial.print("Temp: "); Serial.print(tempC); Serial.print("*C\t"); 
  Serial.print(tempF); Serial.println("*F");
  
  Serial.println("Shutting down the MCP9808...");
  tempsensor.shutdown(); // In this mode the MCP9808 draws only about 0.1uA

  float temperature = tempC; // Select what unit you want to use for this example

  delay(500); // I found that this helps

  // Turn on GPS if it wasn't on already (e.g., if the module wasn't turned off)
#ifdef turnOffShield
  while (!fona.enableGPS(true)) {
    Serial.println(F("Failed to turn on GPS, retrying..."));
    delay(2000); // Retry every 2s
    timeOut();
  }
  Serial.println(F("Turned on GPS!"));
#endif

  // Get a fix on location, try every 2s
  while (!fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude)) {
    Serial.println(F("Failed to get GPS location, retrying..."));
    delay(2000); // Retry every 2s
    timeOut();
  }
  Serial.println(F("Found 'eeeeem!"));
  Serial.println(F("---------------------"));
  Serial.print(F("Latitude: ")); Serial.println(latitude, 6);
  Serial.print(F("Longitude: ")); Serial.println(longitude, 6);
  Serial.print(F("Speed: ")); Serial.println(speed_kph);
  Serial.print(F("Heading: ")); Serial.println(heading);
  Serial.print(F("Altitude: ")); Serial.println(altitude);
  Serial.println(F("---------------------"));

#ifdef turnOffShield // If the shield was already on, no need to re-enable
  // Disable GPRS just to make sure it was actually off so that we can turn it on
  if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS!"));
  
  // Turn on GPRS
  while (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to enable GPRS, retrying..."));
    delay(2000); // Retry every 2s
    timeOut();
  }
  Serial.println(F("Enabled GPRS!"));
#endif

  // Post something like temperature and battery level to the web API
  // Construct URL and post the data to the web API
  char URL[200]; // Make sure this is long enough for your request URL
  char body[200];
  char latBuff[16], longBuff[16], locBuff[32], speedBuff[16], headBuff[16], 
       altBuff[16], tempBuff[16], battBuff[16];

  // Format the floating point numbers
  dtostrf(latitude, 1, 6, latBuff);
  dtostrf(longitude, 1, 6, longBuff);
  dtostrf(speed_kph, 1, 0, speedBuff);
  dtostrf(heading, 1, 0, headBuff);
  dtostrf(altitude, 1, 1, altBuff);
  dtostrf(temperature, 1, 2, tempBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(battLevel, 1, 0, battBuff);

  // Also construct a combined, comma-separated location array
  // (many platforms require this for dashboards, like Adafruit IO):
  sprintf(locBuff, "%s,%s,%s,%s", speedBuff, latBuff, longBuff, altBuff); // This could look like "10,33.123456,-85.123456,120.5"
  
  // Construct the appropriate URL's and body, depending on request type
  // In this example we use the IMEI as device ID

#ifdef PROTOCOL_HTTP_GET
  // GET request
  // You can adjust the contents of the request if you don't need certain things like speed, altitude, etc.
  sprintf(URL, "http://dweet.io/dweet/for/%s?lat=%s&long=%s&speed=%s&head=%s&alt=%s&temp=%s&batt=%s", imei, latBuff, longBuff,
          speedBuff, headBuff, altBuff, tempBuff, battBuff);

  counter = 0; // This counts the number of failed attempts tries
  // Try a total of three times if the post was unsuccessful (try additional 2 times)
  while (counter < 3 && !fona.postData("GET", URL)) {
    Serial.println(F("Failed to post data, retrying..."));
    counter++; // Increment counter
    delay(1000);
  }
#elif (defined(PROTOCOL_HTTP_POST))
  // You can also do a POST request instead

  sprintf(URL, "http://dweet.io/dweet/for/%s", imei);
  sprintf(body, "{\"temp\":%s,\"batt\":%s}", tempBuff, battBuff);

  counter = 0;
  while (counter < 3 && !fona.postData("POST", URL, body)) {
    Serial.println(F("Failed to complete HTTP POST..."));
    counter++;
    delay(1000);
  }


  // Let's try a POST request to thingsboard.io
  /*
  const char* token = "qFeFpQIC9C69GDFLWdAv"; // From thingsboard.io device
  sprintf(URL, "http://demo.thingsboard.io/api/v1/%s/telemetry", token);
  sprintf(body, "{\"lat\":%s,\"long\":%s,\"speed\":%s,\"head\":%s,\"alt\":%s,\"temp\":%s,\"batt\":%s}", latBuff, longBuff,
          speedBuff, headBuff, altBuff, tempBuff, battBuff);
//  sprintf(body, "{\"lat\":%s,\"long\":%s}", latBuff, longBuff); // If all you want is lat/long

  counter = 0;
  while (counter < 3 && !fona.postData("POST", URL, body)) {
    Serial.println(F("Failed to complete HTTP POST..."));
    counter++;
    delay(1000);
  }
  */

#elif defined(PROTOCOL_MQTT_AIO)
  // Let's use MQTT!
  
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected). See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // Now publish all the data to different feeds!
  // The MQTT_publish_checkSuccess handles repetitive stuff.
  // You can see the function near the end of this sketch.
  // For the Adafruit IO dashboard map we send the combined lat/long buffer
  MQTT_publish_checkSuccess(feed_location, locBuff);
//  MQTT_publish_checkSuccess(feed_speed, speedBuff); // Included in "location" feed
  MQTT_publish_checkSuccess(feed_head, headBuff);
//  MQTT_publish_checkSuccess(feed_alt, altBuff); // Included in "location" feed
  MQTT_publish_checkSuccess(feed_temp, tempBuff);
  MQTT_publish_checkSuccess(feed_voltage, battBuff);
 
  // This is our 'wait for incoming subscription packets' busy subloop
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &feed_command) {
      Serial.print(F("*** Got: "));
      Serial.println((char *)feed_command.lastread);
    }
  }

  // Control an LED based on what we receive from the command feed subscription!
  if (strcmp(feed_command.lastread, "ON") == 0) {
    Serial.println(F("*** Commanded to turn on LED!"));
    digitalWrite(LED, HIGH);
  }
  else if (strcmp(feed_command.lastread, "OFF") == 0) {
    Serial.println(F("*** Commanded to turn off LED!"));
    digitalWrite(LED, LOW);
  }
#elif defined(PROTOCOL_MQTT_CLOUDMQTT)
  // Let's use CloudMQTT! NOTE: connecting and publishing work, but everything else
  // still under development!!!
  char MQTT_CLIENT[16] = " ";  // We'll change this to the IMEI
  
  // Let's begin by changing the client name to the IMEI number to better identify
  strcpy(MQTT_CLIENT, imei); // Copy the contents of the imei into the char array "MQTT_client"

  // Connect to MQTT broker
  if (!fona.TCPconnect(MQTT_SERVER, MQTT_SERVERPORT)) Serial.println(F("Failed to connect to TCP/IP!"));
  // CloudMQTT requires "MQIsdp" instead of "MQTT"
  if (!fona.MQTTconnect("MQIsdp", MQTT_CLIENT, MQTT_USERNAME, MQTT_KEY)) Serial.println(F("Failed to connect to MQTT broker!"));
  
  // Publish each data point under a different topic!
  Serial.println(F("Publishing data to their respective topics!"));  
//  if (!fona.MQTTpublish("latitude", latBuff)) Serial.println(F("Failed to publish data!")); // Can send individually if needed
//  if (!fona.MQTTpublish("longitude", longBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("location", locBuff)) Serial.println(F("Failed to publish data!")); // Combined data
  if (!fona.MQTTpublish("speed", speedBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("heading", headBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("altitude", altBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("temperature", tempBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("voltage", battBuff)) Serial.println(F("Failed to publish data!"));
  
  // Subscribe to topic
//  Serial.print(F("Subscribing to topic: ")); Serial.println(sub_topic);
//  if (!fona.MQTTsubscribe(sub_topic, 0)) Serial.println(F("Failed to subscribe!"));

  // Unsubscribe to topic
//  Serial.print(F("Unsubscribing from topic: ")); Serial.println(sub_topic);
//  if (!fona.MQTTunsubscribe(sub_topic)) Serial.println(F("Failed to receive data!")); // Topic, quality of service (QoS)

  // Receive data
//  if (!fona.MQTTreceive(MQTT_topic)) Serial.println(F("Failed to unsubscribe!"));
  
  // Disconnect from MQTT broker
//  if (!fona.MQTTdisconnect()) Serial.println(F("Failed to close connection!"));

  // Close TCP connection
  if (!fona.TCPclose()) Serial.println(F("Failed to close connection!"));
#elif defined(PROTOCOL_MQTT_MYSIGNAL)
  // Let's send data to MySignal.io using MQTT!
  char MQTT_CLIENT[16] = " ";  // We'll change this to the IMEI
  char GPS_data[64]; // Holds lat/long data
  
  // Let's begin by changing the client name to the IMEI number to better identify
  strcpy(MQTT_CLIENT, imei);

  // Connect to MQTT broker
  if (!fona.TCPconnect(MQTT_SERVER, MQTT_SERVERPORT)) Serial.println(F("Failed to connect to TCP/IP!"));
  if (!fona.MQTTconnect("MQTT", MQTT_CLIENT, MQTT_USERNAME)) Serial.println(F("Failed to connect to MQTT broker!")); // No key needed for MySignal.io

  // Construct comma-separated GPS lat/long data
  sprintf(GPS_data, "%s, %s", latBuff, longBuff);
  
  // Publish lat and long data!
  Serial.print(F("Publishing GPS data: ")); Serial.println(GPS_data);
  if (!fona.MQTTpublish("device/coordinates", GPS_data)) Serial.println(F("Failed to publish data!"));

  // Close TCP connection
  if (!fona.TCPclose()) Serial.println(F("Failed to close connection!"));
#endif

  //Only run the code below if you want to turn off the shield after posting data
#ifdef turnOffShield
  // Disable GPRS
  // Note that you might not want to check if this was successful, but just run it
  // since the next command is to turn off the module anyway
  if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS!"));

  // Turn off GPS
  if (!fona.enableGPS(false)) Serial.println(F("Failed to turn off GPS!"));
  
  // Power off the module. Note that you could instead put it in minimum functionality mode
  // instead of completely turning it off. Experiment different ways depending on your application!
  // You should see the "PWR" LED turn off after this command
//  if (!fona.powerDown()) Serial.println(F("Failed to power down FONA!")); // No retries
  counter = 0;
  while (counter < 3 && !fona.powerDown()) { // Try shutting down 
    Serial.println(F("Failed to power down FONA!"));
    counter++; // Increment counter
    delay(1000);
  }
#endif
  
  // Alternative to the AT command method above:
  // If your FONA has a PWRKEY pin connected to your MCU, you can pulse PWRKEY
  // LOW for a little bit, then pull it back HIGH, like this:
//  digitalWrite(PWRKEY, LOW);
//  delay(600); // Minimum of 64ms to turn on and 500ms to turn off for FONA 3G. Check spec sheet for other types
//  delay(1300); // Minimum of 1.2s for SIM7000 shield
//  digitalWrite(PWRKEY, HIGH);
  
  // Shut down the MCU to save power
#ifndef samplingRate
  Serial.println(F("Shutting down..."));
  delay(5); // This is just to read the response of the last AT command before shutting down
  MCU_powerDown(); // You could also write your own function to make it sleep for a certain duration instead
  
#else
  // The following lines are for if you want to periodically post data (like GPS tracker)
  Serial.print(F("Waiting for ")); Serial.print(samplingRate); Serial.println(F(" minutes\r\n"));
  timeOutCount = 0;
  //delay(samplingRate * 60000UL); // Delay - this is where to add JeeLib code to save power


// For Sleep Mode Tracker

  delay(500UL);
for (byte i = 0; i < samplingRate; ++i) {
    for (byte minuteLoop = 0; minuteLoop < 12; ++minuteLoop) {
          Sleepy::loseSomeTime(5000);
    }
}
  
  
  powerOn(); // Powers on the module if it was off previously

  // Only run the initialization again if the module was powered off
  // since it resets back to 115200 baud instead of 4800.
  #ifdef turnOffShield
    moduleSetup();
  #endif
    
#endif
}

// Power on the module
void powerOn() {
  digitalWrite(FONA_PWRKEY, LOW);
  // See spec sheets for your particular module
  #if defined(SIMCOM_2G)
    delay(1050);
  #elif defined(SIMCOM_3G)
    delay(180); // For SIM5320
  #elif defined(SIMCOM_7000)
    delay(100); // For SIM7000
  #elif defined(SIMCOM_7500)
    delay(500); // For SIM7500
  #endif
  
  digitalWrite(FONA_PWRKEY, HIGH);
}

void moduleSetup() {
 // The baud rate always resets back to default (115200) after being powered
 // powered down or shutting off, so let's try 115200 first. Hats off to
 // anyone who can figure out how to make it remember the new baud rate even
 // after being power cycled!
 fonaSerial->begin(115200); // Default LTE shield baud rate
 fona.begin(*fonaSerial); // Don't use if statement because an OK reply could be sent incorrectly at 115200 baud
 // If you are using hardware serial you can uncomment the lines below
 // and comment the one right above
//  if (!fona.begin(*fonaSerial)) { 
//    Serial.println(F("Couldn't find FONA at 115200 baud"));
//  }

 Serial.println(F("Configuring to 4800 baud"));
 fona.setBaudrate(4800); // Set to 4800 baud
 fonaSerial->begin(4800);
 if (!fona.begin(*fonaSerial)) {
   Serial.println(F("Couldn't find FONA"));
   while(1); // Don't proceed if it couldn't find the device
 }

 /*
  // ALTERNATIVE METHOD:
  // With the 5s delay before this function is called
  // we can simply change the baud rate to 4800 since
  // the delay ensures the module is on
  delay(5000); // Ensure the module has time to turn on
  fonaSS.begin(115200); // Default SIM7000 shield baud rate
  
  Serial.println(F("Configuring to 4800 baud"));
  fonaSS.println("AT+IPR=4800");
  fonaSS.begin(4800);
  if (! fona.begin(fonaSS)) {
    Serial.println(F("Couldn't find FONA"));
    return false; // Don't proceed if it couldn't find the device
  }
*/

  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case SIM800L:
      Serial.println(F("SIM800L")); break;
    case SIM800H:
      Serial.println(F("SIM800H")); break;
    case SIM808_V1:
      Serial.println(F("SIM808 (v1)")); break;
    case SIM808_V2:
      Serial.println(F("SIM808 (v2)")); break;
    case SIM5320A:
      Serial.println(F("SIM5320A (American)")); break;
    case SIM5320E:
      Serial.println(F("SIM5320E (European)")); break;
    case SIM7000A:
      Serial.println(F("SIM7000A (American)")); break;
    case SIM7000C:
      Serial.println(F("SIM7000C (Chinese)")); break;
    case SIM7000E:
      Serial.println(F("SIM7000E (European)")); break;
    case SIM7000G:
      Serial.println(F("SIM7000G (Global)")); break;
    case SIM7500A:
      Serial.println(F("SIM7500A (American)")); break;
    case SIM7500E:
      Serial.println(F("SIM7500E (European)")); break;
    default:
      Serial.println(F("???")); break;
  }
  
  // Print module IMEI number.
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  // Configure CFUN=1
  fonaSS.println("AT+CFUN=1,0");
  Serial.println("AT+CFUN=1");
}

// Read the module's power supply voltage
float readVcc() {
  // Read battery voltage
  if (!fona.getBattVoltage(&battLevel)) Serial.println(F("Failed to read batt"));
  else Serial.print(F("battery = ")); Serial.print(battLevel); Serial.println(F(" mV"));

  // Read LiPo battery percentage
  // Note: This will NOT work properly on the LTE shield because the voltage
  // is regulated to 3.6V so you will always read about the same value!
//  if (!fona.getBattPercent(&battLevel)) Serial.println(F("Failed to read batt"));
//  else Serial.print(F("BAT % = ")); Serial.print(battLevel); Serial.println(F("%"));

  return battLevel;
}

bool netStatus() {
  int n = fona.getNetworkStatus();
  
  Serial.print(F("Network status ")); Serial.print(n); Serial.print(F(": "));
  if (n == 0) Serial.println(F("Not registered"));
  if (n == 1) Serial.println(F("Registered (home)"));
  if (n == 2) Serial.println(F("Not registered (searching)"));
  if (n == 3) Serial.println(F("Denied"));
  if (n == 4) Serial.println(F("Unknown"));
  if (n == 5) Serial.println(F("Registered roaming"));

  if (!(n == 1 || n == 5)) return false;
  else return true;
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
#ifdef PROTOCOL_MQTT_AIO
  void MQTT_connect() {
    int8_t ret;
  
    // Stop if already connected.
    if (mqtt.connected()) {
      return;
    }
  
    Serial.println("Connecting to MQTT... ");
  
    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
      Serial.println(mqtt.connectErrorString(ret));
      Serial.println("Retrying MQTT connection in 5 seconds...");
      mqtt.disconnect();
      delay(5000);  // wait 5 seconds
    }
    Serial.println("MQTT Connected!");
  }

  void MQTT_publish_checkSuccess(Adafruit_MQTT_Publish &feed, const char *feedContent) {
    Serial.println(F("Sending data..."));
    if (! feed.publish(feedContent)) {
      Serial.println(F("Failed"));
      txfailures++;
    }
    else {
      Serial.println(F("OK!"));
      txfailures = 0;
    }
  }
#endif

// Turn off the MCU completely. Can only wake up from RESET button
// However, this can be altered to wake up via a pin change interrupt
void MCU_powerDown() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0; // Turn off ADC
  power_all_disable ();  // Power off ADC, Timer 0 and 1, serial interface
  sleep_enable();
  sleep_cpu();
}

// Timeout Counter to reset device
void timeOut() {
  ++timeOutCount;
  Serial.println(F("Timeout..."));
  if (timeOutCount > 30) {
    // Trigger a device reset
    Serial.println(F("Taking too long. Resetting."));
    Serial.println(F("Shutting down..."));
    delay(5); // This is just to read the response of the last AT command before shutting down
    // Power off the module. Note that you could instead put it in minimum functionality mode
    // instead of completely turning it off. Experiment different ways depending on your application!
    // You should see the "PWR" LED turn off after this command
    //  if (!fona.powerDown()) Serial.println(F("Failed to power down FONA!")); // No retries
    counter = 0;
    while (counter < 3 && !fona.powerDown()) { // Try shutting down 
      Serial.println(F("Failed to power down FONA!"));
      counter++; // Increment counter
      delay(1000);
    }
    delay(5); // This is just to read the response of the last AT command before shutting down
    software_Reset();
  }
}

// Restarts program from beginning but 
// does not reset the peripherals and registers
void software_Reset()
{
  Serial.println(F("Restarting Arduino..."));
  delay(5); // This is just to read the response of the last AT command before shutting down
  asm volatile ("  jmp 0");  
}

