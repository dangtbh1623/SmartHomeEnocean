#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <TH02_dev.h>
#include "Arduino.h"

#define ORG_BYTE 3
#define D_BYTE_3 4
#define D_BYTE_2 5
#define D_BYTE_1 6
#define D_BYTE_0 7
#define ID_BYTE_3 8
#define ID_BYTE_2 9
#define ID_BYTE_1 10
#define ID_BYTE_0 11
#define STATUS_BYTE 12
#define CHECKSUM_BYTE 13

const char* ssid = "DangTrinhUnity";       // Personal network SSID
const char* wifi_password = "Joy-IT23!9"; //  Personal network password

const char* mqtt_server = "192.168.0.17";  // IP of the MQTT broker

// MQTT: topics
const char* enocean_topic = "enocean2mqtt/telegramm";
const char* enocean_Client = "enocean2mqtt/Client";
const char* enocean_light_state = "enocean2mqtt/led/status";
const char* enocean_light_command = "enocean2mqtt/led/switch";

// payloads by default (on/off)
const char* light_on = "ON";
const char* light_off = "OFF";

// Status of LED
bool light_status = false;



const char* mqtt_username = "homeassistant"; // MQTT username
const char* mqtt_password = "ve1Ahs9zib2tog0ahv5oewaim8weeruaW3shahPie9ie9ahsheebaeh3iujaoras"; // MQTT password

const char* clientID = "ESP32mitTCM320"; // MQTT client ID

uint8_t softwareVersion_telegramm[14];
uint8_t BaseID_telegram[14];
uint8_t BaseID[4];

WiFiClient esp32Client;
PubSubClient client(esp32Client);

long lastMsg = 0;
char msg[100];
int value = 0;

// LED Pin
const int ledPin = 4;

// Definiert Pins für Serial2. (ESP2)
constexpr uint8_t RXD2_pin = 14;
constexpr uint8_t TXD2_pin = 15;

String device_id = "0x";

////////////////////////
//// printByteHex  ////
//////////////////////
// gibt Data des Typs Uint8 als HEX aus
void printByteHex(uint8_t val) {
  Serial.print(val, HEX);
  Serial.print(" ");
}

////////////////////////
//// calcChksum  ////
//////////////////////
// ermittelt die Checksumnummer eines Enocean-Telegramms
uint8_t calcChksum(uint8_t dat[]) {
  uint32_t chksum = 0;
  for (int i = 2; i < 13; i++) {
    chksum += dat[i];
  }
  return chksum & 0xff;
}

////////////////////////
//// readTelegram  ////
//////////////////////
// liest die Enocean-Telegramm von der Serial2-Schnittstelle.
// Enocean Telegramm beginnt mit 0xA5 0x5A
// Return True wenn eine Telegramm erfolgreich eingelesen wurde.
bool readTelegram(uint8_t dat[]) {
  // Message format (ESP2) starts with 0xA5 0x5A
  do {
    dat[0] = Serial2.read();
  } while (dat[0] != 0xa5);

  dat[1] = Serial2.read();
  if (dat[1] != 0x5a) {
    return false;
  }

  for (int i = 2; i < 14; i++) {
    dat[i] = Serial2.read();
  }

  uint8_t chksum = calcChksum(dat);
  if (chksum == dat[13]) {
    return true;
  } else {
    return false;
  }
}

////////////////////////
//// printTelegram ////
//////////////////////
// gibt Enocean-Telegramm als HEX an der Console aus.
void printTelegram(uint8_t dat[]) {
  for (int i = 0; i < 14; i++) {
    printByteHex(dat[i]);
  }
  Serial.println("");
}

////////////////////////
//// parseTelegram ////
//////////////////////
// Ordnet Byte zu
void parseTelegram(uint8_t tel[]) {
  uint8_t h_seq, len, org, stat;
  uint32_t id = 0; // 4 byte id
  uint32_t dat = 0;    // 4 byte data

  len = tel[2] & 0x1F;
  h_seq = tel[2] >> 5;

  org = tel[3];

  for (int i = 4; i < 8; i++) {
    dat = (dat << 8) + tel[i];
  }

  // read id
  for (int i = 8; i < 12; i++) {
    id = (id << 8) + tel[i];
  }

  stat = tel[12];

  Serial.println("h_seq: 0x" + String(h_seq, HEX) + ", length: " + String(len, DEC) + ", org: 0x" + String(org, HEX) +
                 ", data: 0x" + String(dat, HEX) + ", address: 0x" + String(id, HEX) + ", status: 0x" + String(stat, HEX));
}

////////////////////////
//// Wifi verbinden////
//////////////////////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


