#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

//----------------------Настройки передачи данных-----------------
String apiKey = "Your_API";           //API канала
const char *ssid =  "YOUR_SSID";               //имя интернета
const char *pass =  "YOUR_PASSWORD";               //пароль от интернета
const char* server = "api.thingspeak.com";    //сайт для отправки данных
WiFiClient client;                            //создаёт клиента для подключение к сети
//----------------------Настройки хоста--------------------------
const char* host = "esp8266-webupdate";
ESP8266WebServer httpServer(80);                        //настройка порта
ESP8266HTTPUpdateServer httpUpdater;
//----------------------------------------------------------------
#define WINDOW_SIZE 5                                   //Размер окна для фильтра скользящего среднего
int SUM,READUNGS[WINDOW_SIZE],INDEX=0;                  //Переменные для фильтра скользящего среднего
const int MPU_addr=0x68;                                //I2C address of the MPU-6050 (для акслерометра: SDA - D2, SCL - D1)
int16_t AcX,AcY,AcZ;                                    //переменные для данных акселерометра
int sounds,Progsounds;                                  //переменные для обычных и обработанных данных с микрофона (A0)
unsigned long tim;                                      //Переменная для хранения текущего времени работы

void setup(){
  Serial.begin(9600);
  WiFi.begin(ssid, pass);                                 //инициализация работы вайфая
  while (WiFi.status() != WL_CONNECTED) delay(500);       //ожидание подключения к сети
  Wire.begin();                                           //инициализация библиотеки Wire
  Wire.beginTransmission(MPU_addr);                       //начало отправки данных на MPU
  Wire.write(0x6B);                                       // PWR_MGMT_1 register (пока не измеряет выключен)
  Wire.write(0);                                          // установка 0 (активируем MPU-6050)
  Wire.endTransmission(true);                             //окончание отправки
  
  MDNS.begin(host);                                       //Инициализация хоста
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);

  tim=millis();                                           //запись текущего времени работы
}

//------------------Функция измерения-----------------------------
void measure(){                         
  Wire.beginTransmission(MPU_addr);       //начинает передачу данных на MPU
  Wire.write(0x3B);                       //какие данные отправяться
  Wire.endTransmission(false);            //окончание отправки
  Wire.requestFrom(MPU_addr,14,true);     //запрос байт у ведомого устройства
  AcX=Wire.read()<<8|Wire.read();         //данные акселерометра
  AcY=Wire.read()<<8|Wire.read();
  AcZ=Wire.read()<<8|Wire.read();
  sounds=analogRead(A0);                  //данные микрофона
  Progsounds=SoundAverage(sounds);        //обработка данных микрофона
}

//-----------------Основная-----------------------------------------
void loop(){
  while (WiFi.status() != WL_CONNECTED) delay(500);     //Если сеть отключилась, ждёт её востановления
  httpServer.handleClient();
  MDNS.update();
  measure();                          //функция измерени
  delay(10);
  if(millis()-tim>=16000){            //реализация задержки в 16 секунд
    if (client.connect(server,80)){
      Serial.println(WiFi.localIP());
      String DATA = apiKey;
      DATA +="&field1=";           //номер графика для отправки
      DATA += String(sounds);      //данные для отправки
      DATA +="&field2=";
      DATA += String(Progsounds);
      DATA +="&field3=";
      DATA += String(AcX);
      DATA +="&field4=";
      DATA += String(AcY);
      DATA +="&field5=";
      DATA += String(AcZ);
      DATA += "\r\n\r\n";          //окончание записи данных
      
      client.print("POST /update HTTP/1.1\n");                                //начало отпрвки даннных на сервер
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(DATA.length());
      client.print("\n\n" );
      client.print(DATA);                                                  //конец отпрвки данных на сервер
    }
    tim=millis();                                                          //запись текущего времени работы
  }
}

//---------------Фильтры скользящего среднего-----------------------
float SoundAverage(int a){
  SUM-=READUNGS[INDEX];
  READUNGS[INDEX]=a;
  SUM+=READUNGS[INDEX];
  INDEX=(INDEX+1)%WINDOW_SIZE;
  return (SUM/WINDOW_SIZE);
}
