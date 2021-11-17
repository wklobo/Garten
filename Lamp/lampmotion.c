//********** Kamera *********************************************************//
//*                                                                         *//
//* File:          lampmotion.c                                             *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2021-04-05;                                              *//
//* Last change:   2021-11-15 - 15:39:54                                    *//
//* Description:   Nistkastenprogramm - ergänzt 'fifomotion':               *//
//*                Steuerung der Infrarot-Lampen                            *//
//*                Verwaltung der Umwelt-Sensoren                           *//
//*                Kommunikation per MQTT                                   *//
//*                                                                         *//
//* Copyright (C) 2014-21 by Wolfgang Keuch                                 *//
//*                                                                         *//
//* Aufruf:                                                                 *//
//*    ./LampMotion &    (Daemon)                                           *//
//*                                                                         *//
//***************************************************************************//

#define _MODUL0
#define __LAMPMOTION_DEBUG__   true
#define __LAMPMOTION_DEBUG_1__ true
#define __LAMPMOTION_DEBUG_2__ false

#include "./version.h"
#include "./lampmotion.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <wiringPi.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <i2c/smbus.h>

#include "./mqtt.h"
#include "./ds1820.h"
#include "./bme280.h"
#include "./tsl2561.h"
#include "../error.h"
#include "../datetime.h"
#include "../../sendmail/sendMail.h"

// statische Variable
// --------------------
static char Hostname[ZEILE];          // der Name dieses Rechners
static char meineIPAdr[NOTIZ];        // die IP-Adresse dieses Rechners
static char* IPnmr;                   // letzte Stelle der IP-Adresse
static time_t ErrorFlag = 0;          // Steuerung rote LED
static bool Automatic = false;        // Steuerung IR-Lampem

// Message-Erwiderung
// --------------------
enum RESPONSE { RESP_KEINEANTWORT,
                RESP_QUITTUNG=200,
                RESP_NOSUPPORT,
                RESP_FEHLER};

#define SENDETAKT      60   // für Sensoren [sec]
#define ABFRAGETAKT   250   // für MQTT [msec]


//***************************************************************************//
/*
 * Define debug function.
 * ---------------------
 */
#if __LAMPMOTION_MYLOG__
#define MYLOG(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define MYLOG(...)
#endif

#if __LAMPMOTION_DEBUG__
#define DEBUG(...)  printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#if __LAMPMOTION_DEBUG_1__
#define DEBUG_1(...)  printf(__VA_ARGS__)
#else
#define DEBUG_1(...)
#endif


#if __LAMPMOTION_DEBUG_2__
#define DEBUG_2(...)  printf(__VA_ARGS__)
#else
#define DEBUG_2(...)
#endif

//***************************************************************************//
//
////#ifndef _MQTTTYPEN
////#define _MQTTTYPEN  1
//
//char* MessageTyp[] =
//{ "",
//  "allmeine Information",
//  "aktueller Status",
//  "Messwert",
//  "Verwaltung",
//  "Schaltbefehl",
//  "Fehlermeldung",
//  NULL
//};
////#endif

//
//***********************************************************************************************

// fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_Error( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  sprintf( Fehler, "%s - Err %d-%s", Message, errsv, strerror(errsv));
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  printf("    -- Fehler -->  %s\n", ErrText);   // lokale Fehlerausgabe
  digitalWrite (LED_gn2,  LED_AUS);
  digitalWrite (LED_bl2,   LED_AUS);
  digitalWrite (LED_rt,    LED_EIN);

  finish_with_Error(ErrText);                   // Fehlermeldung ausgeben
}
//***********************************************************************************************

 // nicht-fataler Fehler
 // ------------------------
 // Lokale Fehlerbearbeitung
 // Fehler wird nur geloggt
int Error_NonFatal( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  if (errsv == 0)
    sprintf( Fehler, "%s", Message);            // kein Fehler, nur Meldung
  else
    sprintf( Fehler, "%s - Err %d-%s", Message, errsv, strerror(errsv));

  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  DEBUG("   -- Fehler -->  %s\n", ErrText);     // lokale Fehlerausgabe

  digitalWrite (LED_rt,    LED_EIN);
  ErrorFlag = time(0) + BRENNDAUER;             // Steuerung rote LED

  if (errsv == 24)                              // 'too many open files' ...
    report_Error(ErrText, true);                // Fehlermeldung mit Mail ausgeben
  else
    report_Error(ErrText, false);               // Fehlermeldung ohne Mail ausgeben

  return errsv;
}
//***********************************************************************************************

//                       Topic/Payload bearbeiten
//                      ==========================

// Parameter 1: Quelle-der Topic- oder Payload-String ('/.../.../...')
// Parameter 2: enthält bei Übergabe Kommentarstring bzw. Defaultwert
// Parameter 3: pos des '/'-Felds

// Stringwert aus Topic/Payload isolieren ************************
// --------------------------------------
char* getStr(char* Quelle, char* Ziel, int pos)
{
//  DEBUG_1(">> %s-----%s()#%d: %s('%s', '%s', %d)\n",
//                  __NOW__, __FUNCTION__, __LINE__, __FUNCTION__, Quelle, Ziel, pos);
  char Kopie[NOTIZ];
  strcpy(Kopie, Ziel);                                     // soll Bezeichnung enthalten
  if (!split(Quelle, Ziel, pos))
    Ziel = NULL;
//  DEBUG_1(">> %s-----%s()#%d: %s[%d](%s) = \"%s\"\n",
//                   __NOW__, __FUNCTION__, __LINE__, Quelle, pos, Kopie, Ziel);
  return(Ziel);
}

// Integer-Wert aus Topic/Payload isolieren ************************
// ----------------------------------------
int getInt(char* Quelle, char* Ziel, int pos)
{
  DEBUG_1(">> %s-----%s()#%d: %s('%s', '%s', %d)\n",
                  __NOW__, __FUNCTION__, __LINE__, __FUNCTION__, Quelle, Ziel, pos);
  int ret = NOINT;                    // die ermittelte Integer-Zahl
  char Kopie[NOTIZ];
  char Teil[NOTIZ];
  strcpy(Kopie, Ziel);                // soll Bezeichnung enthalten
  if (!split(Quelle, Teil, pos))
    Teil[0] = '\0';
  if (isnumeric(Teil))
    ret = atoi(Teil);
  DEBUG_1(">> %s-----%s()#%d: %s[%d](%s) = '%s' --> %d\n",
                  __NOW__, __FUNCTION__, __LINE__, Quelle, pos, Kopie, Teil, ret);
  return(ret);
}

