#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "LedControl.h"

#define SEALEVELPRESSURE_HPA  (1013.25)
#define  MMHG_PASCAL          (133.322368)
#define MSG_BUFFER_SIZE       (128)
#define PUB_DELAY             (60000*5)
//#define PUB_DELAY             (15)
#define MQTT_USER             ("raven")
#define MQTT_PASS             ("bynthytn")
#define buttonPin             (D5)

BH1750 lightMeter;
Adafruit_BME280 bme;
WiFiManager wm;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
bool init_sensors = false;
unsigned long timer = 0, disptimer = 0, gettimer = 0;
char msg[MSG_BUFFER_SIZE];
int value = 0;
LedControl lc = LedControl(D8, D7, D6, 1);
char disp[8];
bool bme_ok = true, bh_ok = true;
//
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  displayMsg("nS  0000");
}
//
void display4(int n)
{
  for (int i=0; i<4; i++)
  {
    lc.setChar(0, 7-(n*4)-i, disp[i], false);
  }
}
// Вывод содержимого буфера на дисплей
void displayBuf()
{
  for (int i=0; i<8; i++)
  {
    lc.setChar(0, 7-i, disp[i], false);
  }  
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void displayMsg(String msg)
{
  sprintf(disp, "%s", msg);
  displayBuf();
}

void mqttReconnect()
{
  lc.setChar(0, 3, 'n', false);
  lc.setChar(0, 2, ' ', false);
  lc.setChar(0, 1, '0', false);
  if (mqtt.connected())
  {
    lc.setChar(0, 0, '1', false);
    delay(1000);
    return;
  }
  while (!mqtt.connected())
  {
    Serial.print("Attempting MQTT connection... ");
    if (mqtt.connect("WeatherStationClient_8112", MQTT_USER, MQTT_PASS))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      lc.setChar(0, 0, '0', false);
      delay(5000);
    }
  }
}

void displayHELLO()
{
  lc.setChar(0, 7, 'H', false);
  delay(250);
  lc.setChar(0, 6, 'E', false);
  delay(250);
  lc.setChar(0, 5, 'L', false);
  delay(250);
  lc.setChar(0, 4, 'L', false);
  delay(250);
  lc.setChar(0, 3, '0', false);
  delay(250);
  lc.setIntensity(0, 1);
  delay(250);
  lc.setIntensity(0, 0);
  delay(250);
  lc.setIntensity(0, 1);
  delay(250);
  lc.setIntensity(0, 0);
  delay(250);
  lc.setIntensity(0, 1);
  delay(250);
  lc.setIntensity(0, 0);
  delay(250);
  lc.clearDisplay(0);
}

void setup()
{
  pinMode(buttonPin, INPUT);
  initSensors();
  // Инициализация 7-сегментного 8-символьного дисплея
  lc.shutdown(0, false);
  lc.setIntensity(0, 0);
  lc.clearDisplay(0);
  displayHELLO();
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  //wm.resetSettings();
  wm.setWiFiAutoReconnect(true);
  wm.setAPCallback(configModeCallback);
  if (!wm.autoConnect("WeatherStation","12345678"))
  {
    Serial.println("Connecting to WiFi Failed");
    displayMsg("n 00    ");
    delay(5000);
  }
  else
  {
    Serial.println("Connected to WiFi");
    displayMsg("n 01    ");
    delay(1000);
  }
  mqtt.setServer("192.168.1.127", 1883);
  mqtt.setCallback(mqttCallback);
  mqttReconnect();
}

int rain = 1024;
int lux  = -1;
int humi = -1;
int alt = -1000;
int press = -1;
int temp = -100;

void logWeather()
{
  Serial.print("Rain = ");
  Serial.println(rain);
  Serial.print("Light = ");
  Serial.println(lux);
  Serial.print("Temperature = ");
  Serial.println(temp);
  Serial.print("Pressure = ");
  Serial.println(press);
  Serial.print("Altitude = ");
  Serial.println(alt);
  Serial.print("Humidity = ");
  Serial.println(humi);
  Serial.println();
}

