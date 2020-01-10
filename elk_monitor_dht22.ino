#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <rBase64.h>
#include <DHT_U.h>
#include <PubSubClient.h>

/////////////
// Globals //
/////////////
String espHostname;
char chostname[50];
int loopcnt;

//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "myWiFi";
const char WiFiPSK[] = "password";
WiFiClient client;


//uncomment this line if using a Common Anode LED 
#define COMMON_ANODE

// Type of DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302)

/////////////////////
// Pin Definitions //
/////////////////////
const int DHT_PIN = 2;
const int LED_PIN = 5; // Thing's onboard, green LED
const int LED_RED = 12;
const int LED_GRN = 13;
const int LED_BLU = 4;
const int LED_ON = 7; // Low for minimum brightness
const int ANALOG_PIN = A0; // The only analog pin on the Thing
const int DIGITAL_PIN = 12; // Digital pin to be read

/////////////////////
// ISY Definitions //
/////////////////////
const char ISYHost[] = "isy.example.com";
const char ISYElkArea1Status[] = "/rest/elk/area/1/get/status";
const char ISYUser[] = "admin";
const char ISYPass[] = "admin";

///////////////////////
// DHT22 Definitions //
///////////////////////
DHT_Unified dht(DHT_PIN, DHTTYPE);

//////////////////////
// MQTT Definitions //
//////////////////////
const char MQTTbroker[] = "mqtt.example.com";
const int MQTTport = 1883;
const char MQTTuser[] = "";
const char MQTTpass[] = "";
PubSubClient mqttclient(client);


void setup() 
{
  initHardware();
  startDHT22();
  connectWiFi();
  setupMDNS();
  setupMQTT();
  //digitalWrite(LED_RED, LOW);

  // On startup, cycle LED through colors so we know it's live
  setColor(LED_ON, 0, 0);
  delay(250);
  setColor(0, LED_ON, 0);
  delay(250);
  setColor(0, 0, LED_ON);
  delay(250);
  setColor(LED_ON, LED_ON, 0);
  delay(250);
  setColor(LED_ON, 0, LED_ON);
  delay(250);
  setColor(0, LED_ON, LED_ON);
  delay(250);
  setColor(0, 0, 0);

  // intialize loop counter
  loopcnt = 1;
}


