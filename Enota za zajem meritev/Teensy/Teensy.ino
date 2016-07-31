// Copyright 2013 Tony DiCola (tony@tonydicola.com)

// Koda je spisana s pomočjo že napisanih knjižnic in zgledov:
// http://learn.adafruit.com/fft-fun-with-fourier-transforms/
// sdFat - zgled: dataLogger - https://github.com/greiman/SdFat/blob/master/SdFat/examples/LowLatencyLogger/LowLatencyLogger.ino
  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZAČETEK PROGRAMA
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Uvoz knjižnic, ki so potrebne za delovanje programa
#include <SPI.h>            // SPI knjižnica
#include <SdFat.h>            // Knjižnica za SD modul
#include <LiquidCrystal.h>    // Knjižnica za zaslon
#define error(msg) sd.errorHalt(F(msg)) // Izpis napake vezano na SD modul
#define FILE_BASE_NAME "Data"     // Poimenovanje datoteke na SD kartici (doda se še zaporedna številka) - šest črk ali manj
SdFat sd;               // Potrebno za delovanje sdFat knjižnice
SdFile file;              // Potrebno za delovanje sdFat knjižnice

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NASTAVITVE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// parametri vzorčenja
int Fs = 1024;       // Frekvenca vzorčenja
const int FFT_SIZE = 2048;       // Število meritev uporabljenih za FFT analizo

//parametri za Welch metodo - referenca http://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.welch.html#scipy.signal.welch
char* window[] = {"hann"}; //referenca http://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.get_window.html#scipy.signal.get_window
int nperseg = FFT_SIZE/4; //mora biti enako kot FFT_SIZE ali manj, privzeta nastavitev je 256
int noverlap= nperseg/2; //prekrivanje
int nfft = nperseg; // zero padding

// Nastavitve mikrokrmilnika
const int ANALOG_1 = 15;        // Analogni vhod - Z-os na pospeškomeru
const int ANALOG_READ_RESOLUTION = 13;  // Resolucija ADC pretvornika
const int ANALOG_READ_AVERAGING =16;  // Število vzorcev, ki so povprečeni
const int POWER_LED_PIN = 13;       // LED dioda na mikrokrmilniku - pokazatelj ali mikrokrmilnik deluje. pin 13 je integrirana LED dioda na mikrokrmilniku.
const uint8_t chipSelect = 10;          // Izbira CS pina pri SPI protokolu za SD modul
const int buttonSTART = 2;          //definiranje pina za začetek izvajanja kode
LiquidCrystal lcd(23,22,21,20,19,18);     // določitev priključkov LCD zaslona
IntervalTimer samplingTimer;      // Časovni interval zajema vzorcev - vir:: https://www.pjrc.com/teensy/td_timing_IntervalTimer.html

// Določitev vektorjev kamor se shranjujejo vrednosti pospeškomera
int surovi_podatki[FFT_SIZE];        // Časovna vrsta zajetih vrednosti iz pospeškomera
int sampleCounter = 0;          // Števec
char payload[128];            // String preko katerega se vrednosti FFT analize prenesejo na ESP8266, nato preko MQTT protokola v na RPI
unsigned long cas;            //čas

