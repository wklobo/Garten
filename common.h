//*********************************************************************************************//
//*                                                                                           *//
//* File:          common.h                                                                   *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2021-04-18;                                                                *//
//* Last change:   2021-10-24 - 17:35:38                                                     *//
//* Description:   Hilfsfunktionen und  Vereinbarungen zwischen den Programmen                *//
//*                                                                                           *//
//* Copyright (C) 2019-21 by Wolfgang Keuch                                                   *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef _COMMON_H
#define _COMMON_H  1

#include <stdbool.h>

// Steuerung Log-Datei
// -------------------
#define __LAMPMOTION_MYLOG__   true

// um Warnungen 'unused' zu vermeiden
// -------------------------------------------
#define UNUSED(x) (void)(x)

// Buffer-Größen
// -------------
#define NOTIZ                64       /* Kurzmeldung */
#define ZEILE               256
#define BUFFER         256*1024
#define MIN_FILESIZE    72*1024

#define NOINT -999999999              /* kein Integer-Wert */

// Aufbewahrungsdauer Filme/Bilder
// -------------------------------
#define SEC  1000 /* -- msec */
#define STD   60*60 /* -- min  */
#define SOFORT_h        0
#define KURZ_h          1
#define MITTEL_h       25             /* Aufbewahrungsdauer [h]: Eine Tag + 1 Std  */
#define LANG_h        170             /* Aufbewahrungsdauer [h]: Eine Woche + 2 Std  */
#define SEHRLANG_h    746             /* Aufbewahrungsdauer [h]: LED_EIN Monat + 2 Std */
#define GANZLANG_h   2208             /* Aufbewahrungsdauer [h]: Drei Monate */

#define MAXALTER   SEHRLANG_h         /* Aufbewahrungsdauer  */

#define  FFAKTOR     1000             /* zur Übergabe Datei/Verzeichnis */
#define  DFAKTOR  1000000

#define BRENNDAUER     18             /* [sec]...rote Fehler-LED */
#define REFRESH       300             /* [sec] - Auffrischung Watchdog-Datei */


// Ruhezeit
// --------
#define GUTENACHT     "22:30"         /* IR-Lampen ausschalten */
#define GUTENMORGEN    "7:10"         /* IR-Lampen einschalten */


// MQTT-Trennzeichen
// ------------------
#ifndef DELIM
#define DELIM '/'
#endif


// Verzeichnisse
// --------------
#define _FOLDER       "event_"                  /* Kennzeichnung Verzeichnis */
#define _EVENT_       "Event_"                  /* Kennzeichnung Verzeichnis */
#define FDIR          "/home/pi/Garten/aux"
#define FIFO          "/home/pi/Garten/aux/MOTION.FIFO"
#define FPID          "/home/pi/Garten/aux/%s.pid"
#define SOURCE        "/home/pi/Garten/pix/"
#define DESTINATION   "/media/Garten/"    			/* = 'DISKSTATION/surveillance'/... */

// allgemeine Log-Datei
// --------------------
#define  LOGFILE      "/home/pi/Garten/aux/Motion.log"


// stream_port aus 'kamara1.con'
// ----------------------------------
// The mini-http server listens to this port for requests
#define STREAM_PORT   2401


// Messwerte
// ---------
#define NICHTVORHANDEN -999.999       /* keine Sensor */
#define FEHLLESUNG      -99.99        /* Fehllesung */


// LED-Steuerung (Ersatz für motLED.c)
// -------------------------------------
#define LED_EIN      0
#define LED_AUS      1
#define LED_HELL     1                    /* IR-Lampen */
#define LED_DUNKEL   0

// Pin-Nummerierung: WiringPi
// --------------------------
#define LED_ROT       1      /* Pin  8  */
#define LED_GELB      6      /* Pin 22  */
#define LED_GRUEN    16      /* Pin 10  */
#define LED_BLAU     15      /* Pin 12  */
#define LED_GRUEN1   11      /* Pin 28  */
#define LED_BLAU1    10      /* Pin 26  */
#define LAMP_IRRIGHT  2  /* Pin 13 - IR-Lampe rechts */ // /home/pi/Garten/Pin/Pin 16 0
#define LAMP_IRLEFT   0  /* Pin 11 - IR-Lampe links  */

