#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <rBase64.h>


//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "myWiFi";
const char WiFiPSK[] = "password";

/////////////////////
// Pin Definitions //
/////////////////////
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

//uncomment this line if using a Common Anode LED 
#define COMMON_ANODE

void setup() 
{
  initHardware();
  connectWiFi();
  setupMDNS();
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
}

void loop() 
{
  // Connect ot ISY and check alarm status
  WiFiClient client;
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
  String hostname = "thing_" + String(ESP.getChipId());
  char chostname[hostname.length() + 1];
  hostname.toCharArray(chostname, hostname.length() + 1);
  Serial.println("Hostname: " + hostname);
  if (!MDNS.begin(chostname))
  {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

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