void loop() 
{
  char *dtostrf(double val, signed char width, unsigned char prec, char *s);
  char temp[100];
  char topic[100];
  char *tstr;
  char *hstr;

  // Every 20 loops (5 min), reconnect to MQTT broker, just in case it has restarted
  if (loopcnt++ % 20 == 0) {
    setupMQTT();
  }
  
  // Get temp/humidity and sent it out via MQTT
  Serial.println("Requesting DHT22 Data: ");
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  } else {
    Serial.print(F("Temperature: "));
    Serial.print((event.temperature * 9.0 / 5.0) + 32.0);
    Serial.println(F("°F"));
    tstr = dtostrf((event.temperature * 9.0 / 5.0) + 32.0, 4, 2, temp);
    snprintf(topic, 100, "sensor/%s/temperature", chostname);
    Serial.println(topic);
    Serial.println(tstr);
    mqttclient.publish(topic, tstr);
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  } else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    hstr = dtostrf(event.relative_humidity, 4, 2, temp);
    snprintf(topic, 100, "sensor/%s/relative_humidity", chostname);
    mqttclient.publish(topic, hstr);
  }  

  // Connect ot ISY and check alarm status
  //WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(ISYHost, httpPort)) {
    Serial.println("connection failed");
    setColor(LED_ON / 2, 0, LED_ON);  // On connection error, set LED to blue-ish
    return;
  }

  Serial.print("Requesting Alarm Status: ");
  Serial.println(ISYElkArea1Status);

  String base64Auth = rbase64.encode(String(ISYUser) + ":" + String(ISYPass));
  Serial.println("Authorization: Basic " + base64Auth);
  client.print(String("GET ") + ISYElkArea1Status + " HTTP/1.1\r\n" +
               "Host: " + ISYHost + "\r\n" + 
               "Authorization: Basic " + base64Auth + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      setColor(LED_ON / 2, 0, LED_ON);  // On connection error, set LED to blue-ish
      return;
    }
  }
  
  // Read all the lines of the reply from server and print them to Serial
  int ae[4];  // Array for ae elements from query
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
    String parseLine = line;
    while (parseLine.indexOf('<') != -1) {
      String element = parseLine.substring(parseLine.indexOf("<")+1, parseLine.indexOf(">"));
      //Serial.println(element);
      // Get node and strip it from the rest of the element
      String node;
      if (element.indexOf(' ') != -1) {
        node = element.substring(0, element.indexOf(' '));
        element = element.substring(element.indexOf(' ') + 1);
      }
      // If xml elemenet is ae, has a type attribute and an area attribute with a value of 1 parse the type and value into ae[]
      // http://www.universal-devices.com/developers/wsdk/4.0.5/udiws30-all.html
      // type 1 is AlarmState
      // type 2 is ArmUpState
      // type 3 is ArmedState -- any value other than 0 is armed in some way
      if (node == "ae" and element.indexOf("type=") != -1) {
        int type = element.substring(element.indexOf("type=\"")+6,element.indexOf("type=\"")+7).toInt();
        ae[type] = element.substring(element.indexOf("val=\"")+5,element.indexOf("val=\"")+6).toInt();
        //Serial.println(String(type) + ":" + String(ae[type]));
      }
      parseLine = parseLine.substring(parseLine.indexOf(">")+1);
    }
  }
  
  // Set GPIO5 according to the request
  Serial.println(String(ae[3]));
  if (ae[3] == 0) { // Alarm is not armed if ae[3] is zero
    setColor(0, LED_ON, 0); // set LED to green
  } else {              // we're armed
    setColor(LED_ON, 0, 0); // set LED to red
  }

  delay(15000);
}

void connectWiFi()
{
  byte ledStatus = LOW;
  Serial.println();
  Serial.println("Connecting to: " + String(WiFiSSID));
  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);

  // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
  // to the stated [ssid], using the [passkey] as a WPA, WPA2,
  // or WEP passphrase.
  WiFi.begin(WiFiSSID, WiFiPSK);

  // Use the WiFi.status() function to check if the ESP8266
  // is connected to a WiFi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    setColor(0, ledStatus, ledStatus); // Blink LED with cyan while connecting to wifi
    ledStatus = (ledStatus == LED_ON) ? 0 : LED_ON;

    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
  }
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMDNS()
{
  // Call MDNS.begin(<domain>) to set up mDNS to point to
  // "<domain>.local"
  espHostname = "thing_" + String(ESP.getChipId());
  //char chostname[hostname.length() + 1];
  espHostname.toCharArray(chostname, espHostname.length() + 1);
  Serial.println("Hostname: " + espHostname);
  if (!MDNS.begin(chostname))
  {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

}

void setupMQTT()
{
  mqttclient.setServer(MQTTbroker, MQTTport);
  if (MQTTuser != "") {
    mqttclient.connect(chostname, MQTTuser, MQTTpass);
  }
}

void setColor(int red, int green, int blue)
{
  #ifdef COMMON_ANODE
    red = 1023 - red;
    green = 1023 - green;
    blue = 1023 - blue;
  #endif
  //Serial.println("red: " + String(red) + " grn: " + String(green) + " blu: " + String(blue));
  analogWrite(LED_RED, red);
  analogWrite(LED_GRN, green);
  analogWrite(LED_BLU, blue);
}

void startDHT22()
{
  dht.begin();

  delay(5000);  // Wait for sensor to stabalize

  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
}


void initHardware()
{
  Serial.begin(115200);

  pinMode(DIGITAL_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // turn off board LED

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GRN, OUTPUT);
  pinMode(LED_BLU, OUTPUT);
  // Don't need to set ANALOG_PIN as input, 
  // that's all it can be.
}

