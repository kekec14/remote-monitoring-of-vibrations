/***************************************************
Adafruit MQTT Library ESP8266 Example

Must use ESP8266 Arduino from:
https://github.com/esp8266/Arduino

Works great with Adafruit's Huzzah ESP board & Feather
----> https://www.adafruit.com/product/2471
----> https://www.adafruit.com/products/2821

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Tony DiCola for Adafruit Industries.
MIT license, all text above must be included in any redistribution
****************************************************/
//VIRI
//Program je spisan s pomočjo zgleda - mqtt_esp8266 iz knjižnice Adafruit MQTT Library - https://github.com/adafruit/Adafruit_MQTT_Library
// https://forum.arduino.cc/index.php?topic=288234.0 - Osnove serijske komunikacije


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZAČETEK PROGRAMA
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Uvoz knjižnic, ki so potrebne za delovanje programa
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NASTAVITVE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Definiranje parametrov za povezavo na Wi-Fi omrežje ter MQTT protokol. Obvezno izpolniti, za delovanje programa
/************************* Parametri za brezžično omrežje *********************************/

#define WLAN_SSID       "doma"//"Domen's iPhone"
#define WLAN_PASS       "domen2po"//"domen123"

/************************* MATT parametri *********************************/

#define AIO_SERVER      "192.168.0.113"//"89.142.62.73"
#define AIO_SERVERPORT  1884                  
#define AIO_USERNAME    "DP"
#define AIO_KEY         "...your AIO key..." // nepotrebno izpolniti

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Store the MQTT server, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_SERVER[] PROGMEM    = AIO_SERVER;
const char MQTT_USERNAME[] PROGMEM  = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = AIO_KEY;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, AIO_SERVERPORT, MQTT_USERNAME, MQTT_PASSWORD);

/****************************** Topic ***************************************/

// Nastavitev teme za objavljanje
// Zavedati se je potrebno da oddajamo v topic DP/zajem/
const char PHOTOCELL_FEED[] PROGMEM = AIO_USERNAME "/zajem";
Adafruit_MQTT_Publish photocell = Adafruit_MQTT_Publish(&mqtt, PHOTOCELL_FEED, MQTT_QOS_1); // MQTT_QOS_1

/*************************** ZAČETEK PROGRAMA ************************************/

const int POWER_LED_PIN = D5; // določitev PRIKLJUČKA na katerega je povezana LED dioda, ki nakazuje da je Wi-Fi modul uspoešno povezan
const byte numChars = 250; //velikost vektorja za shranjevanje sprejetih znakov
char prenos_podatkov[numChars];  // Vektor v katerega se shranjujejo vrednosti, ki so sprejete preko serijske komunikacije
boolean newData = false;

void MQTT_connect(); // povezava na Wi-Fi omrežje

void setup() {
  Serial.begin(115200); // Konfiguracija serijske komunikacije z mikrokrmilnikom Teensy 3.2
  delay(10);

  pinMode(POWER_LED_PIN, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  //povezava na Ei-Fi omrežje
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  //indikacija, da je merilnik povezan na Wi-fi omrežje
  digitalWrite(POWER_LED_PIN, HIGH);  
  delay(150);                      
  digitalWrite(POWER_LED_PIN, LOW);  
  delay(150);
  digitalWrite(POWER_LED_PIN, HIGH); 
  delay(150);                      
  digitalWrite(POWER_LED_PIN, LOW);   
  digitalWrite(POWER_LED_PIN, HIGH);  
  delay(150);                      
  digitalWrite(POWER_LED_PIN, LOW);   
  delay(150);
  digitalWrite(POWER_LED_PIN, HIGH); 
  delay(150);                    
  digitalWrite(POWER_LED_PIN, LOW); 
  
  MQTT_connect();
  //Signal posredniku/borkerju, da je Wi-Fi modul uspešno povezan
  photocell.publish("Sem povezan");

}


void loop() {
  //funkcija, ki poveže Wi-Fi modul z brezčičnim omrežjem v primeru, da izgubi povezavo.
  MQTT_connect();
  //Funkcija, ki bere podatke/meritve, ki jih mikrokrmilnik Teensy pošilja na Wifi modul
  Branje();
  //Funkcija, ki odda sprejete podatke/meritve preko MQTT protokola na Raspberry pi, kjer se izvaja obdelava podatkov
  Oddajanje();
}

void Branje() { // vir  https://forum.arduino.cc/index.php?topic=288234.0
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (rc != endMarker) {
      prenos_podatkov[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      prenos_podatkov[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
}

void Oddajanje() {
  if (newData == true) {
    photocell.publish(prenos_podatkov); // objava sprejetih meritev v temo DP/zajem
    newData = false;
    Serial.print("1"); //pošiljanje znaka, da so bili podatki uspešno poslani in da je Wi-Fi modul pripravljen na novo pošiljko meritev
  }
}

// Funkcija, ki poveže Wi-fi modul z MQTT protokolom v primeru prekinitve
// del knjižnice Adafruit MQTT Library
void MQTT_connect() {
  int8_t ret;

  // Prekinitev funkcije, če je povezava z MQTT vzpostavljena
  if (mqtt.connected()) {
    digitalWrite(POWER_LED_PIN, HIGH);
    return;
  }

  uint8_t retries = 3;
  //poizkušanje vzposatavitve povezave z MQTT
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    mqtt.disconnect();
    for (int i=0; i <= 2; i++){
      digitalWrite(POWER_LED_PIN, HIGH);  // utripanje LED
      delay(1000);                      
      digitalWrite(POWER_LED_PIN, LOW);   // utripanje LED
      delay(1000);
    }
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      for (int i=0; i <= 20; i++){
        digitalWrite(POWER_LED_PIN, HIGH);  // turn on LED with voltage HIGH
        delay(100);                      // wait one second
        digitalWrite(POWER_LED_PIN, LOW);   // turn off LED with voltage LOW
        delay(100);
      }
      while (1);
    }
  }
}