// Zeit-Wert aus Topic/Payload isolieren ************************
// ----------------------------------------
char* getTim(char* Quelle, char* Ziel, int pos)
{
  DEBUG_1(">> %s-----%s()#%d: %s('%s', '%s', %d)\n",
                  __NOW__, __FUNCTION__, __LINE__, __FUNCTION__, Quelle, Ziel, pos);
  char Kopie[NOTIZ];
  char Teil[NOTIZ];
  strcpy(Kopie, Ziel);                                     // soll Bezeichnung enthalten
  if (!split(Quelle, Teil, pos))
    Ziel = NULL;
  else
  {
    if (isnumeric(Teil))
    {
      time_t Zeit = atol(Teil);
      mkdatum(Zeit, Ziel);
    }
    else
      strcpy(Ziel, "-- invalid --");
  }
  DEBUG_1(">> %s-----%s()#%d: %s[%d](%s) = '%s'\n",
                  __NOW__, __FUNCTION__, __LINE__, Quelle, pos, Kopie, Ziel);
  return(Ziel);
}

// Stringwert in Topic/Payload einbauen ************************
// --------------------------------------
char* setStr(char* Quelle, int pos, char* Neuwert)
{
  DEBUG_1(">> %s-----%s()#%d: %s('%s', %d, '%s')\n",
                  __NOW__, __FUNCTION__, __LINE__, __FUNCTION__, Quelle, pos, Neuwert);
  char* Ziel = Quelle;                          // Zeiger erhalten
  char kp[ZEILE];                               // Hilfsbuffer für Kopie
  char* Kopie = kp;                             // Zeiger drauf
  if (*Ziel == DELIM) Ziel++;                   // führendes '/' überspringen
  strcpy(Kopie, Ziel);                          // Arbeitskopie in den Hilfsbuffer
  for (int ix=0; ix < pos; ix++)
  {
    while (*Ziel != DELIM)                      // bis zur gesuchten Stelle gehen
    {
      Ziel++;
      Kopie++;
    }
    Ziel++;
    Kopie++;
  }
  DEBUG_1("   %s-----%s()#%d: Ziel='%s', Kopie='%s'\n",
                  __NOW__, __FUNCTION__, __LINE__, Ziel, Kopie);
  strcpy(Ziel, Neuwert);                        // den neuen Wert einbauen
  Ziel += strlen(Neuwert);                      // den Zeiger auf das neue Ende
  while (*Kopie != DELIM)                       // alten Wert überspringen ...
  {
    if (strlen(Kopie) == 0) break;              // ... sofern es ihn gibt
    Kopie++;
  }
  if (strlen(Kopie) > 0)
  {
    *Ziel++ = DELIM;                            // Trenner ...
    strcpy(Ziel, ++Kopie);                      // ... und den Rest anfügen
  }
  DEBUG_1(">> %s-----%s()#%d: Quelle = \"%s\"\n",
                   __NOW__, __FUNCTION__, __LINE__, Quelle);
  return(Quelle);
}


// Integer in Topic/Payload einbauen ************************
// --------------------------------------
char* setInt(char* Quelle, int pos, int iNeuwert)
{
  DEBUG_1(">> %s-----%s()#%d: %s('%s', %d, '%d')\n",
                  __NOW__, __FUNCTION__, __LINE__, __FUNCTION__, Quelle, pos, iNeuwert);
  char* Ziel = Quelle;                          // Zeiger erhalten
  char kp[ZEILE];                               // Hilfsbuffer für Kopie
  char* Kopie = kp;                             // Zeiger drauf
  if (*Ziel == DELIM) Ziel++;                   // führendes '/' überspringen
  strcpy(Kopie, Ziel);                          // Arbeitskopie in den Hilfsbuffer

  char it[ZEILE];                               // Hilfsbuffer
  sprintf(it, "%d", iNeuwert);                  // Integer ...
  char* Neuwert = it;                           // ... als String

  for (int ix=0; ix < pos; ix++)
  {
    while (*Ziel != DELIM)                      // bis zur gesuchten Stelle gehen
    {
      Ziel++;
      Kopie++;
    }
    Ziel++;
    Kopie++;
  }
  strcpy(Ziel, Neuwert);                        // den neuen Wert einbauen
  Ziel += strlen(Neuwert);                      // den Zeiger auf das neue Ende
  while (*Kopie != DELIM)                       // alten Wert überspringen ...
  {
    if (strlen(Kopie) == 0) break;              // ... sofern es ihn gibt
    Kopie++;
  }
  if (strlen(Kopie) > 0)
  {
    *Ziel++ = DELIM;                            // Trenner ...
    strcpy(Ziel, ++Kopie);                      // ... und den Rest anfügen
  }
  DEBUG_1(">> %s-----%s()#%d: Quelle = \"%s\"\n",
                   __NOW__, __FUNCTION__, __LINE__, Quelle);
  return(Quelle);
}

// aktuelle Zeit in Topic/Payload einbauen ************************
// --------------------------------------
char* setTim(char* Quelle, int pos)
{
  DEBUG_1(">> %s-----%s()#%d: %s('%s', %d)\n",
                  __NOW__, __FUNCTION__, __LINE__, __FUNCTION__, Quelle, pos);

  char* Ziel = Quelle;                          // Zeiger erhalten
  char kp[ZEILE];                               // Hilfsbuffer für Kopie
  char* Kopie = kp;                             // Zeiger drauf
  if (*Ziel == DELIM) Ziel++;                   // führendes '/' überspringen
  strcpy(Kopie, Ziel);                          // Arbeitskopie in den Hilfsbuffer

  char zt[ZEILE];                               // Hilfsbuffer
  sprintf(zt, "%ld", time(0));                  // aktuelle Zeit ...
  char* Neuwert = zt;                           // ... als String

  for (int ix=0; ix < pos; ix++)
  {
    while (*Ziel != DELIM)                      // bis zur gesuchten Stelle gehen
    {
      Ziel++;
      Kopie++;
    }
    Ziel++;
    Kopie++;
  }
  strcpy(Ziel, Neuwert);                        // den neuen Wert einbauen
  Ziel += strlen(Neuwert);                      // den Zeiger auf das neue Ende
  while (*Kopie != DELIM)                       // alten Wert überspringen ...
  {
    if (strlen(Kopie) == 0) break;              // ... sofern es ihn gibt
    Kopie++;
  }
  if (strlen(Kopie) > 0)
  {
    *Ziel++ = DELIM;                            // Trenner ...
    strcpy(Ziel, ++Kopie);                      // ... und den Rest anfügen
  }
  DEBUG_1(">> %s-----%s()#%d: Quelle = \"%s\"\n",
                   __NOW__, __FUNCTION__, __LINE__, Quelle);
  return(Quelle);
}
//***********************************************************************************************
//***********************************************************************************************

static int sensoren  = 0;             // Anzahl der ermittelten Sensoren

