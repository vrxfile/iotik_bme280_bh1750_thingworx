#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750FVI.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>

// Точка доступа Wi-Fi
char ssid[] = "IOTIK";
char pass[] = "Terminator812";

// Датчик освещенности
BH1750FVI bh1750;

// Датчик температуры/влажности и атмосферного давления
Adafruit_BME280 bme280;

// Выход реле
#define RELAY_PIN 12

// Датчик влажности почвы емкостной
#define MOISTURE_SENSOR A0
const float air_value1 = 422.0;
const float water_value1 = 240.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;

// Периоды для таймеров
#define BME280_UPDATE_TIME  5000
#define BH1750_UPDATE_TIME  5000
#define ANALOG_UPDATE_TIME  5000
#define IOT_UPDATE_TIME     10000

// Таймеры
SimpleTimer timer_bme280;
SimpleTimer timer_bh1750;
SimpleTimer timer_analog;
SimpleTimer timer_iot;

// Состояния управляющих устройств
int relay_control = 0;

// Параметры IoT сервера
char iot_server[] = "academic-educatorsextension.portal.ptc.io";
IPAddress iot_address(34, 224, 244, 58);
char thingName[] = "iotik_masterclass_2018_rostislav";
char serviceName[] = "iotik_masterclass_2018_service";
char appKey[] = "55394c8c-12bc-499e-854e-a445a672c74e";

// Параметры сенсоров для IoT сервера
#define sensorCount 5
char* sensorNames[] = {"air_temp", "air_hum", "air_press", "sun_light", "soil_hum"};
float sensorValues[sensorCount];
// Номера датчиков
#define air_temp     0x00
#define air_hum      0x01
#define air_press    0x02
#define sun_light    0x03
#define soil_hum     0x04

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 256

// Приемный буфер
char buff[BUFF_LENGTH] = "";

void setup()
{
  // Инициализация последовательного порта
  Serial.begin(115200);
  delay(512);

  // Инициализация Wi-Fi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Инициализация выхода реле
  pinMode(RELAY_PIN, OUTPUT);

  // Однократный опрос датчиков
  readSensorBME280(); readSensorBME280();
  readSensorBH1750(); readSensorBH1750();
  readSensorANALOG(); readSensorANALOG();

  // Вывод в терминал данных с датчиков
  printAllSensors();

  // Инициализация таймеров
  timer_bme280.setInterval(BME280_UPDATE_TIME, readSensorBME280);
  timer_bh1750.setInterval(BH1750_UPDATE_TIME, readSensorBH1750);
  timer_analog.setInterval(ANALOG_UPDATE_TIME, readSensorANALOG);
  timer_iot.setInterval(IOT_UPDATE_TIME, sendDataIot);
}

void loop()
{
  timer_bme280.run();
  timer_bh1750.run();
  timer_analog.run();
  timer_iot.run();
}

// Чтение датчика BH1750
void readSensorBH1750()
{
  Wire.begin(0, 2);         // Инициализация I2C на выводах 0, 2
  Wire.setClock(100000L);   // Снижение тактовой частоты для надежности
  bh1750.begin();           // Инициализация датчика
  bh1750.setMode(Continuously_High_Resolution_Mode); // Установка разрешения датчика
  sensorValues[sun_light] = bh1750.getAmbientLight();
}

// Чтение датчика BME280
void readSensorBME280()
{
  Wire.begin(4, 5);         // Инициализация I2C на выводах 4, 5
  Wire.setClock(100000L);   // Снижение тактовой частоты для надежности
  bme280.begin();
  sensorValues[air_temp] = bme280.readTemperature();
  sensorValues[air_hum] = bme280.readHumidity();
  sensorValues[air_press] = bme280.readPressure() * 7.5006 / 1000.0;
}

// Чтение аналоговых датчиков
void readSensorANALOG()
{
  float sensor_data = analogRead(MOISTURE_SENSOR);
  //Serial.println("ADC0: " + String(sensor_data, 3));
  sensorValues[soil_hum] = map(sensor_data, air_value1, water_value1, moisture_0, moisture_100);
}

// Print sensors data to terminal
void printAllSensors()
{
  for (int i = 0; i < sensorCount; i++)
  {
    Serial.print(sensorNames[i]);
    Serial.print(" = ");
    Serial.println(sensorValues[i]);
  }
  Serial.println();
}

// Подключение к серверу IoT ThingWorx
void sendDataIot()
{
  // Подключение к серверу
  WiFiClientSecure client;
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 443))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();

      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      if (json_array.success())
      {
        relay_control = json_array["relay_control"];
        Serial.println("Relay control:   " + String(relay_control));
        Serial.println();
      }
      else
      {
        Serial.println("Fail to parse packet from Thingworx!");
        Serial.println();
      }

      // Делаем управление устройствами
      controlDevices();

      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

// Управление исполнительными устройствами
void controlDevices()
{
  // Управление реле
  digitalWrite(RELAY_PIN, relay_control);
}