////////////////////////
//// Callback ////
//////////////////////
// callback wird aufgerufen wenn eine MQTT-Nachricht angekommen ist.
// Um die Aufgabe: LED mit ESP32 zu implementieren
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "esp32TCM320/Led") {
    Serial.print("Changing output to ");
    if (messageTemp == "on") {
      Serial.println("Schaltet LED ein");
      digitalWrite(ledPin, HIGH);
    }
    else if (messageTemp == "off") {
      Serial.println("Schaltet LED aus");
      digitalWrite(ledPin, LOW);
    }
  }
}

////////////////////////
//// readSoftwareAPIVersion ////
//////////////////////
// liest die Version und API aus
void readSoftwareAPIVersion(uint8_t temp[])
{
  // telegram to read software version
  uint8_t dat[14] = {0xa5, 0x5a, 0xab, 0x4b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  dat[13] = calcChksum(dat);
  // Command zu lesen die Software-Version
  Serial2.write(dat, 14);
  // result is {0xA5, 0x5A, 0x8B, 0x8C, TCM SW Version Pos.1, TCM SW Version Pos.2, TCM SW Version Pos.3, TCM SW Version Pos.4
  //            API Version Pos.1, API Version Pos.2, API Version Pos.3, API Version Pos.4, X, ChkSum }
  if (readTelegram(temp)) {
    Serial.println("telegram to read software version Enocean TCM320 software version: ");
    printTelegram(temp);
    Serial.print("Enocean Software Version: ");
    Serial.print(temp[4]);
    Serial.print(".");
    Serial.print(temp[5]);
    Serial.print(".");
    Serial.print(temp[6]);
    Serial.print(".");
    Serial.println(temp[7]);
    Serial.print("Enocean API Version: ");
    Serial.print(temp[8]);
    Serial.print(".");
    Serial.print(temp[9]);
    Serial.print(".");
    Serial.print(temp[10]);
    Serial.print(".");
    Serial.println(temp[11]);
  }
}

////////////////////////
//// readBASEID ////
//////////////////////
// liest die BASEID aus
void readBASEID(uint8_t temp[], uint8_t deviceID[])
{
  // telegram to read BASEID version
  uint8_t dat[14] = {0xa5, 0x5a, 0xab, 0x58, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  dat[13] = calcChksum(dat);
  // Command zu lesen die Software-Version
  Serial2.write(dat, 14);
  // result is {0xA5, 0x5A, 0x8B, 0x98, BaseIDByte3, BaseIDByte2, BaseIDByte1, BaseIDByte0
  //           X, X,X, X, X, ChkSum }
  if (readTelegram(temp)) {
    Serial.println("telegram to read Enocean TCM320 BaseID: ");
    printTelegram(temp);
    Serial.print("Enocean TCM320 BaseID: ");
    Serial.print(temp[4], HEX);
    Serial.print(" ");
    Serial.print(temp[5], HEX);
    Serial.print(" ");
    Serial.print(temp[6], HEX);
    Serial.print(" ");
    Serial.println(temp[7], HEX);
    for (int i = 0; i < 4; i++)
    {
      deviceID[i] = temp[4 + i];
    }
  }
}

////////////////////////
//// read_DeviceID ////
//////////////////////
// liest die DeviceID aus
void read_DeviceID(uint8_t temp[], uint8_t device_id[])
{
  for (int i = 0; i < 4; i++)
  {
    device_id[i] = temp[8 + i];
  }
}


////////////////////////
//// Setup ////
//////////////////////
//// put your setup code here, to run once:
void setup() {
  Serial.begin(115200); // Console
  Serial2.begin(9600, SERIAL_8N1, RXD2_pin, TXD2_pin); // EnOcean
  Serial.println("Serial2 Txd is on pin: " + String(TXD2_pin));
  Serial.println("Serial2 Rxd is on pin: " + String(RXD2_pin));
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  readSoftwareAPIVersion(softwareVersion_telegramm);
  readBASEID(BaseID_telegram, BaseID);

  /* Reset HP20x_dev */
  TH02.begin();
  delay(100);

  /* Determine TH02_dev is available or not */
  Serial.println("TH02_dev is available.\n");

  for (int i = 0; i < 4; i++)
  {
    device_id = device_id + String(BaseID[i], HEX);
  }

  //Spaeter fuer die Aufgabe mit LED
  pinMode(ledPin, OUTPUT);
}


////////////////////////
//// reconnect ////
//////////////////////
// Um die Verbindung zu MQTT_Broker wiederherstellen
// Zum Beginn wird es einmal aufgerufen, um die MQTT_Verbindung aufzubauen.
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Subscribe um die LED-Command zu hören.
      client.subscribe("esp32TCM320/Led");
      client.publish(enocean_Client, clientID);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000); 
    }
  }
}

