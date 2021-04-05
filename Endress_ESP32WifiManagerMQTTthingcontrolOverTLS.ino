/*#################################An example to connect thingcontro.io MQTT over TLS1.2###############################

*/
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>

// Modbus
#include <ModbusMaster.h>
#include "REG_CONFIG.h"
#include <HardwareSerial.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <time.h>
#include <WiFi.h>
#include <ESP32Time.h>


#include <Wire.h>
#include <Adafruit_ADS1015.h>


HardwareSerial modbus(2);
ESP32Time rtc;
struct tm tmstruct ;

String dateTimeStr = "";
long timezone = 6;
byte daysavetime = 1;
#define WIFI_AP ""
#define WIFI_PASSWORD ""

String deviceToken = "NVYz0MQ05lTxYaOCGBO3";  //Sripratum@thingcontrio.io
//String deviceToken = "tOkvPadbQqLFsmc0sCON";
char thingsboardServer[] = "thingcontrol.io";

String json = "";

int r1 = 12; // ควบคุม Relay ตัวที่ 1 ต่อขา7
int r2 = 13; // ควบคุม Relay ตัวที่ 2 ต่อขา6
int r3 = 15; // ควบคุม Relay ตัวที่ 3 ต่อขา5
int r4 = 2; // ควบคุม Relay ตัวที่ 4 ต่อขา4

ModbusMaster node;

//static const char *fingerprint PROGMEM = "69 E5 FE 17 2A 13 9C 7C 98 94 CA E0 B0 A6 CB 68 66 6C CB 77"; // need to update every 3 months
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long startTeleMillis;
unsigned long starSendTeletMillis;
unsigned long currentMillis;
const unsigned long periodCallBack = 1000;  //the value is a number of milliseconds
const unsigned long periodSendTelemetry = 10000;  //the value is a number of milliseconds

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

int16_t adc0, adc1, adc2, adc3;
float val0 = 0.0;
float val1 = 0.0;
float val2 = 0.0;
float val3 = 0.0;
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
String downlink = "";
char *bString;
int PORT = 8883;
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

// Modbus
struct Meter
{
  String temp;
  String hum;

};

Meter meter[10] ;
//signal meta ;


