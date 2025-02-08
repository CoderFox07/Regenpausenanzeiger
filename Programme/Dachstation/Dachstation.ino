/*
Regenpausenanzeiger - Dachstation.ino
Copyright (C) 2025  Aurelio Dobmeier

Dieses Programm ist freie Software: Sie können es unter den Bedingungen
der GNU Affero General Public License, wie von der Free Software Foundation,
Version 3 der Lizenz oder (nach Ihrer Wahl) jeder späteren
veröffentlichten Version, weitergeben und/oder modifizieren.

Dieses Programm wird in der Hoffnung verteilt, dass es nützlich sein wird,
aber OHNE JEDE GEWÄHRLEISTUNG; auch ohne die implizite Gewährleistung der
MARKTGÄNGIGKEIT oder der EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
Weitere Einzelheiten finden Sie in der GNU Affero General Public License.

Sie sollten eine Kopie der GNU Affero General Public License zusammen mit
diesem Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.

----

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


//==================== Bibliotheken ====================
// Die Bibliotheken müssen eventuell nachinstalliert werden!
//---------- LoRa ----------
#include <LoRa.h>
#include <SPI.h>

//---------- Temperatur- und Luftfeuchtigkeitssensor (DHT22) ----------
#include "DHT.h"
//++++++++++++++++++++ Bibliotheken ++++++++++++++++++++


//==================== LoRa ====================
#define SS 5
#define RST 14
#define DIO0 2

// Einstellen der genauen Freuqenz. Ist abhängig von der Region und muss den entsprechenden Vorgaben eingestellt werden!!!
#define BAND 869.525E6  // Bsp. Frequenz in Hz für 869,525 MHz

String LoRaData;                     //In dieser Variable werden empfangene Daten gespeichert
bool sendeTempOderRegenstaerke = 0;  //Temperaturmesseung = 0; Regenmessung = 1; Variable wird beim Einschalten des Geräts auf Temp (0) gesetzt.
                                     //Beim Empfang von Regenmessung wird sie auf 1 gesetzt.

// Genauere Erklärungen zu diesen Variablen befinden sich im Programm der Sekretariatsstation!
// DIE EINSTELLUNGEN DER VARIABLEN MUSS MIT DENEN DER SEKRETARIATSSTATION ÜBEREINSTIMMEN!
struct LoRaData {
  const String praefix = "#RPA";
  const String typTemp = "temp";
  const String typRain = "rain";
  const String seperator1 = "B";
  const String seperator2 = "T";
  const String seperator3 = "H";
  const String seperator4 = "W";
  const String typSleep = "sleep";
  const String seperatorSleep = "S";
  const String seperatorDauerRmessung = "D";

  String empfangenesPaket = "";
} loRaData;
//++++++++++++++++++++ LoRa ++++++++++++++++++++


//==================== Temperatur- und Luftfeuchtigkeitssensor (DHT22) ====================
#define DHTPIN 27          //Pin 27 am ESP32
#define DHTTYPE DHT22      //Type: DHT 22 (Es gibt verschiedene Varianten des Sensors)
DHT dht(DHTPIN, DHTTYPE);  // Initialize DHT sensor.

float temperatur;    //Variable, um Temperatur zu speichern. (float kann Kommazahlen enthalten)
float feuchtigkeit;  //Variable, um Luftfeuchtigkeit zu speichern.
//++++++++++++++++++++ Temperatur- und Luftfeuchtigkeitssensor (DHT22) ++++++++++++++++++++


//==================== Regensensor ====================
#define Regensensor 34
unsigned long tropfenzaehler = 0;
int schwelle = 2;  // Schwelle für den Regensensor: Legt fest, ab wann der Messwert am Pin als Regentropfen erkannt wird.
//++++++++++++++++++++ Regensensor ++++++++++++++++++++


//==================== Sonstiges ====================
#define StromPin 25  // Pin, der den Transistor ansteuert, mit dem der DHT 22, das LoRa-Modul und der Regensensor, sowie Spannunsgsteiler für Spannungsmessung des Akkus mit Strom versorgt werden.

// Pin, an dem man einen Buzzer anschließen kann, der immer kurz piept, wenn Tropfen gezählt (für Test- oder Demonstrationszwecke)
#define Buzzer 13  // Wird auf HIGH gesetzt, wenn ein Tropfen erkannt wurde

// An Pin 26 kann ein Spannungsteiler angeschlossen werden, um die Spannung des Akkus zu ermitteln.
// Diese Methode ist allerdings nicht sehr genau und dient nur der groben Überwachung.
#define BatSpannungsPin 26
const float r1 = 100000.0;    // Wert von R1 in Ohm (100 kΩ)
const float r2 = 300000.0;    // Wert von R2 in Ohm (300 kΩ)
const float vRef = 3.3;       // Referenzspannung des ADC
const float ausgleich = 0.0;  // Ausgleichswert
float battSpannung = 0;
const byte anzahlSpannungsMessungen = 20;  // Um Genauigkeit zu erhöhen werden mehrere Messungen hintereinander durchgeführt

// Schaltung Spannungsteiler:
//        Akku (+) ----- R1 -----+----- ADC Pin (BatSpannungsPin: 26)
//                               |
//                               R2
//                               |
//  GND (Emitter Transistor) ----+
//++++++++++++++++++++ Sonstiges ++++++++++++++++++++


//==================== Variablen, die dafür sorgen, dass nicht empfange Pakete nochmals gesendet werden ====================
int anzahlFehlSendeversuche = 0;                      // Nach einer bestimmten Anzahl an gesendeten Paketen, die nicht von der Station im Sekretariat empfangen wurden schaltet sich die Station in Sleep-Modus für 10 min.
const int anzahlSendeversuche = 3;                    // Legt fest, wie oft die Daten an das Sekretariat gesendet werden, wenn es nicht antwortet und nur Temp und Hum gesendet werden.
const int zusaetzlicheSendeversucheRegenmessung = 3;  // Wird zu AnzahlSendeversuche addiert, wenn das Ergebis einer Regenmessung gesendet wird, sodass es auf jeden Fall ankommt.
const int intervallSendeversuche = 5000;              // Intervall der erneuten Sendeversuche (je länger, desto mehr Stromverbrauch, da der ESP32 währenddessen nicht im DeepSleep ist)
const int dauerSleepNachErfolglosemSenden = 600000;   // Schlafenszeit nach erfolglosem Senden (-> wenn die Sekretariatsstation nicht auf das Paket geantwortet hat)
long timer = 0;                                       // Timer, dass Daten immer wieder im Intervall (von intervallSendeversuche) gesendet werden, wenn keine Antwort von Sekretariatsstation erfolgte
//++++++++++++++++++++ Variablen, die dafür sorgen, dass nicht empfange Pakete nochmals gesendet werden ++++++++++++++++++++


// Funktion, die aufgerufen wird, um den DeepSleep-Modus zu starten:
void deepSleep(int sleepTime) {
  Serial.println("Vorbereitung für Deep Sleep...");
  digitalWrite(StromPin, LOW);  // Um Energie zu sparen wird der Strom der Sensoren und Aktoren am Transistor ausgeschaltet

  // Vorbereiten der Dauer des Deep-Sleeps in Mikrosekunden
  esp_sleep_enable_timer_wakeup(sleepTime * 1000);  //Die sleepTime wird vom Server in Millisekunden geschickt und muss in Mikrosekunden umgerechnet werden.

  Serial.println("ESP32 geht in den Deep Sleep-Modus...");

  Serial.flush();          //Warten, bis alles an den Computer gesendet wurde
  esp_deep_sleep_start();  //Starten des Tiefschlafs
}


void setup() {
  Serial.begin(115200);  //Starten der Seriellen Kommunikation mit dem Computer
  Serial.println("Dachstation");
  timer -= intervallSendeversuche;  //Timer muss negativ sein und betraglich mit intervallSendeversuche übereinstimmen, dass die Daten beim ersten Senden sofort gesendet werden.
  pinMode(StromPin, OUTPUT);
  pinMode(BatSpannungsPin, INPUT);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  pinMode(Regensensor, INPUT);   //Pin für den Regensensor
  digitalWrite(StromPin, HIGH);  //Der Transistor wird aktiviert --> alle angeschlossenen können hochfahren
  delay(2000);                   //Wartem, bis der DHT22-Sensor gestartet werden kann
  dht.begin();                   //Starten des Sensors

  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);

  //Starten des Lora-Moduls:
  LoRa.setPins(SS, RST, DIO0);  //Zuweisung der Pins
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");  //Fehlermeldung am Computer ausgeben, wenn es nicht funktioniert hat
    while (1)
      ;
  }

  // Mit diesen Befehlen kann man die LoRa-Übertragungsparameter genauer einstellen (D: Default/ Standart; Min: kleinster einstellbarer Wert; Max: größter einstellbarer Wert)
  // Die Parameter müssen den Vorschriften der jeweiligen Region entsprechen!!!
  /*
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);  // D: 17; Min: 2; Max: 20; Sendeleistung
  LoRa.setSignalBandwidth(125E3);               // D: 125E3; `7.8E3`, `10.4E3`, `15.6E3`, `20.8E3`, `31.25E3`, `41.7E3`, `62.5E3`, `125E3`, `250E3`, and `500E3` Bandbreite
  LoRa.setPreambleLength(8);                    // D: 8; Min: 6; Max: 65535; Preamble-Länge festlegen
  LoRa.setSyncWord(0x12);                       // D: 0x12; Synchronisationswort
  LoRa.enableCrc();                             // D: disableCRC(); CRC
  LoRa.setCodingRate4(5);                       // D: 5; Min: 5; Max: 8; Coding Rate
  LoRa.setSpreadingFactor(7);                   // D: 7; Min: 6; Max: 12; Spreading Factor
  */
  Serial.println("Start...");
}