////////////////////////
//// sentToEnocean ////
//////////////////////
//
void sentToEnocean(float tmp) {
  uint8_t telegram[14];
  telegram[0] = 0xA5;
  telegram[1] = 0x5A;

  // 8bits H_SEQ + LENGTH
  // H_SEQ: 0b011 Transmit radio Telegram (TRT) Host -> Module -> Air
  // LENGTH: Number of octets following the header octet (11 dec)
  telegram[2] = 0x6B;
  telegram[ORG_BYTE] = 0x07;
  telegram[D_BYTE_3] = 0x00;
  telegram[D_BYTE_2] = 0x00;

  //For 4BS Telegram
  // Temperature Sensor: ORG-FUNC-TYPE: 07-02-05;
  // Range: 0 to 40 C
  // D_BYTE_1: Temperature 0...40°C, linear n=255...0
  // DB_0.BIT_3:  0b0 Teach-in telegram
  //              0b1 Data telegram
  uint8_t temp = 255 - ((tmp * 255) / 40);
  telegram[D_BYTE_1] = temp;
  telegram[D_BYTE_0] = 0x08;
  uint8_t id[4];
  for (uint8_t i = 0; i < 4; i++)
  {
    telegram[ID_BYTE_3 + i] = BaseID_telegram[4 + i];
  }
  telegram[STATUS_BYTE] = 0x00;
  telegram[CHECKSUM_BYTE] = calcChksum(telegram);

  Serial.println("Temperature Telegram to sent:");
  // Serial.println(temp);
  printTelegram(telegram);

  //send Telegram to TCM320
  Serial2.write(telegram, 14);

  // Before sending commands via the serial interface please always wait for the re-sponse to the previous command from the module.
  // The reaction time is below 5ms.
  // Be aware that an already received radio telegram might (concurrently to the command) be sent through the serial port before the command gets processed.
  delay(10);




  uint8_t antwort[14];
  if (readTelegram(antwort)) {
    // Serial.println("Answer-Telegram: ");
    // printTelegram(antwort);
    if (antwort[0] == 0xA5 && antwort[1] == 0x5A && antwort[2] == 0x8B && antwort[3] == 0x58)
    {
      Serial.println("Telegram sending is completed.");
      Serial.println("");
    }
    else
    {
      Serial.println("error by sending telegram");
      Serial.println("");
    }
  } else {
    Serial.println("error reading telegram");
    Serial.println("");
  }
}