void _initNTP() {
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");

  delay(2000);
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct, 5000);



  Serial.printf("\nNow is : %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct.tm_year) + 1900, ( tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour , tmstruct.tm_min, tmstruct.tm_sec);
  Serial.println("");

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  listDir(SD, "/", 0);
  removeDir(SD, "/mydir");
  createDir(SD, "/mydir");
  deleteFile(SD, "/hello.txt");
  writeFile(SD, "/hello.txt", dateTimeStr.c_str());
  appendFile(SD, "/hello.txt", " World!\n");
  listDir(SD, "/", 0);
}
void setup() {

  Serial.begin(115200);

  modbus.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println(F("Starting... SHT20 TEMP/HUM_RS485 Monitor"));
  // communicate with Modbus slave ID 1 over Serial (port 2)
  node.begin(ID_meter, modbus);

  Serial.println();
  Serial.println(F("***********************************"));

  WiFiManager wifiManager;
  wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("@Thingcontrol_AP")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    //    ESP.reset();
    delay(1000);
  }

  //  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, PORT );
  client.setCallback(callback);
  reconnectMqtt();

  Serial.print("Start..");
  startMillis = millis();  //initial start time
  _initNTP();
  pinMode(r1, OUTPUT);
  pinMode(r2, OUTPUT);
  pinMode(r3, OUTPUT);
  pinMode(r4, OUTPUT);
  digitalWrite(r1, 0);
  digitalWrite(r2, 0);
  digitalWrite(r3, 0);
  digitalWrite(r4, 0);
  ads.begin();
}
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  if ((in_max - in_min) + out_min != 0) {
    float val = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if ( val < 0)
      return 0;
    else
      return val;
  } else {
    return 0;
  }
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.print (file.name());
      time_t t = file.getLastWrite();
      struct tm * tmstruct = localtime(&t);
      Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, ( tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.print(file.size());
      time_t t = file.getLastWrite();
      struct tm * tmstruct = localtime(&t);
      Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, ( tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void readAnalog() {
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);

  Serial.print(adc0); Serial.print(" ");  Serial.print(adc1); Serial.print(" ");   Serial.print(adc2); Serial.print(" ");  Serial.println(adc3);
  val0 = mapfloat(adc0, 2128, 10560, 4, 20);
  val1 = mapfloat(adc1, 2128, 10560, 4, 20);
  val2 = mapfloat(adc2, 2128, 10560, 4, 20);
  val3 = mapfloat(adc3, 2128, 10560, 4, 20);
  Serial.print(val0); Serial.print(" ");  Serial.print(val1); Serial.print(" ");   Serial.print(val2); Serial.print(" ");  Serial.println(val3);


}

String floatToStr() {
  String toWrite = "";
  char Astr0[20];
  char Astr1[20];
  char Astr2[20];
  char Astr3[20];

  dtostrf(val0, 2, 4, Astr0);

  dtostrf(val1, 2, 4, Astr1);
  dtostrf(val2, 2, 4, Astr2);
  dtostrf(val3, 2, 4, Astr3);
  Serial.println(Astr1);
  toWrite.concat(Astr0);
  toWrite.concat(",");
  toWrite.concat(Astr1);
  toWrite.concat(",");
  toWrite.concat(Astr2);
  toWrite.concat(",");
  toWrite.concat(Astr3);
  toWrite.concat("\n");
  Serial.println(toWrite);
  return toWrite;

}

void writeSD() {

  getLocalTime(&tmstruct, 5000);
  String yearStr =  String(tmstruct.tm_year + 1900  , DEC);
  String monthStr =  String( tmstruct.tm_mon + 1  , DEC);
  String dayStr =  String(tmstruct.tm_mday  , DEC);
  String hourStr =  String(tmstruct.tm_hour  , DEC);
  String minStr =  String(tmstruct.tm_min  , DEC);
  String secStr =  String(tmstruct.tm_sec  , DEC);
  dateTimeStr.concat(dayStr);
  dateTimeStr.concat("/");
  dateTimeStr.concat(monthStr);
  dateTimeStr.concat("/");
  dateTimeStr.concat(yearStr);
  dateTimeStr.concat(" ");
  if (hourStr.length() < 2) {
    dateTimeStr.concat("0");
  }
  dateTimeStr.concat(hourStr);
  dateTimeStr.concat(":");
  if (minStr.length() < 2) {
    dateTimeStr.concat("0");
  }
  dateTimeStr.concat(minStr);
  dateTimeStr.concat(":");
  if (secStr.length() < 2) {
    dateTimeStr.concat("0");
  }
  dateTimeStr.concat(secStr);
  dateTimeStr.concat(" ");
  appendFile(SD, "/hello.txt", dateTimeStr.c_str());

  appendFile(SD, "/hello.txt", floatToStr().c_str() );
  Serial.println(dateTimeStr);
  dateTimeStr = "";
}
void loop()
{
  status = WiFi.status();
  if ( status == WL_CONNECTED)
  {
    if ( !client.connected() )
    {
      reconnectMqtt();
    }
    client.loop();
  }

  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)


  //check call back to connect Switch
  if (currentMillis - startMillis >= periodCallBack)  //test whether the period has elapsed
  {
    processCalled();
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }

  //send telemetry
  if (currentMillis - starSendTeletMillis >= periodSendTelemetry)  //test whether the period has elapsed
  {
    readAnalog();
    sendtelemetry();
    writeSD();
    starSendTeletMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
}


void getMac()
{
  Serial.println("OK");
  Serial.print("+deviceToken: ");
  Serial.println(WiFi.macAddress());
}

void viewActive()
{
  Serial.println("OK");
  Serial.print("+:WiFi, ");
  Serial.println(WiFi.status());
  if (client.state() == 0)
  {
    Serial.print("+:MQTT, [CONNECT] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
  else
  {
    Serial.print("+:MQTT, [FAILED] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
}

void setWiFi()
{
  Serial.println("OK");
  Serial.println("+:Reconfig WiFi  Restart...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  if (!wifiManager.startConfigPortal("ThingControlCommand"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //    ESP.reset();
    delay(5000);
  }

  Serial.println("OK");
  Serial.println("+:WiFi Connect");

  //  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, PORT );  // secure port over TLS 1.2
  client.setCallback(callback);
  reconnectMqtt();
}

void processCalled()
{
  Serial.println("OK");
  Serial.print("+:");
  Serial.println(downlink);
}



void processAtt(char jsonAtt[])
{

  char *aString = jsonAtt;

  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/attributes , "));
  Serial.println(aString);

  client.publish( "v1/devices/me/attributes", aString);
}

void processTele(char jsonTele[])
{

  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/telemetry , "));
  Serial.println(aString);

  client.publish( "v1/devices/me/telemetry", aString);
}

void processToken()
{
  char *aString;

  //  aString = cmdHdl.readStringArg();

  Serial.println("OK");
  Serial.print("+:deviceToken , ");
  Serial.println(aString);
  //  deviceToken = aString;

  reconnectMqtt();
}

void unrecognized(const char *command)
{
  Serial.println("ERROR");
}

void callback(char* topic, byte* payload, unsigned int length)
{

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';
  Serial.println(json);
  StaticJsonDocument<200> doc;
  //  JsonObject& data = jsonBuffer.parseObject((char*)json);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  String methodName = doc["method"];
  Serial.println(methodName);


}

void reconnectMqtt()
{
  if ( client.connect("entechconnect", deviceToken.c_str(), NULL) )
  {
    Serial.println( F("Connect MQTT Success."));
    client.subscribe("v1/devices/me/rpc/request/+");
  }

}


void read_Modbus(uint16_t  REG)
{

  static uint32_t i;
  uint8_t j, result;
  uint16_t data[2];
  uint16_t dat[2];
  uint32_t value = 0;


  // slave: read (6) 16-bit registers starting at register 2 to RX buffer
  result = node.readInputRegisters(REG, 2);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 2; j++)
    {
      data[j] = node.getResponseBuffer(j);
    }
  }
  for (int a = 0; a < 2; a++)
  {
    Serial.print(data[a]);
    Serial.print("\t");
  }
  Serial.println("");
  meter[0].temp = data[0];
  meter[0].hum =  data[1];
  Serial.println("----------------------");
}

void sendtelemetry()
{
  String json = "";
  json.concat("{\"adc0\":");
  json.concat(val0);
  json.concat(",\"adc1\":");
  json.concat(val1);
  json.concat(",\"adc2\":");
  json.concat(val2);
  json.concat(",\"adc3\":");
  json.concat(val3);
  json.concat("}");
  Serial.println(json);

  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);


  processTele(char_array);
  //}
}
