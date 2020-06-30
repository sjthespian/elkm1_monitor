# elkm1_monitor

This project uses a [SparkFun Thing Dev] board to provide alarm status for an ElkM1. In my case I use an [ISY994i] to communicate with my ElkM1, but it shoudl be stright forward to modify the code to talk directly to the Elk XEPM1.

I have also started integrating this with [ESPHome](https://esphome.io/) and [Home Assistant](https://www.home-assistant.io/), the myhome.yaml file is my initial attempt at that. For this implementation, the LED colors are controlled via. NodeRed, I will post those flows once they are up and running fully.

## The build

The deivce itself is very simple, it's just a multi-color LED connected to pins 4, 12, and 13 on the ThingDev with the common lead connected to 3V via. a resistor. The value of the reisistor isn't important, although you may want to test multiple values until you find a brightness you like. With the LED I used, the resistor I neded up with provides the minimum current needed to light the LED as I found everything else to be too bright.

Optionally you can also add a DHT22 for temperature and humidity monitioring. The code in elk_monitor_dht22.ino will talk to that and will transmit the data read over MQTT to the specified broker.

http://www.employees.org/~drich/ElkM1_ThingDev.png
![ThingDev with LED](http://banjo.employees.org/~drich/ElkM1_ThingDev_256x290.png)

## Code changes

To make this work, you will need to specify your WiFi SSID and password as well as the information needed to connect to your ISY in `elk_monitor.ino`. If you are using a DHT22, you will have to specify the pin that is connected to as well, along with your MQTT broker information.

If you decide to use different pins than I did, you will also need to change the pin assignments in the code.

You will need to edit the following values:
```
//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiSSID[] = "myWiFi";
const char WiFiPSK[] = "password";

/////////////////////
// Pin Definitions //
/////////////////////
const int DHT22_PIN = 3; // DHT22 pin
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

//////////////////////
// MQTT Definitions //
//////////////////////
const char MQTTbroker[] = "mqtt.example.com";
const int MQTTport = 1883;
const char MQTTuser[] = "";
const char MQTTpass[] = "";
```

[ElkM1]: https://www.elkproducts.com/m1_controls.html
[SparkFun Thing Dev]: https://www.sparkfun.com/products/13711
[ISY994i]: https://www.universal-devices.com/residential/isy994i-series/