// Pin-Nummerierung: WiringPi - alt Vogel
// --------------------------------------
//#define LED_ROT      15  /* Pin  8  */
//#define LED_GELB     16  /* Pin 10  */
//#define LED_GRUEN     1  /* Pin 12  */
//#define LED_BLAU      4  /* Pin 16  */
//#define LED_GRUEN1   24  /* Pin 35  */
//#define LED_BLAU1    25  /* Pin 37  */
//#define LAMP_IRRIGHT  2  /* Pin 13 - IR-Lampe rechts */ // /home/pi/Garten/Pin/Pin 16 0
//#define LAMP_IRLEFT   0  /* Pin 11 - IR-Lampe links  */


// Pin-Nummerierung: WiringPi - alt Kamera
// -----------------------------------------
//#define SQLBLUE       15      /* Pin 12  */
//#define SQLRED         1      /* Pin  8  */
//#define SQLGREEN      16      /* Pin 10  */
//#define MOTIONORANGE   6      /* Pin 22  */
//#define FIFORED       31      /* Pin 24  */
//#define FIFOBLUE      10      /* Pin 26  */
//#define FIFOGREEN     11      /* Pin 28  */



// Debug-Formate
// -------------
#define LOG_AUSGABE(text)  syslog(LOG_NOTICE, ">> %s: %s", __FIFO__, text)
#define DEBUG_AUSGABE0(text) fprintf(stdout, "%s\n", text)
#define DEBUG_AUSGABE2(text) fprintf(stdout, " %s\n", text)
#define DEBUG_AUSGABE4(text) fprintf(stdout, " %s\n", text)
#define DEBUG_AUSGABE6(text) fprintf(stdout, " %s\n", text)


// Geräte und Sensoren
// -------------------
// Kennzeichnung im MQTT-Topic (Device-ID)
enum Device { D_NODEVICE,
  D_MQTT=100,
  D_IRLAMPE_RECHTS,
  D_IRLAMPE_LINKS,
  D_IRLAMPEN,					/* beide betreffend */
  D_LED_ROT,
  D_LED_GELB,
  D_LED_GRUEN,
  D_LED_BLAU,
  D_LED_GRUEN1,
  D_LED_BLAU1,
  D_DS18B20_00,
  D_DS18B20_01,
  D_DS18B20_02,
  D_DS18B20_03,
  D_DS18B20_04,
  D_DS18B20_05,
  D_DS18B20_06,
  D_DS18B20_07,
  D_DS18B20_08,
  D_DS18B20_09,
  D_DS18B20_10,
  D_DS18B20_11,
  D_BME280_T,
  D_BME280_P,
  D_BME280_H,
  D_TLS2561,
  D_INTERN,
  D_xxx,
  D_ENDE
};



// Zeichen im String ersetzen
// ------------------------------------------
// Rückgabewert: Anzahl der ersetzen Zeichen
unsigned replace_character(char* string, char from, char to);

// überflüssigen Whitespace entfernen
// ----------------------------------
char* trim(char* myString);

// Aufruf einens Kommandozeilen-Befehls
// ------------------------------------
// gibt das Ergebnis in 'Buf' zurück
bool getSystemCommand(char* Cmd, char* Buf, int max);

// aktuelle IP-Adresse auslesen
// -----------------------------
char* readIP(char* myIP, int max);

// PID in Datei ablegen
// ----------------------
long savePID(char* dateipfad);

// das n.te Glied eines Strings zurückgeben
// -----------------------------------------
bool split(const char *msg, char *part, int n);

// um 'not used'-Warnings zu vermeiden:
// -----------------------------------
void destroyInt(int irgendwas);
void destroyStr(char* irgendwas);

// Abfrage auf numerischen String
// ------------------------------
bool isnumeric(char* numstring);