// alle DS18B20-Sensoren einlesen
// -------------------------------
int initds18b20(void)
{
  int sensoren = ds1820_Refresh();
  if (sensoren > 0)
  {
    DEBUG(">> %s---%s()#%d: Anzahl DS18B20-Sensoren: %d\n",
                                              __NOW__, __FUNCTION__, __LINE__, sensoren);
//    { // --- Log-Ausgabe ---------------------------------------------------------
//      char LogText[ZEILE];  sprintf(LogText, "    %d DS18B20-Sensoren:", sensoren);
//      MYLOG(LogText);
//    } // ------------------------------------------------------------------------
    for (int ix=0; ix < sensoren; ix++)
    {
      char SensorName[NOTIZ];
      if( ds1820_Name(ix, SensorName))
      {
        DEBUG(">>  %s---%s()#%d: Sensor(%d) = '%s'\n",
                                              __NOW__, __FUNCTION__, __LINE__, ix, SensorName);
//        { // --- Log-Ausgabe ---------------------------------------------------------
//          char LogText[ZEILE];  sprintf(LogText, "      Sensor(%d) = '%s'", ix, SensorName);
//          MYLOG(LogText);
//        } // ------------------------------------------------------------------------
      }
    }
  }
  return sensoren;
}
//************************************************************

// ds18b20-Sensoren lesen
// ----------------------
int readds18b20(int sensoren, void* mqtt)
{
  char Name[] = "DS1820";            // eindeutiger Name
  float Temperatur[MAXDS1820] = {'\0'};
  char Topic[ZEILE]={'\0'};
  char Payload[ZEILE]={'\0'};
  for (int ix=0; ix < sensoren; ix++)
  {
    Temperatur[ix] = ds1820_Value(ix);
    char myName[NOTIZ];
    sprintf(myName, "%s(%d)", Name, ix);
    DEBUG(">> %s---%s()#%d: Temperatur(%d) = '%.1f'\n",
                                           __NOW__, __FUNCTION__, __LINE__, ix, Temperatur[ix]);
    // -- MQTT --
    sprintf (Topic, MQTT_TOPIC,
               Hostname, atoi(IPnmr), D_DS18B20_00+ix, myName, time(0), MSG_VALUE, INF_REGULAR);
    sprintf (Payload, MQTT_VALUE, Temperatur[ix], "GrdC", "--OK");
    DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                               __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
    MQTT_Publish(mqtt, Topic, Payload);
  }

  return 0;
}
//***********************************************************************************************

// auf eine MQTT-Message antworten
// -------------------------------
// mit Log-Möglichkeit
int ReplyMessage(struct MqttInfo* info, char* topic, char* payload)
{
#ifdef __LAMPMOTION_MYLOG__
  { // --- Log-Ausgabe ----------------------------------------------------------------
    // zum Debuggen Klartext einfügen
    char Messagetyp[ZEILE] = "";
    char Messageinfo[ZEILE] = "";
    char Topic[ZEILE];
    char LogText[ZEILE];

    strcpy(Topic, topic);
    DEBUG(">> %s---%s()#%d: Topic = '%s'\n", __NOW__, __FUNCTION__, __LINE__, Topic);
    if (matchn(Topic, TPOS_TYP, MSG_ERROR))
      sprintf(Messagetyp, "%5d(\"%s\")", MSG_ERROR, "Fehlermeldung");
    else if (matchn(Topic, TPOS_TYP, MSG_VALUE))
      sprintf(Messagetyp, "%5d(\"%s\")", MSG_VALUE, "Messwert");
    DEBUG("   %s   %s()#%d: Messagetyp = '%s'\n", __NOW__, __FUNCTION__, __LINE__, Messagetyp);

    if (matchn(Topic, TPOS_INF, INF_FEHLERHAFT))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_FEHLERHAFT, "Message war fehlerhaft");
    else if (matchn(Topic, TPOS_INF, INF_NOSUPPORT))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_NOSUPPORT, "wird nicht unterstuetzt");
    else if (matchn(Topic, TPOS_INF, INF_ACK))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_ACK, "positive Quittung");
    else if (matchn(Topic, TPOS_INF, INF_NAK))
      sprintf(Messageinfo, "%5d(\"%s\")", INF_NAK, "negative Quittung");
    DEBUG("   %s   %s()#%d: Messageinfo = '%s'\n", __NOW__, __FUNCTION__, __LINE__, Messageinfo);
    if (strlen(Messagetyp) > 0)
      setStr(Topic, TPOS_TYP, Messagetyp);
    if (strlen(Messageinfo) > 0)
      setStr(Topic, TPOS_INF, Messageinfo);
    sprintf(LogText, " Topic: \"%s\" -- Payload: \"%s\"", Topic, payload);
    DEBUG(">> %s---%s()#%d: Topic = '%s' - Payload: \"%s\"\n",
                                        __NOW__, __FUNCTION__, __LINE__, Topic, payload);
    MYLOG(LogText);
  } // ---------------------------------------------------------------------------------
#endif
  return(MQTT_Publish(info, topic, payload));
}
//***********************************************************************************************


