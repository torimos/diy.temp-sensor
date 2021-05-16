#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <string.h>

#define DHTPIN D4
#define DHTTYPE DHT22

const char* ssid = "smart-hub-npik2";
const char* password = "smarthome2018";
const char* mqtt_server = "192.168.1.121";
char dsn[32];
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

char mssg[128];
int sleepTime = 15;
uint32_t n = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) == String("config"))
  {
    int newSleepTime = sleepTime;
    if (length == 2)
      newSleepTime = payload[0] | (payload[1]<<8);
    else
      newSleepTime = payload[0];
    if (newSleepTime != sleepTime)
    {
      sleepTime = newSleepTime;
      Serial.printf("Sleep time has been updated to: %d ms\n\r", sleepTime);
    }
  }
}

void reconnect()
{
  while (!client.connected()) {
    if (!client.connect(dsn)) {
      delay(1000);
      Serial.print("*");
    }
    else {
      client.subscribe("config");
    }
  }
  Serial.println();
}

void setup_wifi(boolean tuenOn = true, unsigned int timeout = 5000)
{
  if (!tuenOn)
  {
    WiFi.mode(WIFI_OFF);
    return;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  auto sw = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
      delay(1000);
      Serial.print(".");
      if ((millis() - sw) > timeout) {
          Serial.println("Failed to connect to WiFi");
          Serial.println("Rebooting now...");
          ESP.restart();
          return;
      }
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void deepSleep(uint32_t time_ms)
{
  Serial.printf("Going to sleep for %d us...\n\r", time_ms);
  ESP.deepSleep(time_ms*1e3, RF_DEFAULT);
  // WiFi.forceSleepBegin();
  // delay(time);
  // WiFi.forceSleepWake();
}

void serialInit()
{
  Serial.begin(115200);
  Serial.write(27);       // ESC command
  Serial.print("[2J");    // clear screen command
  Serial.write(27);
  Serial.print("[H");     // cursor to home command
}

void setup() {
  serialInit();
  pinMode(A0, INPUT);
  sprintf(dsn,"A00Z%08X",ESP.getChipId());
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();

  auto sw = millis();
  reconnect();

  auto sw1 = millis();
  while((millis()-sw1)<=1500)
    client.loop();

  int an0 = analogRead(A0);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h)) h =  0.0;
  if (isnan(t)) t =  0.0;
  sprintf(mssg, "{\"seq\":%d,\"dsn\":\"%s\",\"int\":%d,\"typ\":\"%s\",\"value\":{\"t\":%.2f,\"h\":%.2f,\"an0\":%d}}", n, dsn, sleepTime, "tha", t, h, an0);
  client.publish("/sensor/data", mssg);
  Serial.printf("[%08ld] Message %d sent. JSON: %s\n\r", millis(), n, mssg);
  delay(50);
  
  n++;
  deepSleep(sleepTime * 1e3 - (millis() - sw));
}