////////////////////////
//// sendToMQTT ////
//////////////////////
//
void sendToMQTT(uint8_t tel[])
{
  String sentMessage = "";
  if (tel[ORG_BYTE] == 0x05 )
  {
    sentMessage = sentMessage + "ORG: {'description': 'RPS Telegram', 'raw_value': " + tel[ORG_BYTE] + "}" + "\n";
    // if((tel[D_BYTE_3] & 0xE0) == 0x00, 10, 20,30) AI A0 BI B0
    // Fuer R1
    if ((tel[D_BYTE_3] & 0xE0) == 0x00)
    {
      sentMessage = sentMessage + "R1: {'description': 'Rocker 1st action', 'unit': '', 'value': 'Button AI', 'raw_value': " + String((tel[D_BYTE_3] >> 5)) + "}" + "\n";
    }
    else if ((tel[D_BYTE_3] & 0xE0) == 0x20)
    {
      sentMessage = sentMessage + "R1: {'description': 'Rocker 1st action', 'unit': '', 'value': 'Button AO','raw_value': " + String((tel[D_BYTE_3] >> 5)) + "}" + "\n";
    }
    else if ((tel[D_BYTE_3] & 0xE0) == 0x40)
    {
      sentMessage = sentMessage + "R1: {'description': 'Rocker 1st action', 'unit': '', 'value': 'Button BI','raw_value': " + String((tel[D_BYTE_3] >> 5)) + "}" + "\n";
    }
    else if ((tel[D_BYTE_3] & 0xE0) == 0x60)
    {
      sentMessage = sentMessage + "R1: {'description': 'Rocker 1st action', 'unit': '', 'value': 'Button B0','raw_value': " + String((tel[D_BYTE_3] >> 5)) + "}" + "\n";
    }

    // Fuer EB
    if ((tel[D_BYTE_3] & 0x10) == 0x00)
    {
      sentMessage = sentMessage + "EB: {'description': 'Energy Bow', 'unit': '', 'value': 'released', 'raw_value': " + String(((tel[D_BYTE_3] & 0x10) >> 4)) + "}" + "\n";
    }
    else
    {
      sentMessage = sentMessage + "EB: {'description': 'Energy Bow', 'unit': '', 'value': 'pressed', 'raw_value': " + String(((tel[D_BYTE_3] & 0x10) >> 4)) + "}" + "\n";
    }

    // Fuer R2
    if ((tel[D_BYTE_3] & 0x0E) == 0x00)
    {
      sentMessage = sentMessage + "R2: {'description': 'Rocker 2st action', 'unit': '', 'value': 'Button AI', 'raw_value': " + String((tel[D_BYTE_3] & 0x0E)) + "}" + "\n";
    }
    else if ((tel[D_BYTE_3] & 0x0E) == 0x02)
    {
      sentMessage = sentMessage + "R2: {'description': 'Rocker 2st action', 'unit': '', 'value': 'Button AO','raw_value': " + String((tel[D_BYTE_3] & 0x0E)) + "}" + "\n";
    }
    else if ((tel[D_BYTE_3] & 0x0E) == 0x04)
    {
      sentMessage = sentMessage + "R2: {'description': 'Rocker 2st action', 'unit': '', 'value': 'Button BI','raw_value': " + String((tel[D_BYTE_3] & 0x0E)) + "}" + "\n";
    }
    else if ((tel[D_BYTE_3] & 0x0E) == 0x06)
    {
      sentMessage = sentMessage + "R2: {'description': 'Rocker 2st action', 'unit': '', 'value': 'Button B0','raw_value': " + String((tel[D_BYTE_3] & 0x0E)) + "}" + "\n";
    }

    //Fuer 2nd Action
    if ((tel[D_BYTE_3] & 0x01) == 0x00)
    {
      sentMessage = sentMessage + "SA: {'description': '2nd Action', 'unit': '', 'value': 'no 2nd Action', 'raw_value': " + String((tel[D_BYTE_3]) & 0x01) + "}" + "\n";
    }
    else
    {
      sentMessage = sentMessage + "SA: {'description': '2nd Action', 'unit': '', 'value': '2nd Action valid', 'raw_value': " + String((tel[D_BYTE_3]) & 0x01) + "}" + "\n";
    }

    //Fuer T21
    if ((tel[STATUS_BYTE] & 0x20) == 0x20)
    {
      sentMessage = sentMessage + "T21: {'description': 'T21', 'unit': '', 'value': 'True', 'raw_value': " + String((tel[STATUS_BYTE] & 0x20) >> 5) + "}" + "\n";
    }
    else
    {
      sentMessage = sentMessage + "T21: {'description': 'T21', 'unit': '', 'value': 'False', 'raw_value': " + String((tel[STATUS_BYTE] & 0x20) >> 5) + "}" + "\n";
    }


    //Fuer NU
    if ((tel[STATUS_BYTE] & 0x10) == 0x10)
    {
      sentMessage = sentMessage + "NU: {'description': 'NU', 'unit': '', 'value': 'True', 'raw_value': " + String((tel[STATUS_BYTE] & 0x10) >> 4)  + "}" + "\n";
    }
    else
    {
      sentMessage = sentMessage + "NU: {'description': 'NU', 'unit': '', 'value': 'False', 'raw_value': " + String((tel[STATUS_BYTE] & 0x10) >> 4) + "}" + "\n";
    }


  }
  else if (tel[ORG_BYTE] == 0x06)
  {
    // nicht vorhanden
  }
  else if (tel[ORG_BYTE] == 0x07)
  {
    sentMessage = sentMessage + "ORG: {'description': '4BS Telegram', 'raw_value': " + tel[ORG_BYTE] + "}" + "\n";
    double tmp = (((double)255 - tel[D_BYTE_1]) * 40) / 255;
    sentMessage = sentMessage + "TMP: {'description': 'Temperature Sensors', 'unit': '°C', 'value': '" + String(tmp) + "'" + "'raw_value': " + String(tel[D_BYTE_1], HEX) + "}" + "\n";
  }
  else
  {
    sentMessage = "UNBEKANNTER SENSOR!!!";
  }
  Serial.println(sentMessage);
  client.publish(enocean_topic, sentMessage.c_str());

}


////////////////////////
//// loop ////
//////////////////////
// put your main code here, to run repeatedly:
void loop() {
  //Millis(): Anzahl der Millisekunden seit dem Programmstart. Datentyp: unsigned long.
  long now = millis();
  // Schickt Nachricht nach jede 5s aus.
  if (now - lastMsg > 5000) {
    lastMsg = now;
    float temper = TH02.ReadTemperature();
    Serial.println("Temperature: ");
    Serial.print(temper);
    Serial.println(" C\r");
    sentToEnocean(temper);
    float humidity = TH02.ReadHumidity();
    Serial.println("Humidity: ");
    Serial.print(humidity);
    Serial.println("%\r\n");
    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    String humidity_topic = "enocean2mqtt/" + device_id + "/humidity";
    if (!client.connected()) {
      reconnect();
    }
    client.publish(humidity_topic.c_str(), humString, true);
  }
}