// interne CPU-Temperatur lesen
// ---------------------------------------------------------------------
int readCPUtemp(void* mqtt)
{
  char myName[] = "INTERN";            // eindeutiger Name
  float Temperatur = InternalTemperatur();
  char Topic[ZEILE]={'\0'};
  char Payload[ZEILE]={'\0'};
  DEBUG(">> %s---%s()#%d: CPU-Temperatur = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Temperatur);
  // -- MQTT --
  sprintf (Topic, MQTT_TOPIC,
                    Hostname, atoi(IPnmr), D_INTERN, myName, time(0), MSG_VALUE, INF_REGULAR);
  sprintf (Payload, MQTT_VALUE, Temperatur, "GrdC", "OK");
  DEBUG(">> %s---%s()#%d:  Topic = \"%s  \" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  MQTT_Publish(mqtt, Topic, Payload);

  return 0;
}
//***********************************************************************************************

static bool bme280 = false;

// BME280-Sensor (Temperatur, Luftdruck, Feuchtigkeit) lesen und senden
// ---------------------------------------------------------------------
int readbme280(void* mqtt)
{
  float Temperatur  = '\0';
  float Luftdruck   = '\0';
  float Feuchtigkeit = '\0';
  char Topic[ZEILE]={'\0'};
  char Payload[ZEILE]={'\0'};
  if (bme280_Read(&Luftdruck, &Temperatur, &Feuchtigkeit))
  {
    DEBUG(">> %s---%s()#%d: BME280-Temperatur = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Temperatur);
    DEBUG(">> %s---%s()#%d: BME280-Luftdruck = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Luftdruck);
    DEBUG(">> %s---%s()#%d: BME280-Feuchtigkeit = '%.1f'\n",
                                              __NOW__, __FUNCTION__, __LINE__, Feuchtigkeit);
  }
  // -- MQTT --
  {
    char myName[] = "BME280_TEMP";             // eindeutiger Name
    sprintf (Topic, MQTT_TOPIC,
                    Hostname, atoi(IPnmr), D_BME280_T, myName, time(0), MSG_VALUE, INF_REGULAR);
    sprintf (Payload, MQTT_VALUE, Temperatur, "GrdC", "--OK");
    DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"<\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
    MQTT_Publish(mqtt, Topic, Payload);
  }
  {
    char myName[] = "BME280_PRESS";            // eindeutiger Name
    sprintf (Topic, MQTT_TOPIC,
                    Hostname, atoi(IPnmr), D_BME280_P, myName, time(0), MSG_VALUE, INF_REGULAR);
    sprintf (Payload, MQTT_VALUE, Luftdruck, "hPa", "--OK");
    DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"<\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
    MQTT_Publish(mqtt, Topic, Payload);
  }
  {
    char myName[] = "BME280_HUMI";             // eindeutiger Name
    sprintf (Topic, MQTT_TOPIC,
                    Hostname, atoi(IPnmr), D_BME280_H, myName, time(0), MSG_VALUE, INF_REGULAR);
    sprintf (Payload, MQTT_VALUE, Feuchtigkeit, "%", "--OK");
    DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"<\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
    MQTT_Publish(mqtt, Topic, Payload);
  }

  return 0;
}
//***********************************************************************************************

static void* tsl = NULL;              // das TLS2561-Objekt !

// Initialisierung des TLS2561-Sensors
// -----------------------------------
void* inittsl2561(void* tsl)
{
  tsl = tsl2561_init();
  tsl2561_set_integration_time(tsl, TSL2561_INTEGRATION_TIME_13MS);
  DEBUG(">> %s---%s()#%d   - tsl = '%p'\n", __NOW__, __FUNCTION__, __LINE__, tsl);
  if (tsl != NULL)
    DEBUG(">> %s---%s()#%d: Initialisierung TLS2561 OK\n", __NOW__, __FUNCTION__, __LINE__);

  return tsl;
}
//************************************************************

// tsl2561-Sensor (Helligkeit) lesen und senden
// --------------------------------------------
int readtsl2561(void* tsl, void* mqtt)
{
  char myName[] = "TLS2561";             // eindeutiger Name
  char Topic[ZEILE]={'\0'};
  char Payload[ZEILE]={'\0'};
  float Lux = 1.0 * tsl2561_lux(tsl);
  DEBUG(">> %s---%s()#%d: Helligkeit = '%f'\n", __NOW__, __FUNCTION__, __LINE__, Lux);
  // -- MQTT --
  sprintf (Topic, MQTT_TOPIC,
                    Hostname, atoi(IPnmr), D_TLS2561, myName, time(0), MSG_VALUE, INF_REGULAR);
  sprintf (Payload, MQTT_VALUE, Lux, "Lux", "OK");
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  MQTT_Publish(mqtt, Topic, Payload);

  return true;        // Sendeauftrag zurückgeben
}
//***********************************************************************************************

// angeforderte Quittung senden
// ------------------------------------------
int ReqAcknowledge(char* Topic, char* Payload)
{
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  //  Topic:
  //  0.Raspi-Name (erforderlich)       - neu
  //  1.IP-letzte Stelle (erforderlich) - neu
  //  2.Sensor-ID (erforderlich)        - alt
  //  3.Sensor-Name (oder "-")          - alt
  //  4.Datum/Uhrzeit - als timestamp   - neu
  //  5.Messagetyp (Art der Message)    - alt
  //  6.Messageart (Textteil Message)   - neu

  setStr(Topic, TPOS_RAS, Hostname);            // eigener Name
  setInt(Topic, TPOS_IPA, atoi(IPnmr));         // eigene IP
  setTim(Topic, TPOS_TIM);                      // aktuelle Zeit
  setInt(Topic, TPOS_TYP, MSG_COMMAND);         // Status
  setInt(Topic, TPOS_INF, INF_ACK);             // Antwort auf Anfrage

  //  Payload: (wenn Topic[5] = MSG_COMMAND)
  //  0.Steuerbefehl   - neu
  //  1.Schaltwert     - alt
  //  2.Analogwert     - alt
  //  3.Anzeigewert    - alt

  setStr(Payload, PPOS_STT, "Status");            // eigener Name

  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  return RESP_QUITTUNG;
}
//***********************************************************************************************

// Anforderung wird nicht unterstützt
// ------------------------------------------
int ReqNotSupported(char* Topic, char* Payload)
{
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  //  Topic:
  //  0.Raspi-Name (erforderlich)       - neu
  //  1.IP-letzte Stelle (erforderlich) - neu
  //  2.Sensor-ID (erforderlich)        - alt
  //  3.Sensor-Name (oder "-")          - alt
  //  4.Datum/Uhrzeit - als timestamp   - neu
  //  5.Messagetyp (Art der Message)    - neu
  //  6.Messageart (Textteil Message)   - neu

  setStr(Topic, TPOS_RAS, Hostname);            // eigener Name
  setInt(Topic, TPOS_IPA, atoi(IPnmr));         // eigene IP
  setTim(Topic, TPOS_TIM);                      // aktuelle Zeit
  setInt(Topic, TPOS_TYP, MSG_ERROR);           // Status
  setInt(Topic, TPOS_INF, INF_NOSUPPORT);       // Antwort auf Anfrage

  //  Payload: (wenn Topic[5] = MSG_COMMAND)
  //  0.Steuerbefehl   - alt
  //  1.Schaltwert     - alt
  //  2.Analogwert     - alt
  //  3.Anzeigewert    - alt

  DEBUG(">> %s---%s()#%d ->  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  return RESP_NOSUPPORT;
}
//***********************************************************************************************

// Anforderung war fehlerhaft
// ------------------------------------------
int ReqFehlerhaft(char* Topic, char* Payload, char* Fehlertext)
{
  DEBUG(">> %s---%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  //  Topic:
  //  0.Raspi-Name (erforderlich)       - neu
  //  1.IP-letzte Stelle (erforderlich) - neu
  //  2.Sensor-ID (erforderlich)        - alt
  //  3.Sensor-Name (oder "-")          - alt
  //  4.Datum/Uhrzeit - als timestamp   - neu
  //  5.Messagetyp (Art der Message)    - neu
  //  6.Messageart (Textteil Message)   - neu

  setStr(Topic, TPOS_RAS, Hostname);            // eigener Name
  setInt(Topic, TPOS_IPA, atoi(IPnmr));         // eigene IP
  setTim(Topic, TPOS_TIM);                      // aktuelle Zeit
  setInt(Topic, TPOS_TYP, MSG_ERROR);           // Status
  setInt(Topic, TPOS_INF, INF_FEHLERHAFT);      // war fehlerhaft

  //  Payload: (wenn Topic[5] = MSG_COMMAND)
  //  0.Steuerbefehl   - alt
  //  1.Schaltwert     - alt
  //  2.Analogwert     - alt
  //  3.Anzeigewert    - alt

  sprintf (Payload, MQTT_STATE, Fehlertext);

  DEBUG(">> %s---%s()#%d ->  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  return RESP_NOSUPPORT;
}
//***********************************************************************************************

// Bearbeitung der MODE_COMMAND-Message
// ------------------------------------
// Topic enthält einen Schaltbefehl
int CommandMessage(char* Topic, char* Payload)
{
  DEBUG(">> %s--%s()#%d: Topic: \"%s\" -- Payload: \"%s\"\n",
                                   __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
  int response = RESP_KEINEANTWORT;

  // Auswertung von Topic
  // --------------------
  char CompName[NOTIZ] = "Sendername";                    // Name des Senders
  int CompIP = 0;                                         // letzte Stelle IP des Senders
  int DeviceID = 0;                                       // Device-(Sensor-)-ID
  char DeviceName[NOTIZ] = "Devicename";                  // Name des Geräts (Sensor, Schalter,...)
  char MsgZeit[NOTIZ] = "Sendezeitpunkt";                 // Zeitpunkt des Sendens
  char MsgInfo[NOTIZ] = "---123456789";                   // Zusatzinformation

  getStr(Topic, CompName, TPOS_RAS);                      // Name des Senders
  CompIP = getInt(Topic, "IP-Nummer", TPOS_IPA);          // letzte Stelle IP des Senders
  DeviceID = getInt(Topic, "Sensor-ID", TPOS_SID);        // Device-(Sensor-)-ID
  getStr(Topic, DeviceName, TPOS_NAM);                    // Name des Sensors
  getTim(Topic, MsgZeit, TPOS_TIM);                       // Zeitpunkt des Sendens

  bool AckRequest = false;
  if (partn(Topic, TPOS_INF, MsgInfo))                    // wenn es Zusatzinfos gibt ...
  {
    // ... ist es eine Quittungsanforderung ?
    char Inf_ReqfAck[NOTIZ];
    sprintf(Inf_ReqfAck, "%d", INF_REQFACK);
    AckRequest = (bool)strstr( MsgInfo, Inf_ReqfAck );
    DEBUG(">> %s---%s()#%d ->  MsgInfo='%s' -- AckRequest: %i\n",
                                 __NOW__, __FUNCTION__, __LINE__, MsgInfo, (int)AckRequest);
  }

  // Auswertung von Payload
  // ----------------------
  char Steuerbefehl[NOTIZ] = "---";                       // Analogwert (oder '-')
  char Analogwert[NOTIZ] = "Analogwert";                  // Analogwert (oder '-')
  char Anzeigewert[NOTIZ] = "Anzeigewert";                // Anzeigewert (oder "")

  Ctrl Command = Str2Ctrl( toUpper( getStr(Payload, Steuerbefehl, PPOS_CMD)));
  getStr(Payload, Analogwert, PPOS_ANA);                  // Analogwert (oder '---')
  getStr(Payload, Anzeigewert, PPOS_DIS);                 // Anzeigewert (oder "")


  if (Command == SCHALTEN)
  {
    // Schaltwert in Einheitsform
    // --------------------------
    int Schalter;
    char Schaltwert[NOTIZ] = "Schaltwert";
    Ctrl SValue = Str2Ctrl( toUpper( getStr(Payload, Schaltwert, PPOS_VAL)));
    switch ( SValue)
    {
      case EIN:
      case HELL:
        Schalter = LED_HELL;
        break;
      case AUS:
      case DUNKEL:
        Schalter = LED_DUNKEL;
        break;
      case AUTO:
        Schalter = LED_HELL;
        break;
      default:
        DeviceID = INF_FEHLERHAFT;
        break;
    }

    // welches Gerät?
    // --------------
    switch (DeviceID)
    {
      case D_IRLAMPE_RECHTS:
//        digitalWrite (LAMP_IRRIGHT, Schalter);
        Automatic = false;
        DEBUG(">> %s---%s()#%d: LAMP_IRRIGHT = %s(%d); AckRequest=%d\n",
                         __NOW__, __FUNCTION__, __LINE__, Schaltwert, Schalter, (int)AckRequest);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    LAMP_IRRIGHT = %s(%d) !", Schaltwert, Schalter );
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        if (AckRequest)                       // Bitte um Quittung
        { // Quittung senden
          response = ReqAcknowledge( Topic, Payload);
        }
        break;

      case D_IRLAMPE_LINKS:
//        digitalWrite (LAMP_IRLEFT, Schalter);
        Automatic = false;
        DEBUG(">> %s---%s()#%d: LAMP_IRLEFT = %s(%d); AckRequest=%d\n",
                         __NOW__, __FUNCTION__, __LINE__, Schaltwert, Schalter, (int)AckRequest);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    LAMP_IRLEFT = %s(%d) !", Schaltwert, Schalter );
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        if (AckRequest)                       // Bitte um Quittung
        { // Quittung senden
          response = ReqAcknowledge( Topic, Payload);
        }
        break;

      case D_IRLAMPEN: /* beide betreffend */
        Automatic = true;
        DEBUG(">> %s---%s()#%d: Automatic = %d; AckRequest=%d\n",
                         __NOW__, __FUNCTION__, __LINE__, Automatic, (int)AckRequest);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    Automatic = %d !", Automatic );
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        if (AckRequest)                       // Bitte um Quittung
        { // Quittung senden
          response = ReqAcknowledge( Topic, Payload);
        }
        break;

      default:                  // alles andere
        response = RESP_NOSUPPORT;
        break;
    } // --- ende switch ---
  } // ---- ende Command == CMD_SCHALTEN
  else
    // Befehl nicht unterstützt
    // ------------------------
    response = RESP_NOSUPPORT;



  // um 'not used'-Warnings zu vermeiden:
  // ------------------------------------
  {
    UNUSED(DeviceID);
    UNUSED(CompIP);
    UNUSED(CompName);
    UNUSED(DeviceName);
    UNUSED(MsgZeit);
    UNUSED(Command);
    UNUSED(Analogwert);
    UNUSED(MsgZeit);
//    UNUSED(MsgInfo);
  }

  DEBUG(">> %s---%s()#%d: response = %d\n", __NOW__, __FUNCTION__, __LINE__, response);
  return response;
}
//***********************************************************************************************
//***********************************************************************************************
//**                                                                                           **
//**                                  main()                                                   **
//**                                                                                           **
//***********************************************************************************************
//***********************************************************************************************

int main(int argc, char *argv[])
{
  sprintf (Version, "Vers. %d.%d.%d - %s", MAXVERS, MINVERS, BUILD, __DATE__);
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 ); // Verbindung zum Dämon Syslog aufbauen
  syslog(LOG_NOTICE, ">>>>> %s - %s - PID %d - User %d/%d, Group %d/%d/ <<<<<<",
                          PROGNAME, Version, getpid(), geteuid(), getuid(), getegid(), getgid());

  // Dieses Programm
  // ----------------
  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];
    sprintf(LogText, "'%s %s' - User %d/%d, Group %d/%d\n",
                       argv[0], Version, geteuid(),getuid(), getegid(),getgid());
    printf(LogText);
    sprintf(LogText, ">>> %s - PID %d, User %d/%d, Group %d/%d",
                      Version, getpid(), geteuid(),getuid(), getegid(),getgid());
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // schon mal den Watchdog füttern
  // ------------------------------
  feedWatchdog(PROGNAME);

  // Host ermitteln
  // ---------------
  {
    char LogText[ZEILE];
    int status = gethostname(Hostname, NOTIZ);
    if (status < 0)
    { // -- Error
      char ErrText[ERRBUFLEN];
      sprintf(ErrText, "gethostname '%s'", Hostname);
      return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
    }
    DEBUG(">> %s-%s()#%d: Hostname: '%s'\n", __NOW__, __FUNCTION__, __LINE__, Hostname);
    sprintf(LogText,"    Hostname: '%s'", Hostname);
    MYLOG(LogText);
  }

  // Prozess-ID ablegen
  // ------------------
  {
    char LogText[ZEILE];
    long pid = savePID(FPID);
    DEBUG(">> %s-%s()#%d: meine PID: '%ld'\n", __NOW__, __FUNCTION__, __LINE__, pid);
    sprintf(LogText,"    meine PID = '%ld'", pid);
    MYLOG(LogText);
  }

  // IP-Adresse ermitteln
  // ----------------------
  // nur das letzte Glied wird gebraucht
  {
    char LogText[ZEILE];
    readIP(meineIPAdr, sizeof(meineIPAdr));
    sprintf(LogText,"    meine IPs: '%s'", meineIPAdr);
    MYLOG(LogText);
    DEBUG(">> %s-%s()#%d: meine ganze IP: '%s'\n",
                                     __NOW__, __FUNCTION__, __LINE__,  meineIPAdr);

    char* ptr = strtok(meineIPAdr, ".");
    while (ptr != NULL)
    {
      IPnmr = ptr;
      ptr = strtok(NULL, ".");
    }
    DEBUG(">> %s-%s()#%d: meine IP: '%s'\n", __NOW__, __FUNCTION__, __LINE__, IPnmr);
  }

  // Ist GPIO klar?
  // --------------
  {
    wiringPiSetup();
    pinMode (LED_gn2,   OUTPUT);
    pinMode (LED_bl2,    OUTPUT);    pinMode (LAMP_IRRIGHT, OUTPUT);
    pinMode (LAMP_IRLEFT,  OUTPUT);
    #define ANZEIT  44 /* msec */
    for (int ix=0; ix < 96; ix++)
    {
      digitalWrite (LED_gn2,   LED_EIN);
      digitalWrite (LAMP_IRRIGHT, LED_HELL);
      delay(ANZEIT);
      digitalWrite (LED_gn2,   LED_AUS);
      digitalWrite (LAMP_IRRIGHT, LED_DUNKEL);
      digitalWrite (LED_bl2,    LED_EIN);
      digitalWrite (LAMP_IRLEFT,  LED_HELL);
      delay(ANZEIT);
      digitalWrite (LED_bl2,    LED_AUS);
      digitalWrite (LAMP_IRLEFT,  LED_DUNKEL);
    }
    digitalWrite (LAMP_IRRIGHT,   LED_DUNKEL);
    digitalWrite (LAMP_IRLEFT,    LED_DUNKEL);

    DEBUG(">> %s()#%d @ %s ----- GPIO OK -------\n", __FUNCTION__, __LINE__, __NOW__);
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "    GPIO OK !");
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
  }

  // alle DS18B20-Sensoren einlesen
  // -------------------------------
  sensoren = initds18b20();
  if (sensoren > 0)
  {
    DEBUG(">> %s-%s()#%d: Anzahl DS18B20-Sensoren: %d\n",
                                              __NOW__, __FUNCTION__, __LINE__, sensoren);
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "    %d DS18B20-Sensoren:", sensoren);
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
    for (int ix=0; ix < sensoren; ix++)
    {
      char SensorName[NOTIZ];
      if( ds1820_Name(ix, SensorName))
      {
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "      Sensor(%d) = '%s'", ix, SensorName);
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
      }
    }
  }


  // Initialisierung des BME280-Sensors
  // -----------------------------------
  bme280 = bme280_Init();
  if ( bme280)
  {
    DEBUG(">> %s-%s()#%d: Initialisierung BME280 OK\n",
                                              __NOW__, __FUNCTION__, __LINE__);
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "    Sensor BME280 OK !");
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
  }


  // Initialisierung des TLS2561-Sensors
  // -----------------------------------
  tsl = inittsl2561(tsl);
  if (tsl != NULL)
  {
    DEBUG(">> %s-%s()#%d: Initialisierung TLS2561 OK\n",
                                              __NOW__, __FUNCTION__, __LINE__);
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "    Sensor TLS2561 OK !");
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
  }


  digitalWrite (LED_bl2, LED_EIN);

  // MQTT starten
  // --------------
  char mySubscriptions[3*ZEILE]={'\0'};     // zum Ausdruck per Mail
  struct MqttInfo* mqtt = NULL;             // das MQTT-Objekt
  {
    char Topic[ZEILE]={'\0'};
    char Payload[ZEILE]={'\0'};
    time_t kleinePause = time(NULL) + 2;
    while (kleinePause > time(NULL));
    mqtt = MQTT_Start();              // MQTT wird initiert und die Callbacks aktiviert
     DEBUG(">> %s-%s()#%d   - mqtt = '%p'\n", __NOW__, __FUNCTION__, __LINE__, mqtt);
     DEBUG(">> %s-%s()#%d: Initialisierung MQTT Vers.'%s' OK\n",
                                              __NOW__, __FUNCTION__, __LINE__, MQTT_Version());
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "    MQTT Vers.'%s' OK !", MQTT_Version());
      MYLOG(LogText);
    } // ------------------------------------------------------------------------

    // Start-Meldung über MQTT ausgeben
    // ----------------------------------
    sprintf (Topic, MQTT_TOPIC, Hostname, atoi(IPnmr), D_MQTT, PROGNAME, time(0), MSG_STATE, INF_REGULAR);
    sprintf (Payload, MQTT_STATE, "START");
     DEBUG(">> %s-%s()#%d:  Topic = \"%s\" --- Payload = \"%s\"\n",
                                              __NOW__, __FUNCTION__, __LINE__, Topic, Payload);
    MQTT_Publish(mqtt, Topic, Payload);

    // Subscriptionen anmelden
    // ------------------------
    char Zeile[ZEILE];
    char Subscript[ZEILE];
    sprintf( Subscript, SUBSCR_TYP, MSG_ADMIN);           // Verwaltungsinfos abonnieren
    int mStatus = MQTT_Subscribe(mqtt, Subscript);
     DEBUG(">> %s-%s()#%d: Subscription(\"%s\")=%d \n",
                                              __NOW__, __FUNCTION__, __LINE__, Subscript, mStatus);
    sprintf(Zeile, "       -- Subscription MSG_ADMIN   = '%s'\n", Subscript);
    strcat(mySubscriptions, Zeile);

    sprintf( Subscript, SUBSCR_TYP, MSG_COMMAND);         // Schaltbefehle abonnieren
    mStatus = MQTT_Subscribe(mqtt, Subscript);
     DEBUG(">> %s-%s()#%d: Subscription(\"%s\")=%d \n",
                                              __NOW__, __FUNCTION__, __LINE__, Subscript, mStatus);
    sprintf(Zeile, "       -- Subscription MSG_COMMAND = '%s'\n", Subscript);
    strcat(mySubscriptions, Zeile);

    destroyInt(mStatus);
  }


  { // Bereitmeldung per Mail ---------------------------------------
    // -----------------------
    char Betreff[ZEILE] = {'\0'};
    char Zeitbuf[NOTIZ];
    sprintf( Betreff, "Start >%s< @ %s",  PROGNAME, mkdatum(time(0), Zeitbuf));
    DEBUG( "Betreff: %s\n", Betreff);

    char MailBody[BODYLEN] = {'\0'};
    char Zeile[ZEILE];
    char Buf[NOTIZ];
    char* Path=NULL;
    sprintf(Zeile,"Programm '%s/%s' %s\n", getcwd(Path, ZEILE), PROGNAME, Version);
    strcat(MailBody, Zeile);
    sprintf(Zeile,"RaspBerry Pi  No. %s - '%s' -- IP-Adresse '%s'\n",
               readRaspiID(Buf), Hostname, readIP(meineIPAdr, sizeof(meineIPAdr)));
    strcat(MailBody, Zeile);

    if (sensoren == 0)
    {
      sprintf(Zeile,"      keine DS18B20-Sensoren!\n");
      strcat(MailBody, Zeile);
    }
    else
    {
      sprintf(Zeile,"      %d DS18B20-Sensoren:\n", sensoren);
      strcat(MailBody, Zeile);
      for (int ix=0; ix < sensoren; ix++)
      {
        char SensorName[NOTIZ];
        ds1820_Name(ix, SensorName);
        sprintf(Zeile,"       -- Sensor(%d): '%s'\n", ix, SensorName);
        strcat(MailBody, Zeile);
      }
    }

    if (bme280)
    {
      sprintf(Zeile,"      BME280-Sensor\n");
      strcat(MailBody, Zeile);
    }

    if (tsl != NULL)
    {
      sprintf(Zeile,"      TSL2561-Sensor (Helligkeit)\n");
      strcat(MailBody, Zeile);
    }
    sprintf(Zeile,"      MQTT aktiv und Callbacks aktiviert\n");
    strcat(MailBody, Zeile);

    strcat(MailBody, mySubscriptions);

    DEBUG( "MailBody:\n%s\n", MailBody);
//    sendmail(Betreff, MailBody);
  } // -----  Bereitmeldung per Mail -----------------------------------

  syslog(LOG_NOTICE, "--- Init done ---");

  digitalWrite (LED_bl2, LED_AUS);
  digitalWrite (LED_gn2, LED_EIN);
