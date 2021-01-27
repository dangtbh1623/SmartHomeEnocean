#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

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

const char *ssid = "DangTrinhUnity";      // Personal network SSID
const char *wifi_password = "Joy-IT23!9"; //  Personal network password

const char *mqtt_server = "192.168.0.17"; // IP of the MQTT broker

// MQTT: topics
const char *enocean_topic = "enocean2mqtt/telegramm";
const char *enocean_Client = "enocean2mqtt/Client";
const char *enocean_light_state = "enocean2mqtt/led/status";
const char *enocean_light_command = "enocean2mqtt/led/switch";

// payloads by default (on/off)
const char *light_on = "ON";
const char *light_off = "OFF";

// Status of LED
bool light_status = false;

const char *mqtt_username = "homeassistant";                                                    // MQTT username
const char *mqtt_password = "ve1Ahs9zib2tog0ahv5oewaim8weeruaW3shahPie9ie9ahsheebaeh3iujaoras"; // MQTT password

const char *clientID = "ESP32mitTCM320"; // MQTT client ID

uint8_t softwareVersion_telegramm[14];
uint8_t BaseID_telegram[14];

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

// Definiert Pins für RGB-Led
constexpr uint8_t ledR = 18;
constexpr uint8_t ledG = 10;
constexpr uint8_t ledB = 19;

////////////////////////
//// publishStateLed  ////
//////////////////////
// send Zustand des LEDs zu MQTT-Broker
void publishStateLed()
{
  if (light_status)
  {
    client.publish(enocean_light_state, light_on, true);
  }
  else
  {
    client.publish(enocean_light_state, light_off, true);
  }
}

////////////////////////
//// setStateLed  ////
//////////////////////
// function called to turn on/off the light
void setStateLed()
{
  if (light_status)
  {
    digitalWrite(ledR, LOW); // turn the LED RED ON by making the voltage LOW
    Serial.println("INFO: Turn Red LED on...");
  }
  else
  {
    digitalWrite(ledR, HIGH); // turn the LED RED OFF by making the voltage HIGH
    Serial.println("INFO: Turn Red LED off...");
  }
}

////////////////////////
//// printByteHex  ////
//////////////////////
// gibt Data des Typs Uint8 als HEX aus
void printByteHex(uint8_t val)
{
  Serial.print(val, HEX);
  Serial.print(" ");
}

