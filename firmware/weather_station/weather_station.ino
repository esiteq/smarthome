/*
 * Author: Alex Bugrov
 * Creation date: 2022-02-04
 * Version: 0.2
 * /
// стандартная
#include <Wire.h>
// https://github.com/adafruit/Adafruit_BME280_Library
#include <Adafruit_BME280.h>
// https://github.com/claws/BH1750
#include <BH1750.h>
// https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// https://github.com/wayoda/LedControl
#include "LedControl.h"

// имя сети и пароль для первоначальной настройки сети
#define WIFI_SSID             ("WeatherStation")
#define WIFI_PASS             ("12345678")
// для вычисления примерной высоты (высоту оставил просто ради прикола)
#define SEALEVELPRESSURE_HPA  (1013.25)
// для перевода паскалей в мм/рт. ст.
#define  MMHG_PASCAL          (133.322368)
// размер буфера для формирования MQTT-сообщения
#define MSG_BUFFER_SIZE       (128)
// отправлять сообщения серверу раз в 5 минут
#define PUB_DELAY             (60000*5)
//#define PUB_DELAY             (15)
// хост, порт, имя пользователя и пароль MQTT-брокера
#define MQTT_HOST             ("192.168.1.127")
#define MQTT_PORT             (1883)
#define MQTT_USER             ("raven")
#define MQTT_PASS             ("bynthytn")
// кнопка подключена к D5
#define buttonPin             (D5)

BH1750 lightMeter;
Adafruit_BME280 bme;
WiFiManager wm;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
// true - датчики инициализированы
bool init_sensors = false;
// таймеры, чтобы не пользоваться delay
unsigned long timer = 0, disptimer = 0, gettimer = 0;
// буфер для формирования сообщения MQTT
char msg[MSG_BUFFER_SIZE];
// Pin D6 <==> LOAD
// Pin D7 <==> CLK
// Pin D8 <==> DIN
LedControl lc = LedControl(D8, D7, D6, 1);
// буфер для сообщения на экран
char disp[8];
// датчики true = ок, false = error
bool bme_ok = true, bh_ok = true;
int rain = 1024;
int lux  = -1;
int humi = -1;
int alt = -1000;
int press = -1;
int temp = -100;
// переменные для кнопки
int buttonState, buttonCount = 0, tempflag = 0, sendtimer = 0, senderrors;

// коллбэк режима настройки сети, выводим сообщение на экран
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  displayMsg("nS  0000");
}
// вывод 4 символов в одну половину экрана (0 - левая, 1 - правая)
void display4(int n)
{
  for (int i=0; i<4; i++)
  {
    lc.setChar(0, 7-(n*4)-i, disp[i], false);
  }
}
// Вывод содержимого буфера на дисплей, все 8 символов
void displayBuf()
{
  for (int i=0; i<8; i++)
  {
    lc.setChar(0, 7-i, disp[i], false);
  }  
}
// коллбэк когда приходит сообщение от MQTT-брокера
// Пока не используется, в дальнейшем будет использоваться для управления погодной станцией через веб-интерфейс
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
// вывод строки на экран
void displayMsg(String msg)
{
  sprintf(disp, "%s", msg);
  displayBuf();
}
// подключение к MQTT-брокеру (если еще не подключено)
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
// Вывод на экран приветствия HELLO (не несет никакой функциональной нагрузки)
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
  // инициализация датчиков
  initSensors();
  // выводим дисплей из спящего режима, в котором он по умолчанию
  lc.shutdown(0, false);
  // второй параметр - яркость дисплея, от 0 до 5
  lc.setIntensity(0, 0);
  lc.clearDisplay(0);
  displayHELLO();
  // включаем режим WiFi станции
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  // если соединение с сетью потеряно, автоматически реконнектимся
  wm.setWiFiAutoReconnect(true);
  // когда входим в режим настроек сети, выводим об этом сообщение
  wm.setAPCallback(configModeCallback);
  // 
  if (!wm.autoConnect(WIFI_SSID, WIFI_PASS))
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
  // задаем настройки MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqttReconnect();
}
// логирование данных погоды в консоль, для отладки
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
// получить данные с датчиков
void getWeather()
{
  // если датчики не инициализированы, делаем это
  if (!init_sensors)
    initSensors();
  rain = analogRead(A0);
  lux = lightMeter.readLightLevel();
  humi = bme.readHumidity();
  alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  press = (int)(bme.readPressure() / MMHG_PASCAL);
  temp = bme.readTemperature();
  // если датчики выдают марсианские значения (например, температура выше 100), пробуем инициализировать их заново
  // потому что возможно, соединение с одним из них было потеряно
  if (temp > 100 || press < 0)
  {
    initSensors();
  }
}
// отправляем данные погоды MQTT-брокеру
void publishWeather()
{
  // если датчики не инициализированы, ничего отправлять не нужно (все равно данные не актуальны)
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
    sendtimer = 0;
  }
  else
  {
    Serial.println("Error publishing weather");
    // если сообщение не отправилось, печатаем "E 01" в правой половине экрана
    sprintf(disp, "E 01");
    display4(1);
    senderrors++;
  }
  // задержка в 3 секунды, чтобы успели прочитать сообщение
  //delay(3000);
}

void initSensors()
{
  // если датчики уже инициализированы, ничего не делаем
  if (init_sensors)
    return;
  Wire.begin();
  // у BME280 жестко прошит адрес 0x76
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
    // если оба датчика инициализированы, устанавливаем флаг
    init_sensors = true;
  }
  else
  {
    delay(3000);
  }
}
// выводим температуру и влажность на экран
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
// выводим атмосферное давление и дождь на экран
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

void displaySendTimer()
{
  // выводим количество секунд с последней успешной отправки данных на правый дисплей
  sprintf(disp, "%4d", sendtimer);
  display4(0);
  sprintf(disp, "%4d", senderrors);
  display4(1);
}

void loop()
{
  // опрашиваем датчики, а также кнопку, один раз в секунду
  if (gettimer == 0 || millis() - gettimer > 1000)
  {
    gettimer = millis();
    getWeather();
    buttonState = digitalRead(buttonPin);
    if (buttonState == 1)
    {
      // если кнопка нажата, считаем сколько секунд
      buttonCount++;
      displaySendTimer();
    }
    else
    {
      // если отпущена, сбрасываем счетчик
      buttonCount = 0;
    }
    // если кнопка нажата более 8 секунд
    if (buttonCount > 8)
    {
      // сбрасываем настройки сети
      wm.resetSettings();
      // выводим сообщение 00000000
      displayMsg("00000000");
      delay(5000);
      // перезагружаем микроконтроллер
      ESP.reset();
    }
    // количество секунд с последней отправки
    sendtimer++;
  }
  // держим соединение с MQTT брокером открытым
  mqtt.loop();
  // раз в 5 минут отправляем данные MQTT-брокеру
  if (timer == 0 || millis() - timer > PUB_DELAY)
  {
    timer = millis(); 
    publishWeather();
  }
  // певые 5 секунд показываем температуру и влажность
  if (millis() - disptimer >= 5000 && millis() - disptimer < 10000)
  {
    // если температура еще не выводилась в этом цикле
    if (tempflag == 0)
    {
      displayTempHumi();
    }
    tempflag++;
  }
  // вторые 5 секунд показываем давление и дождь
  if (millis() - disptimer >= 10000 && millis() - disptimer < 15000)
  {
    // а также записываем все данные в консоль для отладки
    logWeather();
    displayPressRain();
    disptimer = millis();
    tempflag = 0;
    //delay(100);
  }
}