void getWeather()
{
  if (!init_sensors)
    initSensors();
  rain = analogRead(A0);
  lux = lightMeter.readLightLevel();
  humi = bme.readHumidity();
  alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  press = (int)(bme.readPressure() / MMHG_PASCAL);
  temp = bme.readTemperature();
  if (temp > 100 || press < 0)
  {
    initSensors();
  }
}

void publishWeather()
{
  if (!init_sensors)
    return;
  // перед отправкой сообщения MQTT-брокеру, выводим букву "n" в последнем столбце
  lc.setChar(0, 0, 'n', false);
  // переконнектиться к MQTT-брокеру, если соединение потеряно
  mqttReconnect();
  // формируем строку в формате JSON
  sprintf(msg, "{\"r\":%d,\"l\":%d,\"t\":%d,\"p\":%d,\"a\":%d,\"h\":%d}", rain, lux, temp, press, alt, humi);
  // публикуем строку в брокер
  bool res = mqtt.publish("weather", msg);
  // стираем символ "n" из последнего столбца
  lc.setChar(0, 0, ' ', false);
  if (res)
  {
    Serial.println("Weather published successfully");
    // если сообщение отправилось, печатаем "n 11" в правой половине экрана
    sprintf(disp, "n 11");
    display4(1);
  }
  else
  {
    Serial.println("Error publishing weather");
    // если сообщение не отправилось, печатаем "E 01" в правой половине экрана
    sprintf(disp, "E 01");
    display4(1);
  }
  // задержка в 3 секунды, чтобы успели прочитать сообщение
  //delay(3000);
}

void initSensors()
{
  if (init_sensors)
    return;
  Wire.begin();
  if (!bme.begin(0x76))
  {
    bme_ok = false;
    // если датчик BME280 не инициализировался (например, не подключен), печатаем E280 в левой половине экрана
    sprintf(disp, "E280", 1);
    display4(0);
    Serial.println("BME280 Fail");
  }
  if (!lightMeter.begin())
  {
    bh_ok = false;
    // если датчик BH1750 не инициализировался (например, не подключен), печатаем E175 в правой половине экрана
    sprintf(disp, "E175", 1);
    display4(1);
    Serial.println("BH1750 Fail");
  }
  pinMode(A0, INPUT);  
  if (bme_ok && bh_ok)
  {
    init_sensors = true;
  }
  else
  {
    delay(3000);
  }
}

void displayTempHumi()
{
  if (init_sensors)
  {
    sprintf(disp, "%4d", temp);
    display4(0);
    sprintf(disp, "%4d", humi);
    display4(1);
  }
}

void displayPressRain()
{
  if (init_sensors)
  {
    sprintf(disp, "%4d", press);
    display4(0);
    sprintf(disp, "%4d", rain);
    display4(1);
  }
}

int buttonState, buttonCount = 0;

void loop()
{
  if (gettimer == 0 || millis() - gettimer > 1000)
  {
    gettimer = millis();
    getWeather();
    buttonState = digitalRead(buttonPin);
    if (buttonState == 1)
    {
      buttonCount++;
    }
    else
    {
      buttonCount = 0;
    }
    if (buttonCount > 8)
    {
      wm.resetSettings();
      displayMsg("00000000");
      delay(5000);
      ESP.reset();
    }
    //Serial.print("ButtonCount = ");
    //Serial.println(buttonCount);
  }
  // держим соединение с MQTT брокером открытым
  mqtt.loop();
  if (timer == 0 || millis() - timer > PUB_DELAY)
  {
    timer = millis(); 
    publishWeather();
  }
  
  if (millis() - disptimer >= 5000 && millis() - disptimer < 10000)
  {
    displayTempHumi();
    delay(100);
  }
  if (millis() - disptimer >= 10000 && millis() - disptimer < 15000)
  {
    logWeather();
    displayPressRain();
    disptimer = millis();
    delay(100);
  }
}