////////////////////////
//// calcChksum  ////
//////////////////////
// ermittelt die Checksumnummer eines Enocean-Telegramms
uint8_t calcChksum(uint8_t dat[])
{
  uint32_t chksum = 0;
  for (int i = 2; i < 13; i++)
  {
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
bool readTelegram(uint8_t dat[])
{
  // Message format (ESP2) starts with 0xA5 0x5A
  do
  {
    dat[0] = Serial2.read();
  } while (dat[0] != 0xa5);

  dat[1] = Serial2.read();
  if (dat[1] != 0x5a)
  {
    return false;
  }

  for (int i = 2; i < 14; i++)
  {
    dat[i] = Serial2.read();
  }

  uint8_t chksum = calcChksum(dat);
  if (chksum == dat[13])
  {
    return true;
  }
  else
  {
    return false;
  }
}

////////////////////////
//// printTelegram ////
//////////////////////
// gibt Enocean-Telegramm als HEX an der Console aus.
void printTelegram(uint8_t dat[])
{
  for (int i = 0; i < 14; i++)
  {
    printByteHex(dat[i]);
  }
  Serial.println("");
}

////////////////////////
//// parseTelegram ////
//////////////////////
// Ordnet Byte zu
void parseTelegram(uint8_t tel[])
{
  uint8_t h_seq, len, org, stat;
  uint32_t id = 0;  // 4 byte id
  uint32_t dat = 0; // 4 byte data

  len = tel[2] & 0x1F;
  h_seq = tel[2] >> 5;

  org = tel[3];

  for (int i = 4; i < 8; i++)
  {
    dat = (dat << 8) + tel[i];
  }

  // read id
  for (int i = 8; i < 12; i++)
  {
    id = (id << 8) + tel[i];
  }

  stat = tel[12];

  Serial.println("h_seq: 0x" + String(h_seq, HEX) + ", length: " + String(len, DEC) + ", org: 0x" + String(org, HEX) +
                 ", data: 0x" + String(dat, HEX) + ", address: 0x" + String(id, HEX) + ", status: 0x" + String(stat, HEX));
}

////////////////////////
//// Wifi verbinden////
//////////////////////
void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

////////////////////////
//// setup_RGBLed ////
//////////////////////
void setup_RGBLed()
{
  delay(10);
  // We start by setting Pinmode
  Serial.println();
  Serial.print("Setting Pins for RGB-Led ...");
  Serial.println();

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);

  Serial.println("");

  delay(1000);
  Serial.println("Testing the RGB-LED...");
  digitalWrite(ledR, HIGH); // turn the LED RED OFF by making the voltage LOW
  delay(10);
  digitalWrite(ledG, HIGH); // turn the LED GREEN OFF by making the voltage LOW
  delay(10);
  digitalWrite(ledB, HIGH); // turn the LED BLUE OFF by making the voltage LOW
  delay(10);
  Serial.println("RGB-Led Setup is successful");
}

////////////////////////
//// Callback ////
//////////////////////
// callback wird aufgerufen wenn eine MQTT-Nachricht angekommen ist.
// Um die Aufgabe: LED mit ESP32 zu implementieren
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic).equals(enocean_light_command))
  {
    if (String(messageTemp).equals(light_on))
    {
      if (light_status != true)
      {
        light_status = true;
        setStateLed();
        publishStateLed();
      }
    }
    else if (String(messageTemp).equals(light_off))
    {
      if (light_status != false)
      {
        light_status = false;
        setStateLed();
        publishStateLed();
      }
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
  if (readTelegram(temp))
  {
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
void readBASEID(uint8_t temp[])
{
  // telegram to read BASEID version
  uint8_t dat[14] = {0xa5, 0x5a, 0xab, 0x58, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  dat[13] = calcChksum(dat);
  // Command zu lesen die Software-Version
  Serial2.write(dat, 14);
  // result is {0xA5, 0x5A, 0x8B, 0x98, BaseIDByte3, BaseIDByte2, BaseIDByte1, BaseIDByte0
  //           X, X,X, X, X, ChkSum }
  if (readTelegram(temp))
  {
    Serial.println("telegram to read software version Enocean TCM320 BaseID: ");
    printTelegram(temp);
    Serial.print("Enocean TCM320 BaseID: ");
    Serial.print(temp[4], HEX);
    Serial.print(" ");
    Serial.print(temp[5], HEX);
    Serial.print(" ");
    Serial.print(temp[6], HEX);
    Serial.print(" ");
    Serial.println(temp[7], HEX);
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
void setup()
{
  Serial.begin(115200);                                // Console
  Serial2.begin(9600, SERIAL_8N1, RXD2_pin, TXD2_pin); // EnOcean
  Serial.println("Serial2 Txd is on pin: " + String(TXD2_pin));
  Serial.println("Serial2 Rxd is on pin: " + String(RXD2_pin));
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  readSoftwareAPIVersion(softwareVersion_telegramm);
  readBASEID(BaseID_telegram);

  //Einrichten fuer die Aufgabe mit LED
  setup_RGBLed();
  pinMode(ledPin, OUTPUT);
}

////////////////////////
//// reconnect ////
//////////////////////
// Um die Verbindung zu MQTT_Broker wiederherstellen
// Zum Beginn wird es einmal aufgerufen, um die MQTT_Verbindung aufzubauen.
void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID, mqtt_username, mqtt_password))
    {
      Serial.println("connected");
      // Subscribe um die LED-Command zu hören.
      client.subscribe(enocean_light_command);
      client.publish(enocean_Client, clientID);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

////////////////////////
//// sendToMQTT ////
//////////////////////
//
void sendToMQTT(uint8_t tel[])
{
  String sentMessage = "";
  if (tel[ORG_BYTE] == 0x05)
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
      sentMessage = sentMessage + "NU: {'description': 'NU', 'unit': '', 'value': 'True', 'raw_value': " + String((tel[STATUS_BYTE] & 0x10) >> 4) + "}" + "\n";
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
//// sendToMQTT_v2 ////
//////////////////////
//
void sendToMQTT_v2(uint8_t tel[])
{
  String sentMessage = "";
  uint8_t id[4];
  read_DeviceID(tel, id);
  String device_id = "0x";
  for (int i = 0; i < 4; i++)
  {
    device_id = device_id + String(id[i], HEX);
  }
  String topic_json = "enocean2mqtt/" + device_id + "/JSON";
  if (tel[ORG_BYTE] == 0x05)
  {
    sentMessage = sentMessage + "{\"ORG\": {\"description\": \"RPS Telegram\", \"raw\": \"" + tel[ORG_BYTE] + "\"}, ";
    // if((tel[D_BYTE_3] & 0xE0) == 0x00, 10, 20,30) AI A0 BI B0
    // Fuer R1
    String topic_R1 = "enocean2mqtt/" + device_id + "/R1";
    if ((tel[D_BYTE_3] & 0xE0) == 0x00)
    {
      sentMessage = sentMessage + "\"R1\": {\"value\": \"Button AI\", \"raw\": \"" + String((tel[D_BYTE_3] >> 5)) + "\"}, ";
      client.publish(topic_R1.c_str(), (String((tel[D_BYTE_3] >> 5))).c_str());
    }
    else if ((tel[D_BYTE_3] & 0xE0) == 0x20)
    {
      sentMessage = sentMessage + "\"R1\": {\"value\": \"Button AO\", \"raw\": \"" + String((tel[D_BYTE_3] >> 5)) + "\"}, ";
      client.publish(topic_R1.c_str(), (String((tel[D_BYTE_3] >> 5))).c_str());
    }
    else if ((tel[D_BYTE_3] & 0xE0) == 0x40)
    {
      sentMessage = sentMessage + "\"R1\": {\"value\": \"Button BI\", \"raw\": \"" + String((tel[D_BYTE_3] >> 5)) + "\"}, ";
      client.publish(topic_R1.c_str(), (String((tel[D_BYTE_3] >> 5))).c_str());
    }
    else if ((tel[D_BYTE_3] & 0xE0) == 0x60)
    {
      sentMessage = sentMessage + "\"R1\": {\"value\": \"Button B0\", \"raw\": \"" + String((tel[D_BYTE_3] >> 5)) + "\"}, ";
      client.publish(topic_R1.c_str(), (String((tel[D_BYTE_3] >> 5))).c_str());
    }

    // Fuer EB
    String topic_EB = "enocean2mqtt/" + device_id + "/EB";

    //released
    if ((tel[D_BYTE_3] & 0x10) == 0x00)
    {
      sentMessage = sentMessage + "\"EB\": {\"value\": \"released\", \"raw\": \"" + String(((tel[D_BYTE_3] & 0x10) >> 4)) + "\"}, ";
      client.publish(topic_EB.c_str(), (String(((tel[D_BYTE_3] & 0x10) >> 4))).c_str());
      // Wenn Device die PTM200 mit ID: 0x0278752 ist, schaltet das RGB-LED aus.
      if (device_id == "0x0278752")
      {
        Serial.println("Turn off RGB-LED");
        digitalWrite(ledR, HIGH); // turn the LED RED OFF by making the voltage LOW
        digitalWrite(ledG, HIGH); // turn the LED GREEN OFF by making the voltage LOW
        digitalWrite(ledB, HIGH); // turn the LED BLUE OFF by making the voltage LOW
      }
    }

    //pressed
    else
    {
      sentMessage = sentMessage + "\"EB\": {\"value\": \"pressed\", \"raw\": \"" + String(((tel[D_BYTE_3] & 0x10) >> 4)) + "\"}, ";
      client.publish(topic_EB.c_str(), (String(((tel[D_BYTE_3] & 0x10) >> 4))).c_str());
      // Wenn Device die PTM200 mit ID: 0x0278793 ist, werden folgende Falls berücksichtigt.
      if (device_id == "0x0278793")
      {
        Serial.println("Turn on LED: ");
        if ((tel[D_BYTE_3] & 0xE0) == 0x00)
        {
          Serial.println("RED: ");
          digitalWrite(ledR, LOW); // turn the LED RED ON by making the voltage LOW
        }
        else if ((tel[D_BYTE_3] & 0xE0) == 0x20)
        {
          Serial.println("GREEN: ");
          digitalWrite(ledG, LOW); // turn the LED RED ON by making the voltage LOW
        }
        else if ((tel[D_BYTE_3] & 0xE0) == 0x60)
        {
          Serial.println("BLUE: ");
          digitalWrite(ledB, LOW); // turn the LED RED ON by making the voltage LOW
        }
        else if ((tel[D_BYTE_3] & 0xE0) == 0x40)
        {
          Serial.println("Turn LED off");
          digitalWrite(ledR, HIGH); // turn the LED RED OFF by making the voltage LOW
          digitalWrite(ledG, HIGH); // turn the LED GREEN OFF by making the voltage LOW
          digitalWrite(ledB, HIGH); // turn the LED BLUE OFF by making the voltage LOW
        }
      }
      // Wenn Device die PTM200 mit ID: 0x0278752 ist, schaltet das RGB-LED ein.
      else if (device_id == "0x0278752")
      {
        Serial.println("Turn on RGB-LED");
        digitalWrite(ledR, LOW); // turn the LED RED ON by making the voltage LOW
        digitalWrite(ledG, LOW); // turn the LED GREEN ON by making the voltage LOW
        digitalWrite(ledB, LOW); // turn the BLUE RED ON by making the voltage LOW
      }
    }

    // Fuer R2
    String topic_R2 = "enocean2mqtt/" + device_id + "/R2";
    if ((tel[D_BYTE_3] & 0x0E) == 0x00)
    {
      sentMessage = sentMessage + "\"R2\": {\"value\": \"Button AI\", \"raw\": \"" + String((tel[D_BYTE_3] & 0x0E)) + "\"}, ";
      client.publish(topic_R2.c_str(), (String((tel[D_BYTE_3] & 0x0E))).c_str());
    }
    else if ((tel[D_BYTE_3] & 0x0E) == 0x02)
    {
      sentMessage = sentMessage + "\"R2\": {\"value\": \"Button AO\", \"raw\": \"" + String((tel[D_BYTE_3] & 0x0E)) + "\"}, ";
      client.publish(topic_R2.c_str(), (String((tel[D_BYTE_3] & 0x0E))).c_str());
    }
    else if ((tel[D_BYTE_3] & 0x0E) == 0x04)
    {
      sentMessage = sentMessage + "\"R2\": {\"value\": \"Button BI\", \"raw\": \"" + String((tel[D_BYTE_3] & 0x0E)) + "\"}, ";
      client.publish(topic_R2.c_str(), (String((tel[D_BYTE_3] & 0x0E))).c_str());
    }
    else if ((tel[D_BYTE_3] & 0x0E) == 0x06)
    {
      sentMessage = sentMessage + "\"R2\": {\"value\": \"Button B0\", \"raw\": \"" + String((tel[D_BYTE_3] & 0x0E)) + "\"}, ";
      client.publish(topic_R2.c_str(), (String((tel[D_BYTE_3] & 0x0E))).c_str());
    }

    //Fuer 2nd Action
    String topic_SA = "enocean2mqtt/" + device_id + "/SA";
    if ((tel[D_BYTE_3] & 0x01) == 0x00)
    {
      sentMessage = sentMessage + "\"SA\": {\"value\": \"no 2nd Action\", \"raw\": \"" + String((tel[D_BYTE_3]) & 0x01) + "\"}}";
      client.publish(topic_SA.c_str(), (String((tel[D_BYTE_3]) & 0x01)).c_str());
    }
    else
    {
      sentMessage = sentMessage + "\"SA\": {\"value\": \"2nd Action valid\", \"raw\": \"" + String((tel[D_BYTE_3]) & 0x01) + "\"}}";
      client.publish(topic_SA.c_str(), (String((tel[D_BYTE_3]) & 0x01)).c_str());
    }

    //Fuer T21
    String topic_T21 = "enocean2mqtt/" + device_id + "/T21";
    if ((tel[STATUS_BYTE] & 0x20) == 0x20)
    {
      client.publish(topic_T21.c_str(), (String((tel[STATUS_BYTE] & 0x20) >> 5)).c_str());
    }
    else
    {
      client.publish(topic_T21.c_str(), (String((tel[STATUS_BYTE] & 0x20) >> 5)).c_str());
    }

    //Fuer NU
    String topic_NU = "enocean2mqtt/" + device_id + "/NU";
    if ((tel[STATUS_BYTE] & 0x10) == 0x10)
    {
      client.publish(topic_NU.c_str(), (String((tel[STATUS_BYTE] & 0x10) >> 4)).c_str());
    }
    else
    {
      client.publish(topic_NU.c_str(), (String((tel[STATUS_BYTE] & 0x10) >> 4)).c_str());
    }
  }
  else if (tel[ORG_BYTE] == 0x06)
  {
    // nicht vorhanden
  }
  else if (tel[ORG_BYTE] == 0x07)
  {
    String topic_TMP = "enocean2mqtt/" + device_id + "/TMP";
    sentMessage = sentMessage + "{\"ORG\": {\"description\": \"4BS Telegram\", \"raw\": \"" + tel[ORG_BYTE] + "\"}, ";
    double tmp = (((double)255 - tel[D_BYTE_1]) * 40) / 255;
    sentMessage = sentMessage + "\"TMP\": {\"description\": \"temperature\", \"unit\": \"°C\", \"value\": \"" + String(tmp) + "\", " + "\"raw\": \"" + String(tel[D_BYTE_1], HEX) + "\"}}" + "\n";
    client.publish(topic_TMP.c_str(), (String(tmp)).c_str());
  }
  else
  {
    sentMessage = "UNBEKANNTER SENSOR!!!";
  }
  Serial.println(sentMessage);
  client.publish(topic_json.c_str(), sentMessage.c_str(), true);
  client.publish(enocean_topic, sentMessage.c_str());
}

////////////////////////
//// loop ////
//////////////////////
// put your main code here, to run repeatedly:
void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  while (Serial2.available())
  {

    //führt Callback() (Topic zu hören) aus

    uint8_t dat[14];
    if (readTelegram(dat))
    {
      // Verbindet ESP32 mit MQTT_Broker
      if (!client.connected())
      {
        reconnect();
      }
      printTelegram(dat);
      sendToMQTT_v2(dat);
      //String telegram;
      //for(int i=0; i<14;i++)
      //{
      //telegram = telegram + " " + String(dat[i],HEX);
      //}
      //client.publish(enocean_Client, telegram.c_str());
      //Serial.println(", checksum ok");
      parseTelegram(dat);
    }
    else
    {
      Serial.println("error reading telegram");
    }
  }

  //Millis(): Anzahl der Millisekunden seit dem Programmstart. Datentyp: unsigned long.
  //long now = millis();
  // Schickt Nachricht nach jede 5s aus.
  //if (now - lastMsg > 5000) {
  //lastMsg = now;
  //client.publish("enocean2mqtt/test", "Text !!!!");

  // Temperature in Celsius
  // temperature = bme.readTemperature();
  // Uncomment the next line to set temperature in Fahrenheit
  // (and comment the previous temperature line)
  //temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit

  // Convert the value to a char array
  //char tempString[8];
  //dtostrf(temperature, 1, 2, tempString);
  //Serial.print("Temperature: ");
  //Serial.println(tempString);
  //client.publish("esp32/temperature", tempString);

  //humidity = bme.readHumidity();

  // Convert the value to a char array
  //char humString[8];
  //dtostrf(humidity, 1, 2, humString);
  //Serial.print("Humidity: ");
  //Serial.println(humString);
  //client.publish("esp32/humidity", humString);
  //}
}

{
  ORG: {
    "description": "4BS Telegram", 
    "raw": "7"
  }, 
  TMP: {
      "description": "temperature", 
      "unit": "C", 
      "value": "28.86", 
      "raw": "47"
  }
  }