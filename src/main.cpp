#include <Arduino.h>
#include <CooperativeMultitasking.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "WiFiClient.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ezButton.h>

void MQTT_connect();

const char *CMD_POWER = "power";
const char *CMD_DIP = "dip";
const char *CMD_RESULT_OK = "{\"result\":\"ok\"}";

const int pin_btn_power = 5;
const int pin_btn_dip = 2;

const int pin_led_dip = 3;
const int pin_led_dip_alt = 7;
const int pin_led_dust = 10;
const int pin_led_warn = 6;

ezButton input__pin_led_dip(pin_led_dip);
ezButton input__pin_led_dip_alt(pin_led_dip_alt);
ezButton input__pin_led_dust(pin_led_dust);
// ezButton input__pin_led_warn(pin_led_warn);

const int pin_debug_trigger = 9;

CooperativeMultitasking tasks;
// Continuation report_stat;
Continuation mqtt_publish_loop;
Continuation mqtt_scrb_loop;
// Continuation mqtt_scrb_end;

#define AIO_SERVER "10.110.89.118"
#define AIO_SERVERPORT 1883 // use 8883 for SSL
#define AIO_USERNAME ""
#define AIO_KEY ""

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish publish__stat = Adafruit_MQTT_Publish(&mqtt, "/cafe/stat");
// Adafruit_MQTT_Publish publish__cmd = Adafruit_MQTT_Publish(&mqtt, "/cafe/cmd");
Adafruit_MQTT_Subscribe subscribe__cmd = Adafruit_MQTT_Subscribe(&mqtt, "/cafe/cmd");

// int stat__pin_led_dip = 0;
// int stat__pin_led_dip_alt = 0;
// int stat__pin_led_dust = 0;
// int stat__pin_led_warn = 0;

StaticJsonDocument<96> doc;

void setup()
{
  Serial.begin(115200);
  // WiFi.begin(ssid, password, 6);
  WiFiManager wm;

  bool res;
  res = wm.autoConnect();

  if (!res)
  {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  pinMode(pin_btn_power, OUTPUT);
  pinMode(pin_btn_dip, OUTPUT);

  pinMode(pin_led_dip, INPUT);
  pinMode(pin_led_dip_alt, INPUT);
  pinMode(pin_led_dust, INPUT);
  pinMode(pin_led_warn, INPUT);

  pinMode(pin_debug_trigger, INPUT_PULLUP);

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(100);
  //   Serial.print(".");
  // }

  Serial.print("\nOK! IP=");
  Serial.println(WiFi.localIP());

  input__pin_led_dip.setDebounceTime(100);
  input__pin_led_dip_alt.setDebounceTime(100);
  input__pin_led_dust.setDebounceTime(100);
  // input__pin_led_warn.setDebounceTime(100);

  mqtt.subscribe(&subscribe__cmd);
  // tasks.now(report_stat);
  tasks.now(mqtt_publish_loop);
  tasks.now(mqtt_scrb_loop);

  Serial.println("Init");
}

void loop()
{
  input__pin_led_dip.loop();
  input__pin_led_dip_alt.loop();
  input__pin_led_dust.loop();
  // input__pin_led_warn.loop();

  tasks.run();
}

// void report_stat() {
//   // int debug__trigger_stat = digitalRead(pin_debug_trigger);
//   // if (debug__trigger_stat == LOW) {
//     stat__pin_led_dip = digitalRead(pin_led_dip);
//     // stat__pin_led_dip_alt = digitalRead(pin_led_dip_alt);
//     stat__pin_led_dust = digitalRead(pin_led_dust);
//     stat__pin_led_warn = digitalRead(pin_led_warn);
//     // zprintf("%d %d %d %d\n", stat__pin_led_dip, stat__pin_led_dip_alt, stat__pin_led_dust, stat__pin_led_warn);
//     tasks.after(1000, report_stat);
//   // } else {
//   //   tasks.after(10, report_stat);
//   // };
// }

void mqtt_publish_loop()
{
  MQTT_connect();
  doc["dip"] = input__pin_led_dip.getState();
  doc["dip_alt"] = input__pin_led_dip_alt.getState();
  doc["dust"] = input__pin_led_dust.getState();
  // doc["warn"] = input__pin_led_warn.getState();
  char j[256];
  size_t n = serializeJson(doc, j);
  serializeJson(doc, j, n);

  // const char* j = z("{\"dip\":%d,\"dip_alt\":%d,\"dust\":%d,\"warn\":%d}", stat__pin_led_dip, stat__pin_led_dip_alt, stat__pin_led_dust, stat__pin_led_warn);
  // Serial.println(j);
  if (!publish__stat.publish(j))
  {
    Serial.println(F("MQTT publish failed."));
  }
  else
  {
    // Serial.println(F("OK!"));
  }
  if (!mqtt.ping())
  {
    mqtt.disconnect();
  }
  tasks.after(1000, mqtt_publish_loop);
}

void mqtt_scrb_loop()
{
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000)))
  {
    if (subscription == &subscribe__cmd)
    {
      const char *c = (char *)subscribe__cmd.lastread;
      if (strcmp(c, CMD_POWER) == 0)
      {
        Serial.println(F("Got: PWR"));
        digitalWrite(pin_btn_power, HIGH);
        delay(500);
        digitalWrite(pin_btn_power, LOW);
        publish__stat.publish(CMD_RESULT_OK);
      }
      else if (strcmp(c, CMD_DIP) == 0)
      {
        Serial.println(F("Got: DIP"));
        digitalWrite(pin_btn_dip, HIGH);
        delay(500);
        digitalWrite(pin_btn_dip, LOW);
        publish__stat.publish(CMD_RESULT_OK);
      }
    }
  }
  // tasks.after(0, mqtt_publish_end);
  tasks.after(100, mqtt_scrb_loop);
}

// void mqtt_publish_end() {
//   tasks.after(1000, mqtt_scrb_loop);
// }

#define MAX_ZPRINTF_LENGTH 80
void zprintf(const char *format, ...)
{
  char s[MAX_ZPRINTF_LENGTH];
  va_list args;

  va_start(args, format);
  vsnprintf(s, MAX_ZPRINTF_LENGTH, format, args);
  va_end(args);

  Serial.print(s);
}

void MQTT_connect()
{
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected())
  {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0)
  { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000); // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}
