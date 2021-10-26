//*********************************************************************************************//
//*                                                                                           *//
//* File:          common.c                                                                   *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2021-04-18;                                                                *//
//* Last change:   2021-09-25 - 12:52:18                                                      *//
//* Description:   Hilfsfunktionen und  Vereinbarungen zwischen den Programmen                *//
//*                                                                                           *//
//* Copyright (C) 2019-21 by Wolfgang Keuch                                                   *//
//*                                                                                           *//
//*********************************************************************************************//

#define __COMMON_DEBUG__ false

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "./datetime.h"
#include "./common.h"

//***********************************************************************************************
/*
 * Define debug function.
 * ---------------------
 */
#if __COMMON_DEBUG__
#define DEBUG(...)  printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define DEBUG_s(...) // printf(__VA_ARGS__)

#define DEBUG_p(...) // printf(__VA_ARGS__)

//***********************************************************************************************

// Zeichen im String ersetzen
// ------------------------------------------
// Rückgabewert: Anzahl der ersetzen Zeichen
unsigned replace_character(char* string, char from, char to)
{
  unsigned result = 0;
  if (!string) return 0;
  while (*string != '\0')
  {
    if (*string == from)
    {
      *string = to;
      result++;
    }
    string++;
  }
  return result;
}
//***********************************************************************************************

// überflüssigen Whitespace entfernen
// ----------------------------------
char* trim(char* myString)
{
  if (myString==NULL)
    return myString;                  // ist schon Leerstring
  while (*myString == ' ' || *myString == '\t' || *myString == '\n')
    myString++;                       // führenden Whitespace überspringen
  char* StringBeg = myString;         // Stringanfang merken

  while (*myString != '\0')
    myString++;                       // das Ende des Strings finden
  myString--;
  while (*myString == ' ' || *myString == '\t' || *myString == '\n')
  {
    myString--;                       // nachfolgenden Whitespace entfernen
    if (myString <= StringBeg)
      return NULL;                    // ist jetzt Leerstring
  }
  myString++;
  *myString = '\0';                   // neues Stringende

  return StringBeg;
}
//***********************************************************************************************

// Aufruf einens Kommandozeilen-Befehls
// ------------------------------------
// gibt das Ergebnis in 'Buf' zurück
bool getSystemCommand(char* Cmd, char* Buf, int max)
{
  FILE* lsofFile_p = popen(Cmd, "r"); // liest die Cmd-Ausgabe in eine Datei
  if (!lsofFile_p)
    return false;
  fgets(Buf, max, lsofFile_p);        // kopiert die Datei in den Buffer
  pclose(lsofFile_p);

  return true;
}
//***********************************************************************************************

// aktuelle IP-Adresse auslesen
// -----------------------------
char* readIP(char* myIP, int max)
{
  char myCmd[] = "hostname -I";
  if (getSystemCommand(myCmd, myIP, max))
  {
    trim(myIP);   // '\n' entfernen
  }
  else
  {
    strcpy(myIP, "--?--");
  }
  return myIP;
}
//***********************************************************************************************

// PID in Datei ablegen ablegen
// -----------------------------
long savePID(char* dateipfad)
{
  char dateiname[ZEILE];
  sprintf(dateiname, dateipfad, PROGNAME);
  FILE *piddatei;
  long pid = getpid();
  piddatei = fopen(dateiname, "w");
  if (piddatei != NULL)
  {
    char myPID[30];
    sprintf(myPID,"%ld", pid);
    fprintf (piddatei, myPID);
    fclose (piddatei);
  }
  return pid;
}
//***********************************************************************************************

#define RASPI_ID "/usr/raspi_id"  /* Datei mit der ID */

// RaspberryPi-Bezeichnung lesen
// ----------------------------
// z.B. '4#42'
char* readRaspiID(char* RaspiID)
{
  FILE *datei;
  datei = fopen(RASPI_ID, "r");
  if(datei == NULL)
    strcpy(RaspiID, "--?--");
  else
    fgets(RaspiID, NOTIZ, datei);
  return RaspiID;
}
//***********************************************************************************************

