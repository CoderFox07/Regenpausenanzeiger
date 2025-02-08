/*
Regenpausenanzeiger - Sekretariatsstation.ino
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


// ==================== Variablen zur Konfiguration des Regenpausenanzeigers ====================
// ---------- WLAN-Zugangsdaten ----------
const char *ssid = "WLAN-Name";
const char *password = "WLAN-Passwort";

// ---------- Schwellen für Regen- und Kältepause ----------
const long regenschwelle = 20;  // Schwelle, ab wann eine Pause als Regenpause in der eingestellten Messzeit (-> dauerRegenmessung) gilt --> muss kalibriert werden (Bsp: 20 Tropfen in 10 min).
const float schwelleKaeltepause = 0.0;

// ---------- Intervall Wetterdaten, Dauer Regenmessung ----------
const int dauerSchlafenszeitDachstation = 10;                                // Gibt in min. an, in welchem Intervall die Dachstation aufwachen soll und die Wetterdaten schicken soll
const int dauerRegenmessung = 10;                                            // Wie lange die Regenstärke in Minuten gemessen werden soll
const int schwelleWarnmeldungDatenZuletztEmpfangenVonDach = 600000 + 60000;  // Schwelle, ab wann eine Warnung ausgegeben werden soll: nach dauerSchlafenszeitDachstation aber noch eine Minute Puffer: 10min + 1min

// ---------- Zeiten der Pausen und Messbeginn Regenmessung ----------
const byte anzahlPausen = 1;  // Für wie viele Pausen eine Regenmessung durchgeführt werden soll
                              // Wenn keine Regenmessungen durchgeführt werden sollen und damit auch keine Regenpausen, etc. angezeigt weden soll,
                              // dann 0 eintragen und die die Arrays für die Uhrzeiten der Pausen leer lassen

// In den folgenden beiden Arrays müssen die Pausenzeiten eingetragen werden.
// Im ersten müssen die Stunden eingetragen werden.
// Im zweiten die Minuten der Pause
// Die Stunden und Minuten müssen von links nach rechts eingetragen werden.
// Die jeweils untereinander stehenden Werte gehören zu einer Uhrzeit (-> Stunde und Minute)
// Bei mehreren Pausen müssen die einzelnen Werte durch Kommas "," getrennt werden.
const int pausenStunden[anzahlPausen] = { 11 };
const int pausenMinuten[anzahlPausen] = { 0 };

// Im fogenden Array kann eingetragen werden, an welchen Tagen eine Regenmessung gestartet wird.
// Von links nach rechts sind die Wochentage. Es beginnt mit Sonntag und endet mit Samstag.
// 0 steht für keine Regenmessungen am jeweiligen Tag; 1 steht für Regenmessungen am jeweiligen Tag
// Beispiel für "nur Regenmessungen unter der Woche und keine am Wochenende": { 0, 1, 1, 1, 1, 1, 0 }
const int messTage[7] = { 0, 1, 1, 1, 1, 1, 0 };

const int messbeginnVorPause = dauerRegenmessung + 3;  // Der Messbeginn der Regenmessung vor der Pause sein: So lange, wie die Messung dauert + z.B. 3 min. So steht das Ergebnis kurz vor der Pause fest.
int pufferBeginnRegenmessung = 3;                      // Pufferzeit in Minuten, in der die Regenmessung trotzdem nach eigentlichem Messbeginn noch gestartet wird (z.B. 3 min)

const long dauerLEDsAnNachRegenmessung = 1200000;    // Die LEDs sollen 20 min = 1200000 ms nach der eingehenden Regenmessung (also während der Pause) leuchten, bis sie aus gehen sollen.
                                                     // Die Anzeigestation hängt auch davon ab: Es wird dort auch solange Regenpause/Schoenwetterpause/... angezeigt.
const int dauerRegenpausenAenderungsTimer = 780000;  // Falls es einen Messfehler gab, kann man nach der Festlegung der Pause, innerhalb von der Zeit des Timers die Pause manuel zu
                                                     // Regen-/Schoenwetterpause ändern (Bsp: 13 min = 780000). Danach ist es nicht mehr möglich eine Änderung durchzuführen
const int drueckDauerPausenAendrerung = 3000;        // Drückdauer des Buttons, um den Pausenstatus zu ändern

// ---------- Anzeigedauer (Millisekunden) der einzelnen Schritte beim Starten ----------
const int anzeigedauerStartdetails = 250;

// ---------- Name des Servers und Pfad, wo die Wetterdaten durch das PHP-Skript gespeichert werden ----------
const String serverName = "http://regenpausenanzeiger.local/rcvwetterdaten.php?";  // Entweder IP-Adresse oder Hostname eigeben (IP nur wenn sie statisch ist!)

// ---------- Im PHP-Skript eingestellter Bestätigungstext, wenn die empfangenen Wetterdaten erfolgreich in SQL-Datenbank gespeichert wurden ----------
const String bestaetigungAnzeigestation = "Daten erfolgreich gespeichert.";  // Wird als Bestätigung benötigt, dass die Daten nochmals gesendet werden, wenn es einen Fehler gab.
// ++++++++++++++++++++ Variablen zur Konfiguration des Regenpausenanzeigers ++++++++++++++++++++


//==================== Bibliotheken ====================
// Die Bibliotheken müssen eventuell nachinstalliert werden!
//---------- LoRa ----------
#include <LoRa.h>
#include <SPI.h>

//---------- Wlan ----------
#include <WiFi.h>

//---------- Verbindung mit Vertretungsplanstation, Website ----------
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncHTTPClient.h>

//---------- OTA ----------
// Mittels OTA lassen sich Updates über die ferne durchführen.
// In diesem Fall über das lokale Netz. Dazu kann man in der Arduino IDE bei Ports den Netzwerkport von der Sekretariatsstation auswählen und den Code hochladen
// Das Passwort kann im setup() als MD5-Hash konfiguriert werden. So kann nicht jeder einfach Code hochladen.
#include <ArduinoOTA.h>

//---------- Zeit ----------
#include "time.h"

//---------- Display ----------
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//++++++++++++++++++++ Bibliotheken ++++++++++++++++++++


//==================== LoRa; Verbindung mit Dachstation ====================
// Pins für das LoRa-Modul (RFM95)
#define SS 5
#define RST 14
#define DIO0 2
// Einstellen der genauen Freuqenz. Ist abhängig von der Region und muss den entsprechenden Vorgaben eingestellt werden!!!
#define BAND 869.525E6  // Bsp. Frequenz in Hz für 869,525 MHz

bool Dachstation_STATUS = 0;                     //rote LED soll leuchten (0 = alles gut, 1 = Problem), wenn kein WLAN verbunden ist
unsigned long datenZuletztEmpfangenVonDach = 0;  //Timer, der speichert, wann zuletzt Daten von der Dachstation empfangen wurden
int dauerZuletztGesendeteSchlafenszeit = 0;

// Um die Wetterdaten über LoRa auszutauschen ist ein bestimmtes Format notwendig: Präfix Typ seperator1 Spannung seperator2 Temperatur seperator3 Luftfeucht. seperator4 (Regentropfen)
// Je nach dem, ob Regedaten übermittelt werden oder nicht, wird entweder "typTemp" oder "typRain" nach dem Präfix verwendet.
// Dass die Ziffern von Temp. und Luftfeucht. nicht direkt nebeneinander stehen, wird ein Seperator benötigt.
// Beispiel für eine LoRa-Nachricht: #RPAtempB3.53T12.30H78.90W oder #RPArainB3.2T13.20H79.80W56 (-> 13.2°C; 79.8% Luftfeucht.; 56 Topfen)
// WICHTIG: Es dürfen in der gesamten LoRa-Nachricht keine Buchstaben doppelt vorkommen, die als Seperator benutzt werden!!!
struct LoRaData {
  const String praefix = "#RPA";
  const String typTemp = "temp";
  const String typRain = "rain";
  const String seperator1 = "B";  // Akku/ Battery
  const String seperator2 = "T";  // Temperatur
  const String seperator3 = "H";  // Luftfeuchtigkeit / Humidity
  const String seperator4 = "W";  // Regentropfen / Wasser
  // Zur Steuerung der Dachstation:
  const String typSleep = "sleep";            // wird benutzt, um die Schlafenszeit/ DeepSleep-Zeit für die Dachstation zu übermitteln.
  const String seperatorSleep = "S";          // Beispiel: (loRaData.praefix + loRaData.typSleep + loRaData.seperatorSleep + dauerSchlafenszeitDachstation) -> RPAsleepS600000
  const String seperatorDauerRmessung = "D";  // außer sleep wird LoRaDatatypRain mit Dauer nach LoRaDataseperatorDauerRmessung benutzt, um eine Regenmessung anzufordern.

  // Variablen, um die Signalqualität für das empfangene Paket abzuspeichern:
  String empfangenesPaket = "";
  String uhrzeitUTCWetterEmpfagen = "";  // Wird später in UTC und nicht lokaler Zeit gespeichert!
  String datumUTCWetterEmpfagen = "";    // Wird später in UTC und nicht lokaler Zeit gespeichert!
  String rssiWetterdaten = "";
  String snrWetterdaten = "";
} loRaData;
//++++++++++++++++++++ LoRa; Verbindung mit Dachstation ++++++++++++++++++++


//==================== Wlan ====================
const char *hostname = "RPAsekretariat";  // Hostname der Sekretariatsstation

// Status: rote LED soll leuchten, wenn kein WLAN verbunden ist
bool WLAN_STATUS = 0;

const int reconnectIntervall = 3000;
unsigned long reconnectTimer = 0;  //Timer, mit dem alle paar Sekunden versucht wird, sich wieder mit dem WLAN zu verbinden, falls Verbindung fehlschlägt
//++++++++++++++++++++ Wlan ++++++++++++++++++++


//==================== Verbindung mit Anzeigestation ====================
AsyncHTTPClient http;
bool datenAnAnzeigeGesendet = true;  // Wird auf true gesetzt, wenn die neuesten Daten schon an Anzeigestation gesendet wurden (am Anfang, bis erste Daten kommen True, da nichts zum Senden)
bool httpAnfrageLaeuft = false;      // Wird während die http-Anfrage läuft auf true gesetzt

// Zu sendende Parameter in diesem Format:
// http://regenpausenanzeiger.local/rcvwetterdaten.php?datum=2024-12-22&uhrzeit=20:02:02&temperatur=2.0&luftfeuchtigkeit=92.2&pausenstatus=0&regentropfen=0
// Wichtig: Uhrzeit und Datum werden im UTC-Format gesendet!
String wetterdatenFuerAnzeige = "";  // Variable, die Wetterdaten in Form der restlichen http-Anfrage enthält

const int ablaufzeitHttpAnfrage = 10000;
unsigned long timerAblaufzeitHttpAnfrage = 0;
//++++++++++++++++++++ Verbindung mit Anzeigestation ++++++++++++++++++++


//==================== Website ====================
// Die Sekretariatsstation gibt beim aufrufen ihrer IP-Adresse oder Hostname eine Website aus, die der Wartung dient.
AsyncWebServer server(80);

// Variablen, in denen die einzelnen Teile der Website gespeichert werden. Die Website wird zur Vereinfachrung direkt im Code gespeichert. Falls der Speicher knapp wird, kann sie entfernt werden.
struct Website {
  const String Head = "<!DOCTYPE html><html><head><meta name =\"viewport\" content=\"width=device-width, initial-scale = 1\">";
  const String Style = "<style> html, body { font-family: Helvetica; margin: 0; height: 100%; display: flex; justify-content: top; align-items: center; flex-direction: column; } .container { text-align: left; padding: 10px; border: 0px solid #0f0f0f; border-radius: 20px; background-color:";
  String TempHum;
  String Pausenstatus;
  String DatenVon;
  String AnzahlTropfen;
  const String Ende = "</div></body></html>";
  String pausenStatus;

  // Hintergrundfarbe
  const String regenpausenFarbe = "#8888ff";
  const String keineRegenpausenFarbe = "#ffff88";
} website;
//++++++++++++++++++++ Website ++++++++++++++++++++


//==================== Zeit ====================
const String zeitzone = "CET-1CEST,M3.5.0/1,M10.5.0/3";  // Zeit für Berlin
// Variablen, in denen die Zeit abgespeichert wird:
struct Zeit {
  String Datum = "";
  String Uhrzeit = "";
  String DatumUTC = "";
  String UhrzeitUTC = "";
  int stunde, minute, sekunde, tag, monat, jahr, wochentag;
} zeit;
//++++++++++++++++++++ Zeit ++++++++++++++++++++


//==================== LC-Display ====================
LiquidCrystal_I2C LCD(0x27, 16, 2);
//++++++++++++++++++++ LC-Display ++++++++++++++++++++


//==================== Status-LEDs ====================
// Pins der Status-LEDs:
#define LED_Status_Gruen 32
#define LED_Status_Rot 33
#define LED_Regenpause 25
#define LED_KeineRegenpause 26

unsigned long timerPausenLEDsAus = 0;  // Timer (wird auf millis() gesetzt), der die LEDs (für Pausenstatus) nach oben konfigurierter Dauer wieder ausschalten lässt.
bool statusWetterLEDs = 0;             // 0 = aus; 1 = an; wird während der Laufzeit angepasst
//++++++++++++++++++++ Status-LEDs ++++++++++++++++++++

//==================== Button ====================
//(roter) Knopf/ Taster (seitlich an der Sekretariatsstation), um eine Pause manuell festzulegen, falls nötig (z.B. bei Messfehler)
#define ButtonPin 27

// Variablen für den Button
struct Button {
  bool status = 0;          // In diese Variable wird der ausgelesene Button-Status (gedrückt, oder nicht) gespeichert.
  bool zuvorGedrueckt = 0;  // Um die Zeit, die der Button gedrückt ist zu ermitteln, benötigt man eine Variable, in der der voherige Status des Buttons gespeichert wird.
  unsigned long timer = 0;  // Timer, um die Drück-Dauer des Buttons zu ermitteln (wird auf millis() gesetzt)
} button;

unsigned long regenpausenAenderungsTimer = 0;
//++++++++++++++++++++ Button ++++++++++++++++++++


//==================== Sonstige Variablen ====================
struct Wetterdaten {
  int regenstaerke = 0;                // Variable, in der die Anzahl der von der Dachstation gemessenen Regentropfen gezählt werden.
  float temperaturFloat;               // Temperatur der Dachstation als Float
  float luftfeuchtigkeitFloat;         // Luftfeuchtigkeit der Dachstation als Float
  String temperaturString = "";        // Temperatur der Dachstation als String
  String luftfeuchtigkeitString = "";  // Luftfeuchtigkeit der Dachstation als String
} wetterdaten;

byte regenpause = 0;  // 0 = restliche Zeit -> wenn keine Pause ist; 1 = Regenpause; 2 = Kaeltepause; 3 = keine Regenpause -> Schoenwetterpause

String spannungDachstation = "";  // Akku-Spannung der Dachstation
// Arrays, in denen der Messbeginn der einzelnen Pausen abgespeichert wird (wird im setup() anhand der pausen-Arrays berechnet)
int messStartStunden[anzahlPausen];
int messStartMinuten[anzahlPausen];
//++++++++++++++++++++ Sonstige Variablen ++++++++++++++++++++


void setup() {
  Serial.begin(115200);
  Serial.println("Sekretariatsstation\n");

  // Die Pins LEDs werden als Output festgelegt:
  pinMode(LED_Status_Gruen, OUTPUT);
  pinMode(LED_Status_Rot, OUTPUT);
  pinMode(LED_Regenpause, OUTPUT);
  pinMode(LED_KeineRegenpause, OUTPUT);

  // Alle LEDs sollen kurz aufleuchten:
  digitalWrite(LED_Status_Gruen, HIGH);
  digitalWrite(LED_Status_Rot, HIGH);
  digitalWrite(LED_Regenpause, HIGH);
  digitalWrite(LED_KeineRegenpause, HIGH);
  delay(1000);
  digitalWrite(LED_Status_Gruen, LOW);
  digitalWrite(LED_Status_Rot, LOW);
  digitalWrite(LED_Regenpause, LOW);
  digitalWrite(LED_KeineRegenpause, LOW);

  pinMode(ButtonPin, INPUT);  // Button als Input

  timerPausenLEDsAus -= dauerLEDsAnNachRegenmessung;

  // LC-Display initialisieren
  LCD.init();
  LCD.backlight();

  LCD_PrintOben("Berechne Pausen");  // Mithilfe der im Code angelegten Funktion Text in der oberen Zeile ausgeben


  // Berechnung des Messbeginns der Regenmessung der verschiedenen Pausen:
  Serial.println("Berechne den Messbeginn der jeweiligen Pause.");
  for (int i = 0; i < anzahlPausen; i++) {  // Für alle Pausen einmal berechnen (wenn keine Pause festgelegt wurde, wird nichts berechnet)
    messStartMinuten[i] = pausenMinuten[i] - messbeginnVorPause;
    messStartStunden[i] = pausenStunden[i];

    // Falls die Minuten negativ werden, passe die Stunden und Minuten an:
    if (messStartMinuten[i] < 0) {
      messStartMinuten[i] += 60;
      messStartStunden[i]--;

      if (messStartStunden[i] < 0) {  // Falls der Messbeginn am Tag vorher ist,
        messStartStunden[i] += 24;    // dann wird die negative Zahl auf die richtige Uhrzeit gestellt.
      }
    }

    // Den Messbeginn der jeweiligen Pause ausgeben:
    Serial.println("Pause Nr." + String(i) + " um " + String(pausenStunden[i]) + ":" + String(pausenMinuten[i])
                   + " hat Messbeginn der Regenmessung um: " + String(messStartStunden[i]) + ":" + String(messStartMinuten[i]));
  }


  delay(anzeigedauerStartdetails);

  LCD_PrintOben("Starte WLAN");
  delay(anzeigedauerStartdetails);

  // Starte WLAN:
  Serial.println("\n" + String(ssid));
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  bool LED_Status = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_Status_Gruen, LED_Status);
    digitalWrite(LED_Status_Rot, !LED_Status);
    LED_Status = !LED_Status;
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED_Status_Gruen, LOW);
  digitalWrite(LED_Status_Rot, LOW);
  Serial.println("");

  LCD_PrintOben("Wlan verbunden");
  delay(anzeigedauerStartdetails);

  LCD_PrintOben("Starte OTA");
  delay(anzeigedauerStartdetails);

  // Konfiguration von OTA (für Aktualisierungen über WLAN):
  ArduinoOTA.setHostname(hostname);

  // Für mehr Sicherheit kann ein Passowort für OTA festgelegt werden, dass als MD5-Hash eingetragen werden muss.
  // Beispiel für Passwort "Regenpausenanzeiger":
  // ArduinoOTA.setPasswordHash("d09a98551201e01f47572a293a1db6fd");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";

      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Synchronisiere Zeit...");
  LCD_PrintOben("Synchronisiere");
  LCD_PrintUnten("Zeit...");
  digitalWrite(LED_Status_Gruen, HIGH);
  digitalWrite(LED_Status_Rot, HIGH);
  delay(anzeigedauerStartdetails);

  // Hole Zeit von NTP und stelle Zeitzone ein:
  configTime(0, 0, "pool.ntp.org");

  Serial.printf("  Setting Timezone to %s\n", zeitzone.c_str());
  setenv("TZ", zeitzone.c_str(), 1);  //  Now adjust the timezone.  Clock settings are adjusted to show the new local time
  tzset();

  // Bevor die Zeit kurz im Display angezeigt wird, müssen die Zeitvariablen aktualisiert werden.
  updateLocalTimeVariables();

  LCD_PrintOben(zeit.Uhrzeit);
  LCD_PrintUnten(zeit.Datum);
  delay(4000);


  LCD_PrintOben("Starte LoRa...");
  LCD_PrintUnten("");

  delay(anzeigedauerStartdetails);

  LoRa.setPins(SS, RST, DIO0);  //Zuweisung der Pins

  // Starten des Lora-Moduls:
  if (!LoRa.begin(BAND)) {
    digitalWrite(LED_Status_Gruen, LOW);

    LCD_PrintOben("LoRa fehl-");
    LCD_PrintUnten("geschlagen!!!");
    delay(3000);
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

  LCD_PrintOben("LoRa gestartet");
  delay(anzeigedauerStartdetails);

  LCD_PrintOben("Starte mDNS");
  delay(anzeigedauerStartdetails);

  // DNS-Namensauflösung für den Hostnamen aktivieren
  if (!MDNS.begin(hostname)) {
    Serial.println("Fehler bei der DNS-Namensauflösung!!!");
    return;
  }
  Serial.println("DNS-Namensauflösung aktiviert.");

  LCD_PrintOben("mDNS gestartet");
  delay(anzeigedauerStartdetails);

  LCD_PrintOben("Starte Webserver");
  delay(anzeigedauerStartdetails);

  // Hier werden die einzelnen Pfade für die Websiten angelegt. Falls man einen Teil davon deaktivieren möchte: einfach auskommentieren
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStartseite(request);
  });
  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleAdmin(request);
  });
  server.on("/sendPacketViaLoRa", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleSendPacketViaLoRa(request);
  });
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleInfo(request);
  });
  server.begin();

  LCD_PrintOben("Bereit");
  LCD_PrintUnten("Warte auf Daten");

  Serial.println("\nBereit. Warte auf Wetterdaten!");

  digitalWrite(LED_Status_Rot, LOW);
}


void loop() {
  ArduinoOTA.handle();
  // Überprüfe, ob WLAN verbunden ist und verbinde erneut, falls nötig
  if ((WiFi.status() != WL_CONNECTED) && (millis() > (reconnectIntervall + reconnectTimer))) {
    Serial.println("WLAN nicht mehr verbunden!");
    WLAN_STATUS = 1;
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    reconnectTimer = millis();
  } else if (WiFi.status() == WL_CONNECTED) {
    WLAN_STATUS = 0;
  }


  button.status = digitalRead(ButtonPin);
  // Um den Pausenmodus zu ändern muss man den Button solange, wie oben konfiguriert (-> drueckDauerPausenAendrerung) drücken. Erst dann soll der Pausenmodus geändert weden.
  // Um den Code nicht zu blockieren, sodass evtl. keine Nachrichten von der Dachstation empfangen werden, werden die Zeitpunkte des Drückens gespeichert und überprüft, ob der Button X ms gedrückt wurde
  if (button.status == 1 && (regenpausenAenderungsTimer + dauerRegenpausenAenderungsTimer) > millis() && button.zuvorGedrueckt == 0 && regenpause != 0) {
    button.timer = millis();
    button.zuvorGedrueckt = 1;
  } else if (button.timer + drueckDauerPausenAendrerung < millis() && button.zuvorGedrueckt == 1) {
    button.zuvorGedrueckt = 0;
    button.status = 0;

    Serial.println("---------- BUTTON MIN. 3 SEK GEDRÜCKT! ----------");

    if (regenpause != 3) {  // Nur wenn gerade eine Pause ist, soll der Modus geändert werden können
      regenpause = false;
      Serial.println("Pause zu Schoenwetterpause geändert!");
      digitalWrite(LED_Regenpause, LOW);
      digitalWrite(LED_KeineRegenpause, HIGH);

      LCD_PrintOben("Keine Regenpause");
      regenpause = 3;  // 3 steht für Schoenwetterpause
    } else {
      if (wetterdaten.temperaturFloat <= schwelleKaeltepause) {
        Serial.println("Pause zu Kaeltepause geändert!");
        digitalWrite(LED_Regenpause, HIGH);
        digitalWrite(LED_KeineRegenpause, LOW);

        LCD_PrintOben("Kaeltepause!");
        regenpause = 2;  // 2 steht für Kältepause
      } else {
        Serial.println("Pause zu Regenpause geändert!");
        digitalWrite(LED_Regenpause, HIGH);
        digitalWrite(LED_KeineRegenpause, LOW);

        LCD_PrintOben("Regenpause!");
        regenpause = 1;  // 1 steht für Regenpause
      }
    }
    LCD_PrintUnten("Temp: " + wetterdaten.temperaturString + " Grad");

    // Damit die Anzeigestation den geänderten Pausenmodus anzeigt, müssen die Daten erneut gesendet werden.
    // Die Uhrzeit muss aber aktuell sein, dass die Anzeigstation sie richtig anzeigt. Dafür wird so getan, als ob die LoRa-Daten zu diesem Zeitpunkt empfangen wurden.
    updateUTCTimeVariables();
    loRaData.datumUTCWetterEmpfagen = zeit.DatumUTC;
    loRaData.uhrzeitUTCWetterEmpfagen = zeit.UhrzeitUTC;
    datenAnAnzeigeGesendet = false;
  } else if (button.status == 0 && button.zuvorGedrueckt == 1) {
    button.zuvorGedrueckt = 0;
  }

  // Falls noch nicht geschehen, sollen die neuesten Wetterdaten an die Anzeigestation gesendet werden:
  if (datenAnAnzeigeGesendet == false && httpAnfrageLaeuft == false && (timerAblaufzeitHttpAnfrage + ablaufzeitHttpAnfrage) < millis() && WLAN_STATUS == 0) {
    String wetterdatenFuerAnzeige = serverName
                                    + "datum=" + loRaData.datumUTCWetterEmpfagen + "&"
                                    + "uhrzeit=" + loRaData.uhrzeitUTCWetterEmpfagen + "&"
                                    + "temperatur=" + wetterdaten.temperaturString + "&"
                                    + "luftfeuchtigkeit=" + wetterdaten.luftfeuchtigkeitString + "&"
                                    + "pausenstatus=" + regenpause + "&"
                                    + "regentropfen=" + wetterdaten.regenstaerke;

    Serial.println(wetterdatenFuerAnzeige);

    httpAnfrageLaeuft = true;
    timerAblaufzeitHttpAnfrage = millis();

    http.initialize(wetterdatenFuerAnzeige);
    http.makeRequest(
      []() {  // onSuccess Callback
        Serial.println("Request erfolgreich!");
        Serial.println("HTTP Status Code: " + String(http.getStatusCode()));
        Serial.println("Antwort: " + http.getBody());

        if (http.getBody() == bestaetigungAnzeigestation) {
          datenAnAnzeigeGesendet = true;
        }

        httpAnfrageLaeuft = false;
      },
      [](String msg) {  // onFail Callback
        Serial.println("Fehler: " + msg);

        httpAnfrageLaeuft = false;
        WiFi.disconnect();  // WLAN trennen, sodass es neu verbunden werden kann. Dann wird die Anfrage automatisch erneut versucht und funktioniert damit vielleicht wieder.
      });
  }

  if ((timerAblaufzeitHttpAnfrage + ablaufzeitHttpAnfrage) < millis() && httpAnfrageLaeuft == true) {
    Serial.println("HTTP-Request Timeout, Verbindung wird zurückgesetzt.");
    httpAnfrageLaeuft = false;
  }


  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // Wenn ein LoRa-Paket empfangen wurde...
    Serial.println("LoRa-Daten empfangen: ");
    while (LoRa.available()) {
      loRaData.empfangenesPaket = LoRa.readString();  // Paket abspeichern
      Serial.print(loRaData.empfangenesPaket);        // und ausgeben
    }
    Serial.println("    Empfangsqualität:    RSSI: " + String(LoRa.packetRssi()) + " dBm    |    SNR: " + String(LoRa.packetSnr()) + " dB\n");  // Signalqualität ausgeben

    if (loRaData.empfangenesPaket.startsWith(loRaData.praefix + loRaData.typTemp + loRaData.seperator1) && loRaData.empfangenesPaket.indexOf(loRaData.seperator2) != -1 && loRaData.empfangenesPaket.indexOf(loRaData.seperator3) != -1 && loRaData.empfangenesPaket.indexOf(loRaData.seperator4) != -1) {
      // Normales Paket von Dachstation empfangen mit Temperatur und Luftfeuchtigkeit (keine Regenmessung)
      Serial.println("LoRaData starts with " + loRaData.praefix + loRaData.typTemp);
      verarbeiteLoRaWetterdaten();
      Serial.println("Temperatur: " + wetterdaten.temperaturString + "   Luftfeuchtigkeit: " + wetterdaten.luftfeuchtigkeitString + "\n");

      // Während auf dem Display Regenpause/ Schoenwetterpause angezeigt wird, soll nur die Temperatur im unteren Displaybereich angezeigt werden.
      // Solange die Pausen-LEDs noch leuchten, soll auf dem Display auch der Pausenstatus angezeigt bleiben.
      if (millis() < (timerPausenLEDsAus + dauerLEDsAnNachRegenmessung)) {
        LCD_PrintUnten("Temp: " + wetterdaten.temperaturString + " Grad");
      } else {
        LCD_PrintOben("Temp: " + wetterdaten.temperaturString + " Grad");
        LCD_PrintUnten("LF: " + wetterdaten.luftfeuchtigkeitString + "%");
      }
    } else if (loRaData.empfangenesPaket.startsWith(loRaData.praefix + loRaData.typRain + loRaData.seperator1) && loRaData.empfangenesPaket.indexOf(loRaData.seperator2) != -1 && loRaData.empfangenesPaket.indexOf(loRaData.seperator3) != -1 && loRaData.empfangenesPaket.indexOf(loRaData.seperator4) != -1) {
      // Paket von der Dachstation enthält ein Ergebnis der Regenmessung
      Serial.println("LoRaData starts with " + loRaData.praefix + loRaData.typRain);
      verarbeiteLoRaWetterdaten();
      wetterdaten.regenstaerke = loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperator4) + 1).toInt();
      Serial.println("Regenstärke:" + String(wetterdaten.regenstaerke) + "   Temperatur: " + wetterdaten.temperaturString + "   Luftfeuchtigkeit: " + wetterdaten.luftfeuchtigkeitString + "\n");

      // Lege Pausenstatus anhand der Wetterdaten fest:
      if (wetterdaten.temperaturFloat <= schwelleKaeltepause) {  // Wenn die Temperatur kleiner/ gleich ist, als/ wie die Schwelle, dann ist eine Kältepause
        digitalWrite(LED_Regenpause, HIGH);
        digitalWrite(LED_KeineRegenpause, LOW);

        LCD_PrintOben("Kaeltepause!");
        regenpause = 2;                                  // 2 steht für Kältepause
      } else {                                           // Wenn keine Kältepause ist, dann...
        if (wetterdaten.regenstaerke > regenschwelle) {  //... ist entweder eine Regenpause ...
          digitalWrite(LED_Regenpause, HIGH);
          digitalWrite(LED_KeineRegenpause, LOW);

          LCD_PrintOben("Regenpause!");
          regenpause = 1;
        } else {  // oder eine Schönwetterpause/ keine Regenpause
          digitalWrite(LED_Regenpause, LOW);
          digitalWrite(LED_KeineRegenpause, HIGH);

          LCD_PrintOben("Keine Regenpause");
          regenpause = 3;
        }
      }

      LCD_PrintUnten("Temp: " + wetterdaten.temperaturString + " Grad");

      // Variablen einstellen, damit die LEDs nach der eingestellten Zeit ausgehen und sich die Pause mit dem Button im festgelegten Rahmen ändern lässt
      statusWetterLEDs = 1;
      regenpausenAenderungsTimer = millis();
      timerPausenLEDsAus = millis();
    } else {
      Serial.println("Daten nicht von Dachstation oder sie wurden im falschen Format gesendet.");
    }
  }


  if (millis() > (timerPausenLEDsAus + dauerLEDsAnNachRegenmessung) && statusWetterLEDs == 1) {
    // LEDs nach eingesteller Zeit automatisch wieder ausschalten und Display auf normalen Modus setzen
    Serial.println("Setze LEDs, Pausenstatus, usw. zurück!");
    statusWetterLEDs = 0;
    regenpause = 0;
    wetterdaten.regenstaerke = 0;
    datenAnAnzeigeGesendet = false;  // wird auf false gesetzt, dass der Anzeigestation auch wieder der normale Status gesendet wird
    Serial.println("Schalte LEDs aus und setze Display wieder auf normalen Status");
    LCD_PrintOben("Temp: " + wetterdaten.temperaturString + " Grad");
    LCD_PrintUnten("LF: " + wetterdaten.luftfeuchtigkeitString + "%");
    digitalWrite(LED_Regenpause, LOW);
    digitalWrite(LED_KeineRegenpause, LOW);
  }


  // Wenn zu lange keine Daten von der Dachstation empfangen wurden:
  if (millis() > (datenZuletztEmpfangenVonDach + schwelleWarnmeldungDatenZuletztEmpfangenVonDach)) {
    Dachstation_STATUS = 1;
  }

  // Wenn es Probleme gibt (kein WLAN oder zu lange keine Wetterdaten empfangen), dann soll die rote statt der grünen LED leuchten:
  if (Dachstation_STATUS + WLAN_STATUS > 0) {
    digitalWrite(LED_Status_Gruen, LOW);
    digitalWrite(LED_Status_Rot, HIGH);
  } else {
    digitalWrite(LED_Status_Gruen, HIGH);
    digitalWrite(LED_Status_Rot, LOW);
  }

  // Die millis() laufen nach etwa 50 Tagen über. Um Fehler zu verhindern, wird der ESP32 nach etwa 46 Tagen neugestartet
  // Dies passiert erst, wenn keine Pause ist und keine andere Schlafenszeit als die Standartschlafenszeit an die Dachstation gesendet wurde,
  // weil dann demnächst eine Pause ansteht und der ESP32 nicht dann neu startet, wenn er eine Regenmessung anfordern müsste:
  if (millis() > 4000000000 && regenpause == 0 && dauerZuletztGesendeteSchlafenszeit == dauerSchlafenszeitDachstation) {
    Serial.println("========== Starte Sekretariatsstation neu, weil millis überlaufen würden!!! ==========");
    ESP.restart();  // Befehl, um den ESP32 neu zu starten
  }
}


// ============================================================ Funktionen ============================================================
// Funktion, um in der oberen Zeile des LC-Displays Text anzuzeigen:
void LCD_PrintOben(String textOben) {
  LCD.setCursor(0, 0);
  LCD.print("                ");
  LCD.setCursor(0, 0);
  LCD.print(textOben);
}

// Funktion, um in der unteren Zeile des LC-Displays Text anzuzeigen:
void LCD_PrintUnten(String textUnten) {
  LCD.setCursor(0, 1);
  LCD.print("                ");
  LCD.setCursor(0, 1);
  LCD.print(textUnten);
}

// Funktion, die die Zeitvariablen updatet
void updateLocalTimeVariables() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    zeit.stunde = timeinfo.tm_hour;
    zeit.minute = timeinfo.tm_min;
    zeit.sekunde = timeinfo.tm_sec;
    zeit.tag = timeinfo.tm_mday;
    zeit.monat = timeinfo.tm_mon + 1;     // tm_mon ist 0-basiert
    zeit.jahr = timeinfo.tm_year + 1900;  // tm_year ist seit 1900
    zeit.wochentag = timeinfo.tm_wday;

    zeit.Datum = String(zeit.jahr) + "-" + String(zeit.monat) + "-" + String(zeit.tag);  // Das Trennzeichen muss ein "-"" sein, dass die Daten im richtigen Format an Anzeigestation gesendet werden

    // Die Zeit wird in dem String zeit.Uhrzeit gespeichert. Da die Stunden und Minuten nicht immer aus 2 Ziffern bestehen, muss evtl. eine 0 hinzugefügt werden.
    // Dies ist auch nötig, dass die Daten von der Anzeigestation richtig angenommen werden können.
    if (zeit.minute < 10) {
      zeit.Uhrzeit = String(zeit.stunde) + ":0" + String(zeit.minute);
    } else {
      zeit.Uhrzeit = String(zeit.stunde) + ":" + String(zeit.minute);
    }
    if (zeit.sekunde < 10) {
      zeit.Uhrzeit += ":0" + String(zeit.sekunde);
    } else {
      zeit.Uhrzeit += ":" + String(zeit.sekunde);
    }

    Serial.println("Datum: " + String(zeit.Datum) + "    Uhrzeit: " + String(zeit.Uhrzeit) + "    Wochentag: " + String(zeit.wochentag) + "\n");
  } else {
    // Falls die Zeit am Amfang im setup() nicht richtig synchronisiert wurde, soll sich der ESP32 nestarten und damit WLAN erneut aufbauen und Zeit erneut synchronisieren
    Serial.println("Fehler beim updaten der Zeitvariablen!!! Starte neu...");
    LCD.clear();
    LCD.setCursor(0, 0);
    LCD.print("Zeit nicht syn-");
    LCD.setCursor(0, 1);
    LCD.print("chronisiert!!!");
    delay(1000);
    LCD.clear();
    LCD.setCursor(0, 0);
    LCD.print("Starte neu...");
    delay(1000);
    ESP.restart();  // Befehl, um den ESP32 neu zu starten
  }
}


void updateUTCTimeVariables() {
  time_t now = time(nullptr);         // Aktueller Unix-Timestamp
  struct tm *utcTime = gmtime(&now);  // Konvertiere Zeitstempel zu UTC

  if (utcTime != nullptr) {
    // Extrahiere UTC-Zeit in Variablen
    Zeit utcZeit;
    utcZeit.stunde = utcTime->tm_hour;
    utcZeit.minute = utcTime->tm_min;
    utcZeit.sekunde = utcTime->tm_sec;
    utcZeit.tag = utcTime->tm_mday;
    utcZeit.monat = utcTime->tm_mon + 1;  // + 1, weil nullbasiert
    utcZeit.jahr = utcTime->tm_year + 1900;

    zeit.DatumUTC = String(utcZeit.jahr) + "-" + String(utcZeit.monat) + "-" + String(utcZeit.tag);
    if (utcZeit.minute < 10) {
      zeit.UhrzeitUTC = String(utcZeit.stunde) + ":0" + String(utcZeit.minute);
    } else {
      zeit.UhrzeitUTC = String(utcZeit.stunde) + ":" + String(utcZeit.minute);
    }
    if (utcZeit.sekunde < 10) {
      zeit.UhrzeitUTC += ":0" + String(utcZeit.sekunde);
    } else {
      zeit.UhrzeitUTC += ":" + String(utcZeit.sekunde);
    }

    // Ausgabe der UTC-Zeit
    Serial.println("UTC-Datum: " + zeit.DatumUTC);
    Serial.println("UTC-Uhrzeit: " + zeit.UhrzeitUTC);
  } else {
    Serial.println("Fehler beim Abrufen der UTC-Zeit");
  }
}


// Funktion, die die empfangenen LoRa-Wetterdaten von der Dachstation in die entsprechenden Variablen schreibt
void verarbeiteLoRaWetterdaten() {
  datenZuletztEmpfangenVonDach = millis();  // Wird auf millis gesetzt, dass man weiß wann zuletzt Daten empfangen wurden und ermittelt werden kann, wenn zu lange keine Daten mehr empfangen wurden
  // Um die Uhrzeiten richtig an die Anzeigestation senden zu können, wird die Zeit zu der die LoRa-Wetterdaten empfangen wurden im UTC-Format gespeichert:
  updateUTCTimeVariables();  // Zuvor weden die Zeitvariablen aktualisiert
  loRaData.datumUTCWetterEmpfagen = zeit.DatumUTC;
  loRaData.uhrzeitUTCWetterEmpfagen = zeit.UhrzeitUTC;
  loRaData.rssiWetterdaten = LoRa.packetRssi();
  loRaData.snrWetterdaten = LoRa.packetSnr();

  // Die empfangenen Daten werden in die entsprechenden Variablen gespeichert
  wetterdaten.temperaturFloat = (loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperator2) + 1, loRaData.empfangenesPaket.indexOf(loRaData.seperator3))).toFloat();
  wetterdaten.luftfeuchtigkeitFloat = (loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperator3) + 1, loRaData.empfangenesPaket.indexOf(loRaData.seperator4))).toFloat();

  spannungDachstation = loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperator1) + 1, loRaData.empfangenesPaket.indexOf(loRaData.seperator2));
  Serial.println("Akku-Spannung Dachstation: " + spannungDachstation + "V");

  // In den jeweiligen String der Temperatur und Luftfeuchtigkeit wird nur 1 Nachkommastelle gespeichert:
  wetterdaten.temperaturString = String(wetterdaten.temperaturFloat, 1);
  wetterdaten.luftfeuchtigkeitString = String(wetterdaten.luftfeuchtigkeitFloat, 1);
  sendDataToDach();                // Funktion, um der Dachstation zu antworten.
  Dachstation_STATUS = 0;          // Der Status wird auf 0 gesetzt, weil Daten empfangen wurden.
  datenAnAnzeigeGesendet = false;  // Die gerade eben empfangenen Wetterdaten wurden noch nicht an die Anzeigestation gesendet.
}


void sendDataToDach() {                       //Daten zu Dachstation senden.
  Serial.println("\nAntworte Dachstation.");  // (\n bewirk Zeilenumbruch)

  updateLocalTimeVariables();  // Aktualisiere Zeitvariablen

  int i = 0;

  // Im folgenden wird überprüft, ob eine Regenmessung angefordert weden muss, oder nur die normale Schlafenszeit.

  if (messTage[zeit.wochentag] == 1 && anzahlPausen != 0) {  // Wenn am jeweiligen Tag eine Regenmessung durchgeführt werden soll und es überhaupt eine Pause gibt
    // Für jede Pause muss überprüft werdenden, ob sie demnächst ansteht und eine Regenmessung gestartet werden muss
    for (i = 0; i < anzahlPausen; i++) {
      Serial.print("  Überprüfe, ob Pause " + String(i) + " ansteht:  ");

      // Die jeweilige Pause wird zwischengespeichert
      int messStartMinute = messStartMinuten[i];
      int messStartStunde = messStartStunden[i];

      // Da es einen kleinen Zeitraum gibt, in dem die Messung gestartet wird, wird in den beiden Variablen die Endzeit gespeichert,
      // also wann spätestens noch eine Regenmessung gestartet wird.
      int messStartEndMinute = messStartMinuten[i] + pufferBeginnRegenmessung;
      int messStartEndStunde = messStartStunden[i];

      // Falls die normale Startzeit der Regemessung bei Minute 59 ist und durch den Puffer ein Wert größer 60 entesteht, werden
      // 60 Minuten abgezogen, sodass bei einem Puffer von 3: 59+3=62 -> 62-60=2 bei Minute 2 die späteste Startzeit einer Regenmessung ist.
      if (messStartEndMinute >= 60) {
        messStartEndMinute -= 60;
        messStartEndStunde++;

        // Fals durch die Anpassung der Stunde ein größerer Wert als 23 herauskommt, muss die Stunde ebenfalls wie beim Prinzip der Minuten angepasst werden.
        if (messStartEndStunde >= 24) {
          messStartEndStunde -= 24;
        }
      }

      // Variablen, in denen gespeichert wird, ab wann die Schlafenszeit der Dachstation verkürzt werden muss, dass Regenmessung rechtzeitig gestartet wird.
      int zeitpunktKurzerSchlafMinute = messStartMinute - dauerSchlafenszeitDachstation;
      int zeitpunktKurzerSchlafStunde = messStartStunde;

      if (zeitpunktKurzerSchlafMinute < 0) {
        zeitpunktKurzerSchlafMinute += 60;
        zeitpunktKurzerSchlafStunde--;

        if (zeitpunktKurzerSchlafStunde < 0) {
          zeitpunktKurzerSchlafStunde += 24;
        }
      }

      // Für jede Pause wird der Messstartzeitraum ausgegeben:
      Serial.println("Messstartzeitraum Pause Nr." + String(i) + ": " + String(messStartStunde) + ":" + String(messStartMinute) + " - " + String(messStartEndStunde) + ":" + String(messStartEndMinute));

      // Prüfe für die jeweilige Pause, ob sie demnächst stattfindet und eine Regenmessung angefordert werden muss,
      // oder die Standardschlafenszeit verkürzt werden muss, dass die Regenmessung rechtzeitig gestartet werden kann.
      if ((zeit.stunde == messStartStunde && zeit.minute >= messStartMinute && zeit.minute < messStartEndMinute)
          || (zeit.stunde == messStartEndStunde && zeit.minute <= messStartEndMinute && messStartStunde != messStartEndStunde)) {
        Serial.println("Fordere Regenmessung für Pause Nr." + String(i) + " an!\n");

        LoRa.beginPacket();
        LoRa.print(loRaData.praefix + loRaData.typRain + loRaData.seperatorDauerRmessung);
        LoRa.print(String(dauerRegenmessung * 60000));
        LoRa.endPacket();

        // Um die for-Schleife vorzeitig zu beenden wird i auf anzahlPausen + 1 gesetzt und die andere Pausen nicht mehr überprüft, weil ja schon eine Regenmessung angefordert wurde.
        // Das + 1 sorgt dafür, dass später nicht noch die normale Schlafenszeit gesendet wird und i nicht gleich anzahlPausen ist.
        i = anzahlPausen + 1;
      } else if (zeit.stunde == zeitpunktKurzerSchlafStunde && zeit.minute > zeitpunktKurzerSchlafMinute) {
        // Überprüfen, ob die normale Schlafenszeit zu lange wäre:
        int differenzMinuten = messStartMinute - zeit.minute;

        if (differenzMinuten < 0) {
          differenzMinuten += 60;
        }

        if (differenzMinuten < dauerSchlafenszeitDachstation) {
          Serial.println("Schicke kürzere Schlafenszeit vor Regenmessung: " + String(differenzMinuten) + " min. = " + String(differenzMinuten * 60000) + " Millisek.\n");
          dauerZuletztGesendeteSchlafenszeit = differenzMinuten * 60000;  // Damit ermittelt werden kann, ob eine normale Schlafenszeit gesendet wurde oder nicht, wird die angepasste Zeit gespeichert.
          LoRa.beginPacket();
          LoRa.print(loRaData.praefix + loRaData.typSleep + loRaData.seperatorSleep + dauerZuletztGesendeteSchlafenszeit);
          LoRa.endPacket();

          i = anzahlPausen + 1;  // Gleiches Prinzip wie zuvor.
        }
      }
    }
  } else {
    Serial.println("Heute findet keine Regenmessung statt!");
    i = anzahlPausen;
  }

  // Falls keine der Pausen ansteht, wird die normale Schlafenszeit gesendet
  if (i == anzahlPausen) {  // Wenn durch die for-Schleife alle Pausen überprüft wurden und keine Pause ansteht, dann ist i gleich anzahlPausen.
                            // Wenn eine Pause ansteht wurde i auf anzahlPausen + 1 gesetzt, sodass nicht noch unnötig die normale Schalfenszeit gesendet wird.
    Serial.println("Schicke normale Schlafenszeit: " + String(dauerSchlafenszeitDachstation) + "\n");
    dauerZuletztGesendeteSchlafenszeit = dauerSchlafenszeitDachstation * 60000;  // Die normale Schalfenszeit wurde zuletzt gesendet.
    LoRa.beginPacket();
    LoRa.print(loRaData.praefix + loRaData.typSleep + loRaData.seperatorSleep);
    LoRa.print(dauerZuletztGesendeteSchlafenszeit);
    LoRa.endPacket();
  }
}


// Route für die Startseite
void handleStartseite(AsyncWebServerRequest *request) {
  // IP-Adresse des Client ausgeben:
  Serial.print("Website aufgerufen: Startseite von: ");
  IPAddress clientIP = request->client()->remoteIP();
  Serial.println(clientIP);

  // Hintergrundfarbe für die Website:
  String pausenfarbe;
  if (regenpause == 3) {
    website.pausenStatus = "<p><big>Keine Regenpause!</big></p>";
    pausenfarbe = website.keineRegenpausenFarbe;
  } else if (regenpause == 1) {
    website.pausenStatus = "<p><big>Regenpause!</big></p>";
    pausenfarbe = website.regenpausenFarbe;
  } else if (regenpause == 2) {
    website.pausenStatus = "<p><big>Kaeltepause!</big></p>";
    pausenfarbe = website.regenpausenFarbe;
  } else {
    website.pausenStatus = "";
    pausenfarbe = "#88ff88";
  }

  updateLocalTimeVariables();
  website.Pausenstatus = pausenfarbe + "; } </style>" + "<body><h1><u>Regenpausenanzeiger</u></h1><div class=\" container \">" + website.pausenStatus;
  website.TempHum = "<p><u>Temperatur:</u> " + wetterdaten.temperaturString + " Grad</p><p><u>rel. Luftfeuchtigkeit:</u> " + wetterdaten.luftfeuchtigkeitString + "%</p>";
  if (regenpause != 0) {  //Zeige Tropfenanzahl nur an, wenn eine Pause ist (-> regenpause ungleich 0).
    website.AnzahlTropfen = "<p><small><i><u>Anzahl Tropfen:</u> " + String(wetterdaten.regenstaerke) + "</i></small></p>";
    website.TempHum += website.AnzahlTropfen;  //Der String für die Tropfenanzahl wird angehängt. Die Tropfenanzahl soll nämlich nur bei einer Regenpause angezeigt werden.
  }
  website.DatenVon = "<p><small><i><u>Akku-Spannung Dachstation:</u> " + spannungDachstation + "V</i></small>"
                     + "<br><small><i><u>Empfangsqualitaet:</u> " + "RSSI: " + loRaData.rssiWetterdaten + "dBm SNR: " + loRaData.snrWetterdaten + "dB</i></small>"
                     + "<br><small><i><u>Daten vom:</u> " + loRaData.datumUTCWetterEmpfagen + " um " + loRaData.uhrzeitUTCWetterEmpfagen + " UTC</i></small><br/></p>";


  spannungDachstation = loRaData.empfangenesPaket.substring(loRaData.empfangenesPaket.indexOf(loRaData.seperator1) + 1, loRaData.empfangenesPaket.indexOf(loRaData.seperator2));
  request->send(200, "text/html", website.Head + website.Style + website.Pausenstatus + website.TempHum + website.DatenVon + website.Ende);
}


// Route für die Admin-Seite
void handleAdmin(AsyncWebServerRequest *request) {
  // IP-Adresse des Client ausgeben:
  Serial.print("Website aufgerufen: Startseite von: ");
  IPAddress clientIP = request->client()->remoteIP();
  Serial.println(clientIP);
  request->send(200, "text/html", "<h1>Admin-Seite</h1><p>Hier koennten Admin-Funktionen sein.</p>");
}


// Route für LoRa-Sendung
// Mit dieser Funktion lassen sich über die Sekretariatsstation LoRa-Pakete senden
void handleSendPacketViaLoRa(AsyncWebServerRequest *request) {
  // IP-Adresse des Client ausgeben:
  Serial.print("Website aufgerufen: Startseite von: ");
  IPAddress clientIP = request->client()->remoteIP();
  Serial.println(clientIP);

  if (request->hasParam("data")) {
    String loraString = request->getParam("data")->value();
    LoRa.beginPacket();
    LoRa.print(loraString);
    LoRa.endPacket();

    request->send(200, "text/plain", "Daten empfangen und gesendet: " + loraString);
  } else {
    request->send(400, "text/plain", "Fehlende Datenparameter! Format: Hostname oder IP/sendPacketViaLoRa?data=...");
  }
}


// Route für die Info-Seite
void handleInfo(AsyncWebServerRequest *request) {
  // IP-Adresse des Client ausgeben:
  Serial.print("Website aufgerufen: Startseite von: ");
  IPAddress clientIP = request->client()->remoteIP();
  Serial.println(clientIP);

  String infoPage = "<h1>Informationen zum Regenpausenanzeiger (Sekretariatsstation)</h1>";
  infoPage += "<p>Hier sind die Links zu den verschiedenen Seiten der Sekretariatstation:</p>";
  infoPage += "<ul>";
  infoPage += "<li><a href=\"/\">Startseite</a></li>";
  infoPage += "<li><a href=\"/admin\">Admin-Seite</a></li>";
  infoPage += "<li><a href=\"/sendPacketViaLoRa\">LoRa-Seite</a></li>";
  infoPage += "</ul>";
  request->send(200, "text/html", infoPage);
}