void loop() {
  if ((timer + intervallSendeversuche) < millis()) {
    Serial.println("Sende Daten via LoRa");
    anzahlFehlSendeversuche++;           //Wird jedes Mal erhöht, sodass ab bestimmter Anzahl automatisch Sleep-Modus aktiviert wird.
    temperatur = dht.readTemperature();  //Auslesen der Temperatur
    feuchtigkeit = dht.readHumidity();   //Auslesen der Luftfeuchtigkeit
    unsigned long adcWert = 0;
    for (int i = 0; i < anzahlSpannungsMessungen; i++) {
      adcWert += analogRead(BatSpannungsPin);
    }
    battSpannung = (((adcWert / anzahlSpannungsMessungen) / 4095.0 * vRef) * (r1 + r2)) / r2;
    battSpannung += ausgleich;
    Serial.println("Batterie-Spannung: " + String(battSpannung));  // Wenn ein USB-Kabel am ESP32 angeschlossen ist, funktioniert die Spannungsmessung möglicherweise nicht richtig!
    if (sendeTempOderRegenstaerke == 0) {
      // Sende Temperatur und Luftfeuchtigkeit
      LoRa.beginPacket();
      LoRa.print(loRaData.praefix + loRaData.typTemp + loRaData.seperator1 + battSpannung + loRaData.seperator2 + String(temperatur) + loRaData.seperator3 + String(feuchtigkeit) + loRaData.seperator4);
      LoRa.endPacket();
    } else {
      // Sende Temperatur und Luftfeuchtigkeit, sowie Regentropfen
      LoRa.beginPacket();
      LoRa.print(loRaData.praefix + loRaData.typRain + loRaData.seperator1 + battSpannung + loRaData.seperator2 + String(temperatur) + loRaData.seperator3 + String(feuchtigkeit) + loRaData.seperator4 + String(tropfenzaehler));
      LoRa.endPacket();
    }
    timer = millis();
  }

  if (anzahlFehlSendeversuche >= anzahlSendeversuche && sendeTempOderRegenstaerke == 0) {
    Serial.println("Sleep-Modus, weil Sekretariat nicht geantwortet hat. (1)");
    deepSleep(dauerSleepNachErfolglosemSenden);  //Schlafe dauerSleepNachErfolglosemSenden Millisekunden, wenn Sekretariat nicht geantwortet hat.
  } else if (anzahlFehlSendeversuche >= (anzahlSendeversuche + zusaetzlicheSendeversucheRegenmessung) && sendeTempOderRegenstaerke == 1) {
    Serial.println("Sleep-Modus, weil Sekretariat nicht geantwortet hat. (2)");
    deepSleep(dauerSleepNachErfolglosemSenden);  //Schlafe dauerSleepNachErfolglosemSenden Millisekunden, wenn Sekretariat nicht geantwortet hat.
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet: ");

    //read packet
    while (LoRa.available()) {
      loRaData.empfangenesPaket = LoRa.readString();
      Serial.print(LoRaData);
    }

    // Signalqualität ausgeben
    int rssi = LoRa.packetRssi();
    Serial.print("    Empfangsqualität:    RSSI: ");
    Serial.print(LoRa.packetRssi());
    Serial.print(" dBm    |    SNR: ");
    Serial.print(LoRa.packetSnr());
    Serial.println(" dB");

    // Überprüfen, ob das Paket von der Sekretariatsstation stammt
    if (loRaData.empfangenesPaket.startsWith(loRaData.praefix)) {
      Serial.println("LoRa-Data starts with " + loRaData.praefix);

      // Als nächstes wird überprüft, ob die Dachstation dazu aufgefordert wurde in den Sleep-Modus zu wechseln, oder eine Regenmessung durchzuführen.
      if (loRaData.empfangenesPaket.startsWith(loRaData.praefix + loRaData.typSleep + loRaData.seperatorSleep)) {
        long schlafenszeit = loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperatorSleep) + 1).toInt();
        Serial.print("Ich schlafe jetzt: ");
        Serial.println(schlafenszeit);

        // Aufruf der Funktion zum Starten des Deep-Sleep-Modus:
        deepSleep(schlafenszeit);
      } else if (loRaData.empfangenesPaket.startsWith(loRaData.praefix + loRaData.typRain + loRaData.seperatorDauerRmessung)) {
        // LoRa-Paket der Sekretariatsstation enthält Auffrderung zur Regenmessung
        sendeTempOderRegenstaerke = 1;
        anzahlFehlSendeversuche = 0;

        // Dauer der Regenmessung wird aus dem LoRa-Paket extrahiert und ausgegeben:
        unsigned long dauerRegenmessung = loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperatorDauerRmessung) + 1).toInt();
        Serial.println(String("Dauer der Regenmessung: " + String(dauerRegenmessung) + String("Dauer in Sek.: ") + String(dauerRegenmessung / 1000)));

        digitalWrite(Buzzer, HIGH);  // Vor der Regenmessung kurz piepen
        delay(200);
        digitalWrite(Buzzer, LOW);
        Serial.println("========== REGENMESSUNG ==========");
        tropfenzaehler = 0;
        unsigned long messTimer = millis();

        while ((messTimer + dauerRegenmessung) > millis()) {  // Schleife für Regenmessung
          int messwert = analogRead(Regensensor);             // Regensensor auslesen und gemessenen Wert in die Variable speichern.
          if (messwert > schwelle) {                          // Wenn der messwert größer als die Schwelle ist, dann wurde ein Regentropfen erkannt
            tropfenzaehler++;                                 // Der Tropfenzähler wird um 1 erhöht.
            Serial.println("Messwert Regentronpfen: " + String(messwert) + "    |    Anzahl Tropfen: " + String(tropfenzaehler));

            // Kurz warten, sodass ein Tropfen nicht mehrmals auslöst und dabei piepsen (30ms von 70ms soll der Buzzerpin auf HIGH sein):
            // HINWEIS: Die genaue Dauer muss evtl. angepasst werden
            digitalWrite(Buzzer, HIGH);
            delay(30);
            digitalWrite(Buzzer, LOW);
            delay(40);
          }
        }

        Serial.println("========== REGENMESSUNG FERTIG! ==========");

        digitalWrite(Buzzer, HIGH);  // Nach der Regenmessung kurz piepen
        delay(200);
        digitalWrite(Buzzer, LOW);
        timer = millis() - intervallSendeversuche;  // Timer wird so eingestellt, dass Daten immer sofort gesendet werden
      }
    } else {
      // Wenn das Paket nicht von der Sekretariatsstation stammt
      Serial.println("Anderes Paket empfangen.");
    }
  }
}