// das n.te Glied eines Strings zurückgeben
// --------------------------------------------------------------
// Stringformat: '/xxx/yyy/zzz/...'
bool split(const char *msg, char *part, int n)
{
  DEBUG("%s-%s:%s()#%d: -- %s('%s', -, %d) \n",
                                    __NOW__,__FILE__,__FUNCTION__,__LINE__,__FUNCTION__, msg,  n);
  bool retval=false;
  const char delim[] = "/";
  char* ptr;

  char val1[ZEILE] = {'\0'};
  strcpy(val1, msg);                  // umkopieren, da String zerstört wird

  int ix=0;
  ptr = strtok(val1, delim);          // das erste Glied

  while (ix++ < n)
  {
//    printf("------ split(%s) --- n=%d : '%s'\n", msg, ix, ptr);
    ptr = strtok(NULL, delim);        // das nächste Glied
    if (ptr == NULL) break;           // Stringende erreicht
  }
//  printf("------ split(%s) --- n=%d : '%s'\n", msg, ix, ptr);

  if (ptr == NULL)
    part = NULL;
  else
  {
    strcpy(part, ptr);
    retval = true;
  }
  DEBUG("%s-%s:%s()#%d: --  '%s'  == %d ==> '%s'\n",
                                      __NOW__,__FILE__,__FUNCTION__,__LINE__, msg, n, part);
  return retval;
}
//***********************************************************************************************

// um 'not used'-Warnings zu vermeiden:
// -----------------------------------
void destroyInt(int irgendwas)
{
  irgendwas=0;
}

void destroyStr(char* irgendwas)
{
  irgendwas=NULL;
}
//***********************************************************************************************

// Abfrage auf numerischen String
// ------------------------------
// Vorzeichen sind erlaubt
bool isnumeric(char* numstring)
{
  if (!(isdigit(*numstring) || (*numstring == '-') || (*numstring == '+')))
    return false;
  numstring++;
  for (int ix=0, en=strlen(numstring); ix < en; ix++)
  {
    if (!isdigit(*numstring++))
     return false;
  }
  return true;
}
//***********************************************************************************************

// numerisches Wert aus String holen
// ---------------------------------
// jeweils die letzte Zifferngruppe
bool getnumeric(char* instring, char* numeric)
{
  bool retval = false;
  bool neu = true;
  char* numbuf = numeric;             // Stringanfang merken
  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: in='%s' num=%s\n"
    DEBUG_s(MELDUNG, __FUNCTION__, __LINE__, instring, numeric);
    #undef MELDUNG
  } // --------------------------------------------------------------
  while (*instring != '\0')
  {
    if ((*instring < '0') || (*instring > '9'))
    {
      instring++;
      neu = true;
    }
    else
    {
      if (neu)
        numeric = numbuf;             // neu aufsetzen
      *numeric++ = *instring++;
      retval=true;
      { // --- Debug-Ausgaben ------------------------------------------
        #define MELDUNG   "       %s()#%d: in='%s' num='%s'\n"
        DEBUG_s(MELDUNG, __FUNCTION__, __LINE__, instring, numbuf);
        #undef MELDUNG
      } // --------------------------------------------------------------
    }
    *numeric = '\0';
  }
  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: in='%s' num='%s'\n"
    DEBUG_s(MELDUNG, __FUNCTION__, __LINE__, instring, numeric);
    #undef MELDUNG
  } // --------------------------------------------------------------
  return retval;
}
//***********************************************************************************************

// Ist dieser String ein Datum?
// ----------------------------
// wie '2021-05-09'
bool isDatum(char* einString)
{
  bool retval = false;
  char* byte4 = einString+4;
  char* byte7 = einString+7;
  if (strlen(einString) == 10)
  {
    if (*byte4 == '-')
    {
      *byte4 = '0';
      if (*byte7 == '-')
      {
        *byte7 = '0';
        retval = isnumeric(einString);
      }
    }
  }
  return retval;
}
//***********************************************************************************************
//***********************************************************************************************


