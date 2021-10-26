//***************************************************************************//
//*                                                                         *//
//* File:          ds1820.c                                                 *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2021-04-06;                                              *//
//* Last change:   2021-09-10 - 13:49:15: neuer Ansatz                      *//
//* Description:   Treiber für Temperatur-Sensor DS18B20                    *//
//*                Treiber für CPU-Temperatur                               *//
//*                                                                         *//
//* Copyright (C) 2021 by Wolfgang Keuch                                    *//
//*                                                                         *//
//*                                                                         *//
//***************************************************************************//

#define __DS1820_DEBUG__   false        /* Debug-Funktion */

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <wiringPi.h>

#define EXTERN

#include "./ds1820.h"
#include "../error.h"
#include "../common.h"
#include "../datetime.h"

#define NAMLEN    20                  /* max. Namenslänge     */
#define FOUND   "YES"                 /* Flag im String: OK   */
#define MTMARK  "t="                  /* Markierung Messwert  */

// Messgrenzen 18B20
#define URANGE 125.0
#define LRANGE -55.0


static char SensorTabelle[MAXDS1820][NAMLEN];
static  int SensorAnzahl;


//***************************************************************************//
/*
 * Define debug function.
 * ---------------------
 */
#if __DS1820_DEBUG__
#define DEBUG(...)  printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

//***************************************************************************//
//*********************************************************************************************//

// fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showDS1820_Error( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;                            // Fehlernummer sicherstellen
  char ErrText[ERRBUFLEN];
  char Fehler[2*NOTIZ];
  sprintf( Fehler, "%s - Err %d-%s", Message, errsv, strerror(errsv));
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  printf("    -- Fehler -->  %s\n", ErrText);   // lokale Fehlerausgabe
  digitalWrite (LED_GELB,   LED_AUS);
  digitalWrite (LED_GRUEN,  LED_AUS);
  digitalWrite (LED_BLAU,   LED_AUS);
  digitalWrite (LED_ROT,    LED_EIN);

  finish_with_Error(ErrText);                   // Fehlermeldung ausgeben
}
//*********************************************************************************************//

#define DEVICES "/sys/bus/w1/devices/"

// Tabelle mit allen DS1820-Sensoren anlegen
// -----------------------------------------
// in der Regel nur zur Initialisierung aufrufen
// Rückgabe: Anzahl der Sensoren
int ds1820_Refresh(void)
{
  DEBUG("\n   ===> %s()#%d: %s(void) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__);
  // Tabelle löschen
  // ---------------
  SensorAnzahl=0;
  for (int ix=0; ix < MAXDS1820; ix++)
  {
    SensorTabelle[ix][0] = '\0';
  }

  // Verzeichnis mit Sensornamen auslesen
  // --------------------------------------
  DIR *dir;
  struct dirent *dirzeiger;
  if((dir=opendir(DEVICES)) != NULL)
  {
    while((dirzeiger=readdir(dir)) != NULL)
    { // z.B.: '//28-00044cb880ff  28-818b891d64ff  w1_bus_master1'
      char sensorname[NAMLEN]={'\0'};
      strcpy(sensorname, (*dirzeiger).d_name);
      if (sensorname[0] >= '0' && sensorname[0] <= '9')
      {
        strcpy(SensorTabelle[SensorAnzahl], sensorname);
        #if _DEBUG_
        { // --- Debug-Ausgabe -----------------------------------------------
          char Text[ERRBUFLEN];
          #define MELDUNG  " --- %s()#%d: Sensor %d:'%s'"
          sprintf(Text, MELDUNG, __FUNCTION__, __LINE__,
                                 SensorAnzahl, SensorTabelle[SensorAnzahl]);
          DEBUG_AUSGABE2(Text);
          #undef MELDUNG
        } // -----------------------------------------------------------------
        #endif
        SensorAnzahl++;
      }
    }
  }
  else
  { // -- Error
    char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText, "Fehler opendir '%s' - %i(%s)",
                      DEVICES, errsv, strerror(errsv));
    showDS1820_Error( ErrText,__FUNCTION__,__LINE__);
  }

  // Lesezeiger wieder schließen
  // ----------------------------
  if(closedir(dir) == -1)
  { // -- Error
    char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText, "Fehler closedir '%s' - %i(%s)",
                      DEVICES, errsv, strerror(errsv));
    showDS1820_Error( ErrText,__FUNCTION__,__LINE__);
  }

  DEBUG("   <--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, SensorAnzahl);
  return SensorAnzahl;
}
//*********************************************************************************************//

#define SENSOR "/sys/bus/w1/devices/%s/w1_slave"

// aktuellen Temperaturwert eines Sensors lesen
// --------------------------------------------
float ds1820_Value(int SensorNummer)
{
  DEBUG("\n   ===> %s()#%d: %s(%d) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__, SensorNummer);

  char Sensorwert[ZEILE] = {'\0'};
  int cnt=0;
  float Temperatur = NICHTVORHANDEN;            // Rückgabewert

  if (SensorNummer >= MAXDS1820)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    sprintf(ErrText, "SensorNummer '%d' invalid", SensorNummer);
    showDS1820_Error( ErrText,__FUNCTION__,__LINE__);
  }

  // Prüfung, ob Sensor vorhanden
  // -------------------------------
  if (strlen(SensorTabelle[SensorNummer]) > 0)
  {
    // Sensorpfad erstellen
    // --------------------
    char sensorpath[NAMLEN] = {'\0'};
    sprintf(sensorpath, SENSOR, SensorTabelle[SensorNummer]);
    { // --- Debug-Ausgabe -----------------------------------------------------
      #define MELDUNG  " --- %s()#%d: Sensorpfad[%d]:'%s'"
      DEBUG(MELDUNG, __FUNCTION__, __LINE__, SensorNummer, sensorpath);
      #undef MELDUNG
    } // -----------------------------------------------------------------------

    // Sensor auslesen
    // ---------------
    FILE *fp;
    fp = fopen(sensorpath, "r");
    if(fp == NULL)
    { // -- Error
      char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
      int errsv = errno;
      sprintf(ErrText, "Fehler fopen '%s' - %i(%s)",
                        DEVICES, errsv, strerror(errsv));
      showDS1820_Error( ErrText,__FUNCTION__,__LINE__);
    }
    else
    { // Sensorwert zeichenweise einlesen
      // ---------------------------------
      int c;
      while( (c=fgetc(fp)) != EOF)
      {
        Sensorwert[cnt++] = c;
      }
    }
    // Datei schliessen
    // ----------------
    fclose(fp);

    { // --- Debug-Ausgabe --------------------------------------------------------
      #define MELDUNG  " --- %s()#%d: Sensorwert[%d]: %d chars = '\n%s'"
      DEBUG(MELDUNG, __FUNCTION__, __LINE__, SensorNummer, cnt, Sensorwert);
      #undef MELDUNG
    } // --------------------------------------------------------------------------

    // Sensorwert verarbeiten
    // ----------------------
    // der Sensorwert enthält den kompletten Datenblock
    Temperatur = FEHLLESUNG;                    // Rückgabewert
    char* pos = strstr(Sensorwert, FOUND);
    if( pos != NULL)
    {
      pos = strstr(Sensorwert, MTMARK);
      if( pos != NULL)
      {
        pos += strlen(MTMARK);
        Temperatur = atoi(pos) / 1000.0;
        { // --- Debug-Ausgabe --------------------------------------------------------
          #define MELDUNG  " --- %s()#%d: Temperatur: %f\n"
          DEBUG(MELDUNG, __FUNCTION__, __LINE__, Temperatur);
          #undef MELDUNG
        } // --------------------------------------------------------------------------
      }
    }

  } // -- Prüfung, ob Sensor vorhanden

  DEBUG("   <--- %s()#%d -<%f>- \n",  __FUNCTION__, __LINE__, Temperatur);
  return Temperatur;
}
//*********************************************************************************************//

#define ITDIR "/sys/class/thermal/thermal_zone0/temp"

// interne CPU-Temperatur lesen
// --------------------------------
float InternalTemperatur(void)
{
  DEBUG("\n   ===> %s()#%d: %s() =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__);

  FILE *tempfl;
  char tname[ZEILE] = ITDIR;                    // Pfad auf Temperatur-Datei
  char RBuff[ZEILE];                            // Lesebuffer
  double Temperatur = FEHLLESUNG;               // .. vorbesetzen

  tempfl = fopen(tname, "r");                   // Temperatur-Datei öffnen
  fscanf(tempfl, "%s", RBuff);                  // aus Datei lesen
  fclose(tempfl);                               // Datei wieder schließen
  Temperatur = atof(RBuff);
  Temperatur /= 1000;

  DEBUG("   <--- %s()#%d -<%f>- \n",  __FUNCTION__, __LINE__, Temperatur);
  return Temperatur;
}
//*********************************************************************************************//

// Sensorname holen
// ------------------
bool ds1820_Name(int SensorNummer, char* SensorName)
{
  if (SensorNummer >= MAXDS1820)
    return false;
  else
  {
    DEBUG("   <--- %s()#%d -<%s>- \n",  __FUNCTION__, __LINE__, SensorTabelle[SensorNummer]);
    strcpy(SensorName, SensorTabelle[SensorNummer]);
    DEBUG("   <--- %s()#%d -<%s>- \n",  __FUNCTION__, __LINE__, SensorName);
  }
  return true;
}
//*********************************************************************************************//