//  digitalWrite (LAMP_IRRIGHT, LED_HELL);
//  digitalWrite (LAMP_IRLEFT,  LED_HELL);
  Automatic = true;

  DEBUG("\n");
  time_t AbfrageStart = time(0);
  DO_FOREVER // *********************** Endlosschleife **************************************
             // *****************************************************************************
  {
    feedWatchdog(PROGNAME);

    // *+++++* Sensoren bedienen **+++++*
    // -----------------------------------
    static bool done=false;
    static bool ldone=false;
    static int lcnt = 5;
    long Zeitpunkt = (time(0)-AbfrageStart) % SENDETAKT;
    switch (Zeitpunkt)
    {
      case SENDETAKT/5*0:
        if (!done)
          DEBUG("\n");
        done=true;
        break;

      case SENDETAKT/5*1:     // ds18b20-Sensoren lesen
        if ((sensoren > 0) && !done)
          readds18b20(sensoren, mqtt);
        done=true;
        break;

      case SENDETAKT/5*2:     // interne CPU-Temperatur lesen
        if (!done)
          readCPUtemp(mqtt);
        done=true;
        break;

      case SENDETAKT/5*3:     // BME280-Sensor lesen
        if ((bme280) && !done)
          readbme280(mqtt);
        done=true;
        break;

      case SENDETAKT/5*4:     // tsl2561-Sensor (Helligkeit) lesen
        if ((tsl != NULL) && !done)
          readtsl2561(tsl, mqtt);
        done=true;
        break;

      default:                // nichts oder sonstwas
        done = false;
        break;
    } // --- end switch ---


    // *+++++* MQTT bedienen **+++++*
    // ------------------------------
    static bool msg=false;
    static bool lmsg=false;
    {
      char Topic[ZEILE]={'\0'};
      char Payload[ZEILE]={'\0'};
      int antwort = RESP_KEINEANTWORT;

      // empfangene Messages holen
      // -------------------------
      msg=false;
      enum TOPICMODE Status = MQTT_Loop(mqtt, Topic, Payload);

      // Test auf eigene Message
      // -----------------------
      int CompIP = getInt(Topic, "IP-Nummer", TPOS_IPA);  // letzte Stelle IP des Senders
      if (CompIP == atoi(IPnmr))
      { // überspringen
        // -------------
        DEBUG("\n>> %s-%s()#%d - neue Message: CompID '%d' == IPnmr '%s'!\n\n",
                                   __NOW__, __FUNCTION__, __LINE__, CompIP, IPnmr);
        Status = MODE_NOTHING;
      }

      if (Status > MODE_NOTHING)
      { // eine Message ist eingetroffen !!
        // =======================================================================
        DEBUG("\n>> %s-%s()#%d: Status: %d -- Topic: \"%s\" -- Payload: \"%s\"\n",
                                   __NOW__, __FUNCTION__, __LINE__, Status, Topic, Payload);

#ifdef __LAMPMOTION_MYLOG__
        { // --- Log-Ausgabe ----------------------------------------------------------------
          // zum Debuggen Klartext einfügen
          char LogText[ZEILE];
          char Messagetyp[NOTIZ] = "";
          char Messageinfo[NOTIZ] = "";
          switch (Status)
          {
            case MODE_ADMIN:    // Verwaltung
              sprintf(Messagetyp, "%5d(\"%s\")", MSG_ADMIN,   "Verwaltung");
              break;

            case MODE_COMMAND:  // Schaltbefehl
              sprintf(Messagetyp, "%5d(\"%s\")", MSG_COMMAND, "Schaltbefehl");
              if (matchn(Topic, TPOS_INF, INF_REQFACK))
                sprintf(Messageinfo, "%5d(\"%s\")", INF_REQFACK, "Bitte um Quittung");
              break;

            default:            // sonstwas
              antwort = RESP_NOSUPPORT;
              break;

          } // --- end switch ---
          if (strlen(Messagetyp) > 0)
            setStr(Topic, TPOS_TYP, Messagetyp);
          if (strlen(Messageinfo) > 0)
            setStr(Topic, TPOS_INF, Messageinfo);
          sprintf(LogText, "%d - Topic: \"%s\" - Payload: \"%s\"", Status, Topic, Payload);
          MYLOG(LogText);
        } // ---------------------------------------------------------------------------------
#endif

        // in 'Status' steht, wie es weitergeht
        // ------------------------------------
        // die übermittelten Infos sind in Topic und Payload

        switch (Status)
        {
          case MODE_ADMIN:    // Verwaltung
            antwort = RESP_NOSUPPORT;
            break;

          case MODE_COMMAND:  // Schaltbefehl
            antwort = CommandMessage(Topic, Payload);
            break;

          default:            // sonstwas
            antwort = RESP_NOSUPPORT;
            break;
        } // --- end switch ---
        msg=true;
      }

//      DEBUG("\n>> %s-%s()#%d: antwort = %d\n",
//                                   __NOW__, __FUNCTION__, __LINE__, antwort);

      switch (antwort)
      { // ggf. die empfangene Message beantworten
        // ---------------------------------------
        case RESP_QUITTUNG:
          // die Message ist schon fertig zusammengestellt
          // ---------------------------------------------
          break;

        case RESP_NOSUPPORT:
          // Die Anforderung wird nicht unterstützt
          // --------------------------------------
          ReqNotSupported(Topic, Payload);
          ReplyMessage(mqtt, Topic, Payload);
          antwort = RESP_KEINEANTWORT;
          break;

        case RESP_FEHLER:
          // die Anforderung enthält Fehler
          // ------------------------------
          // weitere Info ggf. in Payload
          ReqFehlerhaft(Topic, Payload, "das war nix");
          ReplyMessage(mqtt, Topic, Payload);
          antwort = RESP_KEINEANTWORT;
          break;

        default:                  // alles andere
//          DEBUG("\n>> %s-%s()#%d: antwort = default\n",
//                                   __NOW__, __FUNCTION__, __LINE__);
          break;
      } // --- end switch ---

      if ( antwort != RESP_KEINEANTWORT)
        ReplyMessage(mqtt, Topic, Payload);

    }   // -- end MQTT


    // LED-Steuerung
    // -------------
    if (done != ldone)
    {
      ldone = done;
      if (done)
        lcnt = 3;
    }
    if (msg != lmsg)
    {
      lmsg = msg;
      if (msg)
        lcnt = 3;
    }
    digitalWrite (LED_bl2, (--lcnt > 0) ? LED_EIN : LED_AUS);
    static int blink = (STD*4)-20;
    blink++;
    if (blink % 5 == 0)
    {
      digitalWrite (LED_gn2, LED_AUS);
      delay(100);
      digitalWrite (LED_gn2, LED_EIN);
    }
    
    if (!Automatic)
    { // Handsteuerung
      // -------------      
    }
    
    else if (Zeitfenster(GUTENMORGEN, GUTENACHT))
    { // Normalbetrieb (Automatic!)
      // --------------------------
      DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
//      digitalWrite (LAMP_IRRIGHT, LED_HELL);
//      digitalWrite (LAMP_IRLEFT,  LED_HELL);
//      if (((blink % (STD*4)) == 0))     // entspricht ca. 1:07 Stunden
        
      time_t tnow;
      time(&tnow);
      if ((tnow % STD) == 0)              // zur vollen Stunde
      
        
      { // mit IR-Lampen Photo auslösen
        // ----------------------------
        DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
//        digitalWrite (LAMP_IRRIGHT, LED_DUNKEL);
//        digitalWrite (LAMP_IRLEFT,  LED_DUNKEL);
        { // --- Log-Ausgabe ---------------------------------------------------------
          char LogText[ZEILE];  sprintf(LogText, "    --- Probeaufnahme: %ld mod %d = %ld", tnow, STD, (tnow % STD));
          MYLOG(LogText);
        } // ------------------------------------------------------------------------
        delay(10*SEC);
//        digitalWrite (LAMP_IRRIGHT, LED_HELL);
//        digitalWrite (LAMP_IRLEFT,  LED_HELL);
        DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
      }
    }
    else
    { // Nachts ist Ruhe!
      // -----------------
      DEBUG_2(">> %s-%s()#%d\n",  __NOW__, __FUNCTION__, __LINE__);
//      digitalWrite (LAMP_IRRIGHT, LED_DUNKEL);
//      digitalWrite (LAMP_IRLEFT,  LED_DUNKEL);
    }

    if (ErrorFlag > 0)
    { // rote LED wieder ausschalten
      // ---------------------------
      if (time(0) > ErrorFlag)
      { // wenn Zeit abgelaufen
        // --------------------
        ErrorFlag = 0;
        digitalWrite (LED_rt, LED_AUS);
      }
    }

    delay(ABFRAGETAKT);

  } // ===========================================================================================

//  tsl2561_close(tsl);
//
//  // Fehler-Mail abschicken
//  // ----------------------
//  char MailBody[BODYLEN] = {'\0'};
//  char Logtext[ZEILE];
//  digitalWrite (LED_rt, LED_EIN);
//  sprintf(Logtext, ">> %s()#%d: Error %s ---> '%s' OK\n",__FUNCTION__, __LINE__, PROGNAME, "lastItem");
//  syslog(LOG_NOTICE, "%s: %s", __LAMP__, Logtext);
//
//  strcat(MailBody, Logtext);
//  char Betreff[ERRBUFLEN];
//  DEBUG(Betreff, "Error-Message von %s: >>%s<<", PROGNAME, "lastItem");
//  sendmail(Betreff, MailBody);                  // Mail-Message absetzen
}
//************************************************************************************************