// GLAVNI PROGRAM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  char fileName[13] = FILE_BASE_NAME "00.json";
  pinMode(buttonSTART, INPUT);
  lcd.begin(16, 2);
  Serial.begin(115200);         // Konfiguracija serijske komunikacije s PC-jem
  Serial1.begin(115200);        // Konfiguracija serijske komunikacije preko UART1 (RX1 in TX1 priključek) z ESP 8266
  analogReadResolution(ANALOG_READ_RESOLUTION); //Določitev resolucije ADC - https://www.arduino.cc/en/Reference/AnalogReadResolution
  analogReadAveraging(ANALOG_READ_AVERAGING);

  // Inicializacija SD kartice
  if (!sd.begin(chipSelect, SPI_FULL_SPEED)) {  // Use SPI_FULL_SPEED for better performance.
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Check SD Card & "); // Začetek izvajanaj meritve
    lcd.setCursor(0, 1);
    lcd.print("Press RESTART   "); // Začetek izvajanaj meritve
    sd.initErrorHalt();
  }

  // Določanje imena datoteke, v katero se bodo zapisovali podatki - referenca Examples->sdFat->LowLatencyLogger.ino
  if (BASE_NAME_SIZE > 6) {
    error("FILE_BASE_NAME too long");
  }
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Can't create log"); 
      lcd.setCursor(0, 1);
      lcd.print("Empty SD Card   ");
      error("Can't create file name");
    }
  }
  if (!file.open(fileName, O_CREAT | O_WRITE | O_EXCL)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("File is open!   "); 
    lcd.setCursor(0, 1);
    lcd.print("Press RESTART   ");
    error("file.open");
  }

  // Vključitev LED diode - zank za začetek snemanja
  pinMode(POWER_LED_PIN, OUTPUT);
  digitalWrite(POWER_LED_PIN, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press button REC"); // Začetek izvajanaj meritve
  lcd.setCursor(0, 1);
  lcd.print("to start...     "); // Začetek izvajanaj meritve
  while (digitalRead(buttonSTART)==LOW) {} // Krožimo v zanki dokler ni pritisnjen START gumb

//Izpis na LCD zaslonu
  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("Log: ");
  lcd.setCursor(5, 0);
  lcd.print(fileName);
  lcd.setCursor(0, 1);
  lcd.print("Hold REC to stop");
  
//Pošiljanje parametrov na Wi-Fi modul
  Serial1.print(fileName);
  Serial1.print(",");
  Serial1.print(Fs);
  Serial1.print(",");
  Serial1.print(FFT_SIZE);
  Serial1.print(",");
  Serial1.print(window[0]);
  Serial1.print(",");
  Serial1.print(nperseg);
  Serial1.print(",");
  Serial1.print(noverlap);
  Serial1.print(",");
  Serial1.println(nfft);

  //Zapis glave na SD kartico
  writeHeader();
  // Začetek zajemanja
  samplingBegin();
}

void loop() {
  // V primeru, da je vektor z vrednostmi iz pospeškomera poln se podatki zapišejo na SD karico in pošljejo na Wi-Fi modul
  if (samplingIsDone()) {
    
    //zapis surovih podatkov na SD kartico
    SDwrite();

    //Pošiljanje surovih podatkov na Wi-Fi modul
    for (int i = 0; i < FFT_SIZE; i+=16) { //pošiljamo po 16 podatkov naenkrat, saj knjižnica na Wi-Fi modulu ne more sprejeti več podatkov
      sprintf (payload, "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i", surovi_podatki[i],surovi_podatki[i+1],surovi_podatki[i+2],surovi_podatki[i+3], surovi_podatki[i+4],surovi_podatki[i+5],surovi_podatki[i+6],surovi_podatki[i+7],surovi_podatki[i+8],surovi_podatki[i+9],surovi_podatki[i+10],surovi_podatki[i+11], surovi_podatki[i+12],surovi_podatki[i+13],surovi_podatki[i+14],surovi_podatki[i+15]);
      Serial1.println(payload);

      //Čakanje na signal, da je Wi-Fi modul sprejel poslane podatke
      while(!Serial1.available()) {}
      //Čiščenje zalogovnika
      while (Serial1.read() >= 0) {} 
      delay(1);
    }
    
    // Ob pritisku gumba se snemanje zaključi
    if (digitalRead(buttonSTART)==HIGH) {
      // Zaprtje dokumenta v katerega se zapisujejo meritve
      file.close();
      Serial1.println("Konec");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("To start again");
      lcd.setCursor(0, 1);
      lcd.print("Press RESET");
      while(1) {}
    }
    
    samplingBegin();
  }
}


void samplingCallback() {
  // Branje vrednosti iz pospeškomera
  surovi_podatki[sampleCounter] = analogRead(ANALOG_1);
  sampleCounter ++;
  if (sampleCounter >= FFT_SIZE) {
    samplingTimer.end();
  }
}

void samplingBegin() {
  // Postavitev števca na 0
  sampleCounter = 0;
  cas = micros();
  samplingTimer.begin(samplingCallback, 1000000/Fs);  // funkcija, ki kliče funkcijo v točno določenem časovnem imtervalu - samplingTimer.begin(function, microseconds); - vir https://www.pjrc.com/teensy/td_timing_IntervalTimer.html 
}

boolean samplingIsDone() {
  return sampleCounter >= FFT_SIZE;
}

void writeHeader() {
  file.write("| Recording started: ");
  file.print(micros());
  file.write(" | Sample rate: ");
  file.print(Fs);
  file.write(" Hz | FFT size: ");
  file.print(FFT_SIZE);
  file.write(" |");
  file.println();
  file.print("Čas,");
  file.println("meritve");
}
void SDwrite(){
  file.print(cas);
  file.write(",");
  //zapis surovih podatkov
  for (int i = 0; i < FFT_SIZE; i++) {
    file.print(surovi_podatki[i]);
    file.write(",");
  }
  file.println(surovi_podatki[FFT_SIZE]);
}