// numerisches Wert aus String holen
// ---------------------------------
bool getnumeric(char* instring, char* numeric);

// Ist dieser String ein Datum?
// ----------------------------
// wie '2021-05-09'
bool isDatum(char* einString);


bool match(const char *topic, const char *key, int n);    // Stringvergleich
bool matchn(const char *topic, int key, int n);           // Stringvergleich
bool partn(const char* String, int pos, char* Ziel);      // n.tes Glied des Strings zurückgeben


// Watchdog-Datei auffrischen
// ---------------------------
bool feedWatchdog(char* Name);

// RaspberryPi-Bezeichnung lesen
// ----------------------------
char* readRaspiID(char* RaspiID);

// Protokoll schreiben
// ------------------------
bool MyLog(const char* Program, const char* Function, int Line, const char* pLogText);

// alle Vorkommen eines Strings im String ersetzen
// -----------------------------------------------
//bool replace(const char* OldText, char* NewText, const char* oldW, const char* newW );


// Dateityp Bild/Film ermitteln
// ----------------------------
enum Filetype
{
  OHNE=0,
  JPG,
  AVI,
  MKV,
  ANDERE=99
};
#define _JPG ".jpg"
#define _AVI ".avi"
#define _MKV ".mkv"

enum Filetype getFiletyp(const char* Filename);

#endif //_COMMON_H

//*********************************************************************************************//

// ASCII-Tabelle
// =============

#define NUL       0x00     /* Null Character */
#define SOH       0x01     /* Start of Header  */
#define STX       0x02     /* Start of Text  */
#define ETX       0x03     /* End of Text  */
#define EOT       0x04     /* End of Transmission  */
#define ENQ       0x05     /* Enquiry  */
#define ACK       0x06     /* Acknowledge  */
#define BEL       0x07     /* Bell (Ring)*/
#define BS        0x08     /* Backspace  */
#define HT        0x09     /* Horizontal Tab */
#define LF        0x0A     /* Line Feed  */
#define VT        0x0B     /* Vertical Tab */
#define FF        0x0C     /* Form Feed  */
#define CR        0x0D     /* Carriage Return  */
#define SO        0x0E     /* Shift Out  */
#define SI        0x0F     /* Shift In */
#define DLE       0x10     /* Data Link Escape */
#define DC1       0x11     /* Device Control 1 */
#define DC2       0x12     /* Device Control 2 */
#define DC3       0x13     /* Device Control 3 */
#define DC4       0x14     /* Device Control 4 */
#define NAK       0x15     /* Negative Acknowledge */
#define SYN       0x16     /* Synchronize  */
#define ETB       0x17     /* End Transmission Block */
#define CAN       0x18     /* Cancel */
#define EM        0x19     /* End Of Medium  */
#define SUB       0x1A     /* Substitute */
#define ESC       0x1B     /* Escape */
#define FS        0x1C     /* File Separator */
#define GS        0x1D     /* Group Separator  */
#define RS        0x1E     /* Record Separator */
#define US        0x1F     /* Unit Separator */
// missbrauchte ASCIIzeichen:
#define NIX       ' '      /* 0x20 - Space */
#define EIN       '!'      /* 0x21         */
#define AUS       '"'      /* 0x22         */
#define HELL      '#'      /* 0x23         */
#define DUNKEL    '$'      /* 0x24         */
#define SCHALTEN  '%'      /* 0x25         */
#define MESSEN    '&'      /* 0x26         */
#define LEER      '\''     /* 0x27         */
#define AUTO      '('      /* 0x28         */
#define GRENZE    ')'      /* Begrenzung   */
 
// Steuerzeichen über String finden
// --------------------------------
#define Ctrl char
Ctrl Str2Ctrl(char* strControl);

// Steuerzeichen als String ausgeben
// ----------------------------------
bool Ctrl2Str(char Control, char* strControl);

// String zu Großbuchstaben
// -------------------------
char* toUpper(char* low);

//*********************************************************************************************//