// Dateityp Bild/Film ermitteln
// ----------------------------
enum Filetype getFiletyp(const char* Filename)
{
#if _DEBUG
  printf("=====> %s()#%d: %s(%s) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__, Filename);
#endif
  char *ptr;
  enum Filetype retval = ANDERE;

  ptr = strrchr(Filename, '.');
  if (ptr != NULL)
  {
    if (strncmp(ptr, _JPG, strlen(_JPG)) == 0)
      retval = JPG;
    else if (strncmp(ptr, _AVI, strlen(_AVI)) == 0)
      retval = AVI;
    else if (strncmp(ptr, _MKV, strlen(_MKV)) == 0)
      retval = MKV;
  }
#if _DEBUG
  printf("<----- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
#endif
  return retval;
}
//***********************************************************************************************
//*********************************************************************************************//

// Vergleich, ob die n.ten Glieder der Strings übereinstimmen
// -----------------------------------------------------------
// topic: "xxx/yyy/zzz"
// key:   "yyy"
bool match(const char *topic, const char *key, int n)
{
  bool retval=false;
  char delim[] = {DELIM, '\0'};
  char* ptr1;
  char* ptr2;
  char val1[ZEILE] = {'\0'};
  char val2[ZEILE] = {'\0'};
   DEBUG("%s-%s:%s()#%d: -- topic='%s', key='%s'\n",
                    __NOW__,__FILE__,__FUNCTION__,__LINE__, topic, key);
  strcpy(val1, topic);                // umkopieren, da String zerstört wird
  strcpy(val2, key);

  // 1. Wert
  // -------
  int ix=0;
  ptr1 = strtok(val1, delim);         // das erste Glied
  while (ix < n)
  {
    ix++;
    ptr1 = strtok(NULL, delim);       // das nächste Glied
    if (ptr1 == NULL) break;          // Stringende erreicht
  }

  // 2. Wert
  // -------
  if (ptr1 != NULL)
  {
    ix=0;
    ptr2 = strtok(val2, delim);       // das erste Glied
    while (ix < n)
    {
      ix++;
      ptr2 = strtok(NULL, delim);     // das nächste Glied
      if (ptr2 == NULL) break;        // Stringende erreicht
    }
    if (ptr2 != NULL)
      retval = (0 == strcmp(ptr1, ptr2));
  }

  if (retval)
     DEBUG("%s-%s:%s()#%d: -- OK: %s('%s')==%s('%s')\n",
                     __NOW__,__FILE__,__FUNCTION__,__LINE__, topic, ptr1, key, ptr2);
  else
     DEBUG("%s-%s:%s()#%d: -- -xx-: %s('%s')==%s('%s')\n",
                     __NOW__,__FILE__,__FUNCTION__,__LINE__, topic, ptr1, key, ptr2);
  return retval;
}
//*********************************************************************************************//

// n.tes Glied des Strings zurückgeben
// -----------------------------------------------------------
// String: "xxx/yyy/zzz/..."
// pos:     Position
// Ziel:    Zielbuffer
bool partn(const char* String, int pos, char* Ziel)
{
  bool retval=false;
  char delim[] = {DELIM, '\0'};
  char* ptr1;
  char* ziel = Ziel;
  char string[ZEILE] = {'\0'};
  strcpy(string, String);             // umkopieren, da String zerstört wird
  DEBUG("%s-%s:%s()#%d: -- String='%s', pos=%d, Ziel='%s'\n",
                       __NOW__,__FILE__,__FUNCTION__,__LINE__, String, pos, Ziel);

  // 1. Wert
  // -------
  int ix=0;
  ptr1 = strtok(string, delim);       // das erste Glied
  while (ix < pos)
  {
    ix++;
    ptr1 = strtok(NULL, delim);       // das nächste Glied
    if (ptr1 == NULL) break;          // Stringende erreicht
  }

  if (ptr1 != NULL)
  {
    do                                // Begrenzung finden
    {
      *ziel++ = *ptr1++;
      if (*ptr1 == '\0') break;       // Stringende erreicht
    }
    while (*ptr1 != DELIM);
    *ziel = '\0';
    retval = true;
  }
  DEBUG("%s-%s:%s()#%d: -- String='%s', pos=%d, Ziel = '%s'\n",
                       __NOW__,__FILE__,__FUNCTION__,__LINE__, String, pos, Ziel);
  return retval;
}
//*********************************************************************************************//

// Vergleich, ob das n.te Glied des Strings den Schlüssel enthält
// --------------------------------------------------------------
// Stringformat: 'xxx/yyy/zzz/...' Feldnr. Inhalt
bool matchn(const char *topic,     int n,  int key)
{
   DEBUG("%s-%s:%s()#%d: -- %s('%s', %d, %d) \n",
                    __NOW__,__FILE__,__FUNCTION__,__LINE__,__FUNCTION__, topic, key, n);
  bool retval=false;
  const char delim[] = {DELIM, '\0'};
  char* ptr1;
  char val1[ZEILE] = {'\0'};
  char val2[ZEILE] = {'\0'};
  strcpy(val1, topic);                // umkopieren, da String zerstört wird
  sprintf(val2, "%d", key);           // Schlüssel als String

  // 1. Wert
  // -------
  int ix=0;
  ptr1 = strtok(val1, delim);         // das erste Glied
  while (ix < n)
  {
    ix++;
    ptr1 = strtok(NULL, delim);       // das nächste Glied
    if (ptr1 == NULL) break;          // Stringende erreicht
  }

  // 2. Wert
  // -------
  if (ptr1 != NULL)
  {
    retval = (0 == strcmp(ptr1, val2));
  }

  if (retval)
     DEBUG("%s-%s:%s()#%d: -- OK: %s('%s')==%d('%s')\n",
                     __NOW__,__FILE__,__FUNCTION__,__LINE__, topic, ptr1, key, val2);
  else
     DEBUG("%s-%s:%s()#%d: -- -xx-: %s('%s')==%d('%s')\n",
                     __NOW__,__FILE__,__FUNCTION__,__LINE__, topic, ptr1, key, val2);
  return retval;
}
//*********************************************************************************************//

// Watchdog-Datei auffrischen
// ---------------------------
// Wenn dies Funktion nicht regelmäßig aufgerufen wird, schlägt der watchdog zu!
bool feedWatchdog(char* Name)
{
  time_t now;
  FILE* fWatchDog;                                        // Zeiger auf Datenstrom der Datei
  char filename[ZEILE];
  char filedata[ZEILE];
  sprintf(filename,"/tmp/%s.%s", Name, "wdg");            // der generierte Dateiname
  now = time(0);                                          // Sekunden seit 1.1.70
  sprintf(filedata, "%s\n", ctime(&now));                 // Datum/Uhrzeit als Inhalt
  static time_t nextRefresh = 0;

  if (nextRefresh < time(NULL))
  {
#ifdef _DEBUG_
     DEBUG("%s-%s:%s#%d - %s <= '%s'",__NOW__,__FILE__,__FUNCTION__,__LINE__, filename, filedata);
#endif

    fWatchDog = fopen(filename, "w+b");
    if (fWatchDog)
    {
      fwrite(&filedata, strlen(filedata), 1, fWatchDog);
      fclose(fWatchDog);
      fWatchDog = NULL;
    }
    else
      return false;
    nextRefresh = time(NULL) + REFRESH;                   // nächster Durchlauf
  }

  return true;
}
//***********************************************************************************************
//***********************************************************************************************

// Protokoll schreiben
// ------------------------
bool MyLog(const char* Program, const char* Function, int Line, const char* pLogText)
{
  DEBUG_p("=======> %s()#%d: %s(\"%s\")\n", __FUNCTION__, __LINE__, __FUNCTION__, pLogText);
  int status = false;

  FILE* logFile = fopen(LOGFILE, "a+");
  if(NULL == logFile)
  { // --- Debug-Ausgaben ---------------------------------------------------
    char ErrText[ZEILE];
    sprintf(ErrText, "Error fopen('%s'): Error %d(%s)", LOGFILE, errno, strerror(errno));
    DEBUG_p("         %s()#%d: %s!\n", __FUNCTION__, __LINE__, ErrText);
  } // ----------------------------------------------------------------------
  else
  {
    char neueZeile[ZEILE];
    char aktuelleZeit[NOTIZ];
    sprintf(neueZeile, "%s- %s.%s()#%d: %s\n",
               mkdatum(time(0), aktuelleZeit), Program, Function, Line, pLogText);
    { // --- Debug-Ausgaben ---------------------------------------------------
      DEBUG_p("         %s()#%d: neue Zeile: '%s'!\n",
                                            __FUNCTION__, __LINE__, neueZeile);
    } // ----------------------------------------------------------------------
    fputs(neueZeile, logFile);
    fclose(logFile);
    status = true;
  }

  DEBUG_p("<------- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// alle Vorkommen eines Strings im String ersetzen
// -----------------------------------------------
// 2021-09-21; nicht getestet
bool replace1( char* OldText, char* NewText, const char* oldW, const char* newW )
{
  bool retval = false;
  char* alt = OldText;
  char* neu = NewText;
  strcpy(neu, OldText);               // erstmal alles kopieren
  char* fnd = strstr(alt, oldW);      // alten Text suchen
  while (fnd != NULL)
  { // 'oldW' enthalten
    int lg = fnd-alt;                 // übersprungene Textlänge
    neu += lg;                        // neu-Zeiger korrigieren
    strcpy(neu, newW);                // draufkopieren
    neu += strlen(newW);              // neu-Zeiger korrigieren
    alt += strlen(oldW);              // neu-Zeiger korrigieren
    fnd = strstr(alt, oldW);          // alten Text suchen
  }
  *neu = '\0';                        // Stringende

  return(retval);
}
//***********************************************************************************************

//*************************************************************

char* ascii[] =
{
  "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
  "BS",  "HT",  "LF",  "VT",  "FF", "CR", "SO", "SI",
  "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
  "CAN", "EM ", "SUB", "ESC", "FS", "GS", "RS", "US",
  "NIX", "EIN", "AUS", "HELL", "DUNKEL", "SCHALTEN", "MESSEN",
  "---", "AUTO", "GRENZE"
};

//#define NIX       ' '      /* 0x20 - Space */
//#define EIN       '!'      /* 0x21         */
//#define AUS       '"'      /* 0x22         */
//#define HELL      '#'      /* 0x23         */
//#define DUNKEL    '$'      /* 0x24         */
//#define SCHALTEN  '%'      /* 0x25         */
//#define MESSEN    '&'      /* 0x26         */
//#define LEER      '\''     /* 0x27         */
//#define GRENZE    '('      /* Begrenzung   */
//#define NIX       ' '      /* 0x20 - Space */
//#define EIN       '!'      /* 0x21         */
//#define AUS       '"'      /* 0x22         */
//#define HELL      '#'      /* 0x23         */
//#define DUNKEL    '$'      /* 0x24         */
//#define SCHALTEN  '%'      /* 0x25         */
//#define MESSEN    '&'      /* 0x26         */
//#define LEER      '\'      /* 0x27         */
//#define AUTO      '('      /* 0x28         */
//#define GRENZE    ')'      /* Begrenzung   */

#define SCHALTEN  '%'      /* 0x25         */
#define MESSEN    '&'      /* 0x26         */
#define LEER      '\''     /* 0x27         */
#define AUTO      '('      /* 0x28         */
#define GRENZE    ')'      /* Begrenzung   */

//*************************************************************

// Steuerzeichen über String finden
// --------------------------------
Ctrl Str2Ctrl(char* strControl)
{
  Ctrl cch = NUL;
  do
  {
    if (strcmp(strControl, ascii[(int)cch]) == 0)
      return cch;
  }
  while (++cch < GRENZE);
  return -1;
}
//*************************************************************

// Steuerzeichen als String ausgeben
// ----------------------------------
bool Ctrl2Str(Ctrl Control, char* strControl)
{
  if (Control <= GRENZE)
  {
    strcpy(strControl, ascii[(int)Control]);
    return true;
  }
  else
  {
    strControl = NULL;
    return false;
  }
}
//*************************************************************

// String zu Großbuchstaben
// -------------------------
char* toUpper(char* low)
{
  char* beg = low;
  char* upp = low;
  while(*low)
  {
    *upp++ = toupper(*low++);
  }
  return(beg);
}
//***********************************************************************************************
