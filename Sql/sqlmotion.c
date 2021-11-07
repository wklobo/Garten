//*********** Kamera ********************************************************//
//*                                                                         *//
//* File:          sqlmotion.c                                              *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2014-07-20  --  2016-02-18                               *//
//* Last change:   2021-11-07 - 11:04:08                                    *//
//* Description:   Weiterverarbeitung von 'motion'-Dateien:                 *//
//*                Event ermitteln, daraus ein Verzeichnis erstellen,       *//
//*                zugehörige Dateien in dieses Verzeichnis verschieben     *//
//*                                                                         *//
//* Copyright (C) 2014-22 by Wolfgang Keuch                                 *//
//*                                                                         *//
//* Kompilation:                                                            *//
//*    make                                                                 *//
//*                                                                         *//
//* Aufruf:                                                                 *//
//*    ./SqlMotion thisPfad/*                                               *//
//*                                                                         *//
//***************************************************************************//

#define _POSIX_SOURCE
#define _DEFAULT_SOURCE
#define _MODUL0
#define __SQLMOTION_MYLOG__    true
#define __SQLMOTION_DEBUG__    true
#define __SQLMOTION_DEBUG__d   false     /* Datenbanken */
#define __SQLMOTION_DEBUG__1   false
#define __SQLMOTION_DEBUG__2   false
#define __SQLMOTION_DEBUG__z   true

#include "./version.h"
#include "./sqlmotion.h"

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <wiringPi.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

#include "../error.h"
#include "../datetime.h"
#include "../../sendmail/sendMail.h"

// statische Variable
// --------------------
static time_t ErrorFlag = 0;          // Steuerung rote LED
static int myPID;                     // Prozess-ID
static int EventNummer = 0;						// laufende Ereignis-Nummer

// Breakpoints
// -----------
#define BREAK1      0     /* der erste Durchlauf                  */
#define BREAK21     0     /* Beginn Phase 2                       */
#define BREAK22     0     /* nächste Datei in diesem Verzeichnis  */
#define BREAK23     0     /* ein Eventverzeichnis durchlaufen     */
#define BREAK24     0     /* alle Eventverzeichnisse durchlaufen  */ 

char MailBody[BODYLEN];

//***************************************************************************// bool MyLog(const char* pLogText)

/*
 * Define debug function.
 * ---------------------
 */
 
//// Log-Ausgabe
//// -----------
#if __SQLMOTION_MYLOG__
 #define MYLOG(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
 #define MYLOG(...)
#endif

// main()
// ------
#if __SQLMOTION_DEBUG__
 #define DEBUG(...)  printf(__VA_ARGS__)
 #define DEBUG_m(...)  printf(__VA_ARGS__)
 #define SYSLOG(...)  syslog(__VA_ARGS__)
 #define BREAKmain1 false
 #define BREAKmain2 false
#else
 #define DEBUG(...)
 #define DEBUG_m(...)
 #define SYSLOG(...)  
#endif

// Datenbanken
// -------------
#if __SQLMOTION_DEBUG__d
 #define DEBUG_d(...)  printf(__VA_ARGS__)
#else
 #define DEBUG_d(...)
#endif


#if __SQLMOTION_DEBUG__1
#define DEBUG_1(...)  printf(__VA_ARGS__)
#else
#define DEBUG_1(...)
#endif

#if __SQLMOTION_DEBUG__2
 int cnt = 0;
 #define DEBUG_2(...)  printf(__VA_ARGS__)
#else
 #define DEBUG_2(...)
#endif

#if __SQLMOTION_DEBUG__z
 #define DEBUG_z(...)  printf(__VA_ARGS__)
 #define MYLOG(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
 #define DEBUG_z(...)
#endif

//************************************************************************************//

// Fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_Error( char* Message, const char* Func, int Zeile)
{
  char ErrText[ERRBUFLEN];
  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Message);

  printf("\n    -- Fehler --> %s\n", ErrText);    // lokale Fehlerausgabe
  syslog(LOG_NOTICE, ErrText);
  digitalWrite (LED_ge1,   LED_AUS);
  digitalWrite (LED_gn1,   LED_AUS);
  digitalWrite (LED_bl1,   LED_AUS);
  digitalWrite (LED_rt,    LED_EIN);

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, 
       "<<< %s: Exit!",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------
  
  finish_with_Error(ErrText);
}
//************************************************************************************//

// Fataler Datenbank-Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_SQL_Error( char* Label, const char* Func, int Zeile, MYSQL *con)
{
  char ErrSql[ZEILE];
  char ErrText[ERRBUFLEN];
  sprintf(ErrSql, "SQL-Error %d(%s)", mysql_errno(con), mysql_error(con));
  mysql_close(con);                             // Datenbank schließen
  sprintf(ErrText, "%s -- %s", Label, ErrSql);
  showMain_Error( ErrText, Func, Zeile);        // Meldung weiterreichen
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

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, 
       "<<< %s",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------
  
  return errsv;
}
//************************************************************************************//

int getEventNummer()
{
  char buffer[NOTIZ]="000000";
  FILE* fp = fopen(EVENTNUMMER, "r");		
	if (NULL != fp)
	{
  	fread(buffer, sizeof(buffer), 1, fp);
  	fclose(fp);
  }
  return atoi(buffer);
}

void saveEventNummer(int EvNum)
{
  char buffer[NOTIZ];
  sprintf(buffer, "%d", EvNum);
  FILE* fp = fopen(EVENTNUMMER, "w");	 
	if (NULL != fp)
	{
  	fwrite(buffer, strlen(buffer) + 1, 1, fp);
  	fclose(fp);
  }
}
//************************************************************************************//

// Kamera-LED schalten
//  ------------------
void ChannelLED(int Herkunft, int EinAus)
{
	switch(Herkunft) 
	{
		case PIX: digitalWrite (LED_gn1, EinAus); break;
		case USB: digitalWrite (LED_ge1, EinAus); break;
		default:  digitalWrite (LED_rt, LED_EIN); break;
	}
}
//************************************************************************************//

//Kamera-LED blinken
// ------------------
void ChannelBlink(int Herkunft, int Blinkzeit)
{
	ChannelLED(Herkunft, LED_AUS);
  delay(Blinkzeit);
	ChannelLED(Herkunft, LED_EIN);
  delay(Blinkzeit);
}
//************************************************************************************//

// Eventnummer ermitteln
// ---------------------
//     home/pi/motion/pix/401-20210205_134103-02.jpg
//     -- 06-20201229133023-01 -- Eventnummer steht jetzt vorn

#define EVMRK '/'      /* vordere Markierung von hinten */
#define EVEND '-'      /* hintere Markierung */

int getEventNo(char* Filename)
{
  DEBUG("=> %s()#%d %s(%s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, Filename);
  int EventNo = -1;
  char sEvent[12];

  char* evbeg = strrchr(Filename, EVMRK) + 1;
  if (evbeg != NULL)
  {
    char* evend = strchr(evbeg, EVEND);
    if (evend != NULL)
    {
      int evlen = evend-evbeg;
      strncpy(&sEvent[0], evbeg, evlen);
      sEvent[evlen] = '\0';
      EventNo = atoi(sEvent);
    }
  }

  DEBUG("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, EventNo);
  return EventNo;
}
//************************************************************************************// 

// Eventnamen erzeugen
// -----------------------------------------  
// ausgehend vom der Eventnummer (xx_ ... )wird die erste nicht besetzte Nummer gesucht
// Dies ist der 'unique'-Schlüssel für das Ereignis       ` 

#define EVSLH '_'      /* Markierung von hinten */

int getEventKey(MYSQL* con, char* FolderName, char* EventKey)
{
  DEBUG_d("=> %s()#%d: %s('%s', '%s') ======\n",
             __FUNCTION__, __LINE__, __FUNCTION__, FolderName, EventKey);
	Startzeit(T_FILE);                                           // Zeitmessung starten

  int retval = false;
  int TempZahl = 1000;                // Default-Basis
  char textQuery[ZEILE];              // Buffer für Datenbankabfragen

  char* str = strrchr(FolderName, EVSLH) + 1;
  // dies sollte eine 5stellige Zahl sein
  if (str != NULL)
  { // ... aus Foldername
    TempZahl = atoi(str);
  }
  { // --- Log-Ausgabe ---------------------------------------------------------
  	char LogText[ZEILE];  sprintf(LogText, "%d   - %s - EventNummer: %d -  TempZahl: %d", myPID, __FUNCTION__, EventNummer, TempZahl);
    MYLOG(LogText);
  }
 	TempZahl = EventNummer > TempZahl ? EventNummer : TempZahl;

  unsigned int num_rows = -1;         // Anzahl Zeilen in der Abfrage
  do  // ===============================================================================
  {
    char TempKey[ZEILE];
    sprintf( TempKey, "%s%05d", _FOLDER, TempZahl);
    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: TempZahl='%d' => '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, TempZahl, TempKey);
      #undef MELDUNG
    } // --------------------------------------------------------------
    sprintf( textQuery, "SELECT  %s FROM %s WHERE %s = '%s';",
                             "evKeyID", "tabEvent", "evEvent", TempKey);    
    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery);
      #undef MELDUNG
      { // --- Log-Ausgabe ---------------------------------------------------------
      	char LogText[ZEILE];  sprintf(LogText, "%d   - %s - '%s'", myPID, __FUNCTION__, textQuery);
        MYLOG(LogText);
      }
    } // --------------------------------------------------------------

    if (mysql_query(con, textQuery))            // Abfrage
      showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);
    else // Abfrage erfolgreich, Rückgabedaten können verarbeitet werden
    {
      MYSQL_RES* result = mysql_store_result(con);  // Array-Buffer Ergebnismenge
      if (result)
      { // Es ist ein Ergebnis vorhanden
        // -----------------------------  
        num_rows = mysql_affected_rows(con);    // Anzahl Zeilen in der Abfrage;    
        { // --- Debug-Ausgaben --------------------------------------------------
          #define MELDUNG   "   %s()#%d: num_rows: <'%i'>\n"
          DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, num_rows);
          #undef MELDUNG
        } // ---------------------------------------------------------------------
        { // --- Log-Ausgabe ---------------------------------------------------------
        	char LogText[ZEILE];  sprintf(LogText, "%d     -  %s - num_rows = %d", myPID, __FUNCTION__, num_rows);
          MYLOG(LogText);
        }
      }
      EventNummer = TempZahl;
      mysql_free_result(result);                // Buffer freigeben
      TempZahl++;
    } 
    strcpy(EventKey, TempKey);
  }
  while(num_rows > 0);  // wiederholen, bis es den Key nicht mehr gibt ===============
  
  { // --- Log-Ausgabe ---------------------------------------------------------
  	char LogText[ZEILE];  sprintf(LogText, "%d   - %s: %ld msec", myPID, __FUNCTION__, Zwischenzeit(T_FILE));
    MYLOG(LogText);
  }

  retval = true;
 
  DEBUG_d("<- %s()#%d -<%d>-  : '%s' \n",  __FUNCTION__, __LINE__, retval, EventKey);
  return retval;
}
//************************************************************************************//

// Verzeichnis für Eventnummer anlegen
// -----------------------------------
// Rückgabe: Neu / schon vorhanden

#define MODUS  0774

int makeEventfolder(char* thisPfad, int EvNummer, char* Verzeichnis)
{
  DEBUG_m("=> %s()#%d: %s(%s, %d, %s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, thisPfad, EvNummer, Verzeichnis);
  int retval = FOLDER_NEW;
  mode_t oldMask = umask((mode_t) 0);
  umask(oldMask & ~(S_IWGRP|S_IXGRP));

  { // --- Debug-Ausgaben --------------------------------------------------------
    #define MELDUNG     "   %s()#%d: oldMask '%on', newMask '%on'\n"
    DEBUG_m(MELDUNG, __FUNCTION__, __LINE__, oldMask, oldMask ^ S_IXGRP);
    #undef MELDUNG
  } // ---------------------------------------------------------------------------

  sprintf(Verzeichnis, "%s%s%04d", thisPfad, _FOLDER, EvNummer);
  if(mkdir(Verzeichnis, MODUS) == -1)   // Fehler aufgetreten ...
  {
    if (errno == EEXIST)                        // ... der darf!
      retval = FOLDER_EXIST;
    else
    { // -- Error
      char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
      int errsv = errno;
      sprintf(ErrText, "Error Verzeichnis '%s' - = %i(%s)",
                        Verzeichnis, errsv, strerror(errsv));
      showMain_Error( ErrText,__FUNCTION__,__LINE__);
    }
  }
  umask(oldMask);
  DEBUG_m("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//************************************************************************************//

// Datei in das neue Verzeichnis verschieben
// -----------------------------------------
// über 'rename'
void MoveFile( const char* File, const char* Verz)
{
  DEBUG("=> %s()#%d: %s(%s, \n                            %s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, File, Verz);
  char myDir[128];
  strcpy(myDir, Verz);
  char *ptr = strrchr(File, '/');
  strcat(myDir, ptr);

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "   %s()#%d: Datei: '%s',"\
    "\n                     alt: '%s',\n                     neu: '%s'\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, ptr, File, Verz);
    #undef MELDUNG
  } // --------------------------------------------------------------

  if( (rename(File, myDir)) < 0)
  { // -- Error
    char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText, "Error rename '%s' ->  '%s'- = %i(%s)",
                     File, Verz, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }
  DEBUG("<- %s()#%d -<>- \n",  __FUNCTION__, __LINE__);
}
//************************************************************************************//

// PID holen
// -----------
pid_t getPID(void)
{
  DEBUG("====> %s()#%d: %s(void) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__);
  pid_t myPID = 0;
  FILE *piddatei;
  piddatei = fopen(FPID, "r");
  if (piddatei != NULL)
  {
    char puffer[ZEILE];
    fread(&puffer, sizeof(char), ZEILE-1, piddatei);
    myPID = atoi(puffer);
    fclose (piddatei);
  }
  DEBUG("<---- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, myPID);
  return myPID;
}
//***********************************************************************************************
//***********************************************************************************************

// Datenbank anlegen
// ==================
MYSQL* CreateDB(MYSQL* con)
{
  DEBUG_d("===> %s()#%d: %s(%ld) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__, (long int)con);
  // Connection initialisieren
  // -------------------------
  con = mysql_init(con);
  if (con == NULL)
    showMain_SQL_Error( "Init", __FUNCTION__, __LINE__, con);

  // User einloggen
  // --------------
#if __SQLMOTION_DEBUG__d
  syslog(LOG_NOTICE, "CreateDB(%p) connect - Host:'%s', User:'%s', Passwort:'%s' \n", con, THISHOST, THISUSER, THISPW);
#endif
  if (mysql_real_connect(con, THISHOST, THISUSER, THISPW, NULL, 0, NULL, 0) == NULL)
    showMain_SQL_Error( "Connect", __FUNCTION__, __LINE__, con);

  // Datenbank erzeugen
  // ------------------
  char textCrea[100] = "CREATE DATABASE ";
  strcat(textCrea, MYDATABASE);
  if (mysql_query(con, textCrea))
  {
    if(mysql_errno(con) == ER_DB_CREATE_EXISTS)
    {
      // Fehler: 1007 - Kann Datenbank '%s' nicht erzeugen. Datenbank existiert bereits
    }
    else
    { // sonstiger Fehler
      // ----------------
      showMain_SQL_Error( textCrea, __FUNCTION__, __LINE__, con);
    }
  }

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: CREATE DATABASE %s: %p\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__,  MYDATABASE, con);
    #undef MELDUNG
  } // --------------------------------------------------------------

  DEBUG_d("<--- %s()#%d -<%ld>- \n",  __FUNCTION__, __LINE__,  (long int)con);
  return con;
}
//***********************************************************************************************

// Tabellen anlegen
// ================
void CreateTables(MYSQL* con)
{
  DEBUG_d("===> %s()#%d: %s(%ld) =======\n",
             __FUNCTION__, __LINE__, __FUNCTION__,  (long int)con);
  MYSQL_RES *res;
  char textQuery[1000];     // Buffer für Datenbankabfragen

  // Datenbank aktivieren
  // --------------------
  sprintf( textQuery, "USE %s;", MYDATABASE);
  if (mysql_query(con, textQuery))
    showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: %s: %p\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery, con);
    #undef MELDUNG
  } // --------------------------------------------------------------

  // Tabelle für Events prüfen
  // -------------------------------
  {
    sprintf( textQuery, "SELECT * FROM %s;", MYEVENTTABLE);
    mysql_query(con, textQuery);
    res = mysql_use_result(con);

    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "     %s()#%d: Test TABLE: %s - Error %i\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery, mysql_errno(con));
      #undef MELDUNG
    } // --------------------------------------------------------------

    if(mysql_errno(con) == NO_ERROR)
      {} // Tabelle vorhanden

    else if(mysql_errno(con) == ER_NO_SUCH_TABLE)
      showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);

    mysql_free_result(res);
  }

  DEBUG_d("<--- %s()#%d -< >- \n",  __FUNCTION__, __LINE__ );
}
//***********************************************************************************************

// Datensatz für Event anlegen ...
// ===============================
// thisEvent - Name des Events (Unterverzeichnis)
// FaDatum   - Datum  des Events
// thisSize  - Größe des Unterverzeichnisses
// Remark    - weitere Informationen
// ... und die ID des Datensatzes zurückgeben
int AddEvent(MYSQL* con, char* thisEvent, time_t* FaDatum, long thisSize, char* Remark)
{
  DEBUG_d("=> %s()#%d: %s(%ld, %s, %ld, %ld, %s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__,
             (long int)con, thisEvent, *FaDatum, thisSize, Remark);
	Startzeit(T_EVENT);               	// Zeitmessung starten

  int PriID = 0;                      // PrimaryID des Datensatzes
  char textQuery[ZEILE];              // Buffer für Datenbankabfragen
  bool makeNewSet = false;            // Flag: neuen Datensatz anlegen
  char thisDatum[20] = {'\0'};        // String Datum
  char thisZeit[20] = {'\0'};         // String Zeit
  struct tm* ts = localtime(FaDatum); // Wandlung der Ereigniszeit
  #if _DEBUGzd
    long Zeitmarke(6);
  #endif

  sprintf(thisDatum, "%04d-%02d-%02d", 1900 + ts->tm_year, 1 + ts->tm_mon, ts->tm_mday);
  sprintf(thisZeit,  "%02d:%02d:%02d", ts->tm_hour, ts->tm_min, ts->tm_sec);

  // Prüfung, ob Datensatz schon vorhanden
  // -------------------------------------
  sprintf( textQuery, "SELECT %s FROM %s WHERE %s='%s'" \
                      " AND %s = '%s' AND %s = '%s' ORDER BY %s",
                        EVKEYID, MYEVENTTABLE, EVEVENT, thisEvent, 
                        EVDATE, thisDatum, EVTIME, thisZeit, EVSAVED);

//#define  EVKEYID     "evKeyID"
//#define  EVSAVED     "evSaved"
//#define  EVEVENT     "evEvent"
//#define  EVDATE      "evDate"
//#define  EVTIME      "evTime"
//#define  EVSIZE      "evSize"
//#define  EVREMARK    "evRemark"

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "   %s()#%d: textQuery: %s\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery);
    #undef MELDUNG
  } // --------------------------------------------------------------

  if (mysql_query(con, textQuery))
    showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);
  else // Anfrage erfolgreich, Rückgabedaten können verarbeitet werden
  {
    MYSQL_RES *result;                // Array Ergebnismenge
    MYSQL_ROW row=0;                  // Array Datensatz
    result = mysql_store_result(con);
    if (result)
    { // Es ist ein Ergebnis vorhanden
      // -----------------------------
      unsigned int num_rows = mysql_affected_rows(con);             // Zeile in der Abfrage

      { // --- Debug-Ausgaben -----------------------------------------------------
        #define MELDUNG   "   %s()#%d: 2.num_rows: '%i', 3.num_fields: '%i'\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, num_rows, mysql_num_fields(result));
        #undef MELDUNG
      } // -----------------------------------------------------------------------------

      if( num_rows == 0 )
      { // es gibt noch keinen Datensatz mit diesem Event
        // ----------------------------------------------
        makeNewSet = true;
      }
      else
      { // ein Datensatz bereits vorhanden
        // -------------------------------
        row = mysql_fetch_row( result );

        { // --- Debug-Ausgaben ------------------------------------------
          #define MELDUNG   "   %s()#%d: Feld[0] = '%s'"
          DEBUG(MELDUNG, __FUNCTION__, __LINE__, row[0]);
          #undef MELDUNG
        } // --------------------------------------------------------------

        PriID = atoi(row[0]);         // der zuletzt gelesene (j_ngste) Wert gilt
      }
    }
    mysql_free_result(result);
  }

  if( makeNewSet )
  { // noch kein Eintrag - Datensatz einfügen
    // --------------------------------------
    MYSQL_RES *result;
    sprintf( textQuery, "INSERT INTO %s (%s, %s, %s, %s, %s)" \
                        "VALUES ('%s', '%s', '%s', '%ld', '%s')",
                        MYEVENTTABLE, EVEVENT, EVDATE, EVTIME, EVSIZE, EVREMARK, 
                                    thisEvent, thisDatum, thisZeit, thisSize, Remark);

//#define  EVKEYID     "evKeyID"
//#define  EVSAVED     "evSaved"
//#define  EVEVENT     "evEvent"
//#define  EVDATE      "evDate"
//#define  EVTIME      "evTime"
//#define  EVSIZE      "evSize"
//#define  EVREMARK    "evRemark"

    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: textQuery = '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, textQuery);
      #undef MELDUNG
    } // --------------------------------------------------------------

    if (mysql_query(con, textQuery))
      showMain_SQL_Error( textQuery, __FUNCTION__, __LINE__, con);
    else   // Datensatz erfolgreich angelegt
    {
      if ((result = mysql_store_result(con)) == 0 &&
          mysql_field_count(con) == 0 &&
          mysql_insert_id(con) != 0)
      {
        PriID = mysql_insert_id(con);
      }
    }
    mysql_free_result(result);

    char Logtext[ZEILE];
    sprintf(Logtext, "> neues Event: KeyID '%d' = '%s' @ %s %s\n", PriID, thisEvent, thisDatum, thisZeit);
    strcat(MailBody, Logtext);

    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "   %s()#%d: KeyID '%d' = '%s'\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, PriID, thisEvent);
      #undef MELDUNG
    } // --------------------------------------------------------------
  }

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "   %s()#%d: Datensatz-ID = '%i'\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, PriID);
    #undef MELDUNG
   	{ // --- Log-Ausgabe ---------------------------------------------------------
			char LogText[ZEILE];  sprintf(LogText, " %d     - %s: %ld msec", myPID, __FUNCTION__, Zwischenzeit(T_EVENT));
      MYLOG(LogText);
    }
  } // --------------------------------------------------------------

  DEBUG_d("<- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__ , PriID);
  return(PriID);
}
//***********************************************************************************************

// Nachricht über FIFO senden
// --------------------------
bool toFIFO (char* inhalt)
{
  DEBUG_1("=> %s()#%d: %s(%s)\n",
             __FUNCTION__, __LINE__, __FUNCTION__, inhalt);
  int fd=0;
  int status=0;

  { // --- Debug-Ausgaben --------------------------------------------------------
    #define MELDUNG   "%s()#%d: open(%s, >%c<)\n"
    DEBUG_1(MELDUNG, __FUNCTION__, __LINE__, FIFO, (O_WRONLY | O_NONBLOCK));
    #undef MELDUNG
  } // ---------------------------------------------------------------------------

  fd = open(FIFO, O_WRONLY | O_NONBLOCK);       // Sender: Write Only
  if (fd < 0)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    if (errsv == 6)                             // 'No such device or address'
    { // 'FifoMotion' noch nicht bereit !
      // --------------------------------
      // nur Blinksignal
      #define ANZEIT  333 /* msec */
      for (int ix = 12; ix <=0; ix--)
      {
        digitalWrite (LED_rt, LED_EIN);
        delay(ANZEIT);
        digitalWrite (LED_rt, LED_AUS);
        delay(ANZEIT);
      }
      return false;
      #undef ANZEIT
    }
    else
    {
      sprintf(ErrText,"Error open('%s') = %i(%s)",
                    FIFO, errsv, strerror(errsv));
      showMain_Error( ErrText,__FUNCTION__,__LINE__);
    }
  }

  { // --- Debug-Ausgaben --------------------------------------------------------
    #define MELDUNG   "%s()#%d: '%s'"
    DEBUG_1(MELDUNG, __FUNCTION__, __LINE__, inhalt);
    #undef MELDUNG
  } // ---------------------------------------------------------------------------

  status = write (fd, inhalt, strlen(inhalt) + 1);
  if (status < 0)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText,"Error write('%s') = %i(%s)",
                    FIFO, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }
  DEBUG_1("<- %s()#%d -<0>- \n",  __FUNCTION__, __LINE__);
  return 0;
}
//***********************************************************************************************
//                                                                                              *
//                                    main()                                                    *
//                                                                                              *
//***********************************************************************************************

int main(int argc, char* argv[])
{
  char thisPfad[ZEILE] = {'\0'};                  // aktueller Verzeichnispfad

  sprintf (Version, "Vers. %d.%d.%d - %s", MAXVERS, MINVERS, BUILD, __DATE__);
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 ); // Verbindung zum Dämon Syslog aufbauen
  SYSLOG(LOG_NOTICE, ">>>>>> %s - %s - PID %d - User %d/%d - Group %d/%d <<<<<<",
                PROGNAME, Version, getpid(), geteuid(),getuid(), getegid(),getgid());
  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, 
       ">>>> %s - PID %d, User %d/%d, Group %d/%d, Anzahl Argumente: '%d'",
        Version, getpid(), geteuid(),getuid(), getegid(),getgid(), argc);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // Argument-Prüfung
  // -------------------------------------------
  { // --- Debug-Ausgaben -------------------------------------------
    #define MELDUNG ">> %s()#%d @ %s - Anzahl Argumente: '%d'%c"  
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, __NOW__, argc, '\n');
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, __NOW__, argc, '\0');
    #undef MELDUNG
  } // ---------------------------------------------------------------
  if (argc <= 1)
  {     // -- Error
    printf("   - Anzahl Argumente '%d'\n", argc);
    printf("   - Aufruf: \"%s\": Exit !\n", *argv);
    syslog(LOG_NOTICE, ">> %s()#%d -- Anzahl Argumente '%d': Exit!",
                                      __FUNCTION__, __LINE__, argc);
    {// --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, 
         "<<< Anzahl Argumente '%d': Exit!",  argc);
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
    exit (EXIT_FAILURE);
  } 
  
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-value"
  { // Verzeichnispfad ermitteln
    // -------------------------
    *argv++;
    char* pfad = thisPfad;
    char* ptr = (char*) *argv;
    char* end = strrchr(ptr, '/');
    if (end != NULL)
    {
      while (ptr <= end)
      {
        *pfad++ = *ptr++;
      }
    }
    { // --- Debug-Ausgaben -------------------------------------------
      #define MELDUNG ">> %s()#%d - Daten-Verzeichnis '%s'%c"  
      DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad, '\n');
      SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, thisPfad, '\0');
      #undef MELDUNG
    } // ---------------------------------------------------------------
    *argv--;
  }
  #pragma GCC diagnostic pop
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);

	
	enum Src Herkunft = UNDEF;
	if (strstr(argv[1], SOURCE1))
	{ 
		Herkunft = PIX;
	}
	else if (strstr(argv[1], SOURCE2))       
	{ 
		Herkunft = USB;
	}
  DEBUG(">> %s()#%d - Herkunft '%d'-> '%s'\n", 
                               __FUNCTION__, __LINE__, Herkunft, argv[1]);


  for (int ix=0; ix < argc; ix++)
  {
    { // --- Debug-Ausgaben -------------------------------------------
      #define MELDUNG ">> %s()#%d --- Argument %d: %s%c"  
      DEBUG(MELDUNG, __FUNCTION__, __LINE__, ix, argv[ix], '\n');
      SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, ix, argv[ix], '\0');
      #undef MELDUNG
    } // ---------------------------------------------------------------
//    DEBUG(">> %s()#%d   - Argument %d: %s\n", 
//                                      __FUNCTION__, __LINE__, ix, argv[ix]);
  }
  { // --- Debug-Ausgaben -------------------------------------------
    #define MELDUNG ">> %s()#%d   ------ Argumentlist done ------%c"  
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, '\n');
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, '\0');
    #undef MELDUNG
  } // ---------------------------------------------------------------

  // Prozess-ID ablegen  
  // ------------------
  myPID = savePID(FPID);
  { // --- Debug-Ausgaben -------------------------------------------
    #define MELDUNG ">> %s()#%d: meine PID: '%d'%c"  
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, myPID, '\n');
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, myPID, '\0');
    #undef MELDUNG
  } // ---------------------------------------------------------------
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);
  {// --- Log-Ausgabe ---------------------------------------------------------
		char LogText[ZEILE];  sprintf(LogText, "     %d - '%s'", myPID, argv[1]);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // Ist GPIO klar?
  // -------------------------------------------
  wiringPiSetup();
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);
  pinMode (LED_rt,    OUTPUT);
  pinMode (LED_ge1,   OUTPUT);
  pinMode (LED_gn1,   OUTPUT);
  pinMode (LED_bl1,   OUTPUT);
  pullUpDnControl (LED_rt,   PUD_UP) ;
  pullUpDnControl (LED_ge1,  PUD_UP) ;
  pullUpDnControl (LED_gn1,  PUD_UP) ;
  pullUpDnControl (LED_bl1,  PUD_UP) ;
  {
		#define INTERVALL  33 /* msec */
    for (int ix=12; ix > 0; ix--)
    {
			ChannelBlink(Herkunft, INTERVALL);
    }
    ChannelLED(Herkunft, LED_EIN);
  	#undef INTERVALL
  }

  { // --- Debug-Ausgaben -------------------------------------------
    #define MELDUNG ">> %s()#%d: GPIO OK%c"  
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, '\n');
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, '\0');
    #undef MELDUNG
  } // ---------------------------------------------------------------

//	printf("- #%d\n", __LINE__);

  // wenn keine Daten: hier beenden
  // -------------------------------
  if (argc <= 2)
  {     // -- Error
    printf("   - Anzahl Argumente '%d': Exit!\n", argc);
    printf("   - Aufruf: %s'\n", *argv);
    syslog(LOG_NOTICE, ">> %s()#%d -- Anzahl Argumente '%d': Exit!\n",
                                      __FUNCTION__, __LINE__, argc);
    {// --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, 
         "<<< Anzahl Argumente '%d': Exit!",  argc);
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
 		digitalWrite (LED_gn1, LED_AUS);
 		digitalWrite (LED_ge1, LED_AUS);
    exit (EXIT_FAILURE);
  }
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);


	// gespeicherte Eventnummer holen
	// ------------------------------
	EventNummer = getEventNummer();
  {// --- Log-Ausgabe ---------------------------------------------------------
		char LogText[ZEILE];  sprintf(LogText, "     %d - EventNummer: %d", myPID, EventNummer);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------
	saveEventNummer(EventNummer+1);


  // Datenbank und Tabellen erzeugen, wenn noch nicht vorhanden
  // ----------------------------------------------------------
  DEBUG(">> %s()#%d - Datenbank erzeugen\n", __FUNCTION__, __LINE__);
  MYSQL* con = NULL;
  con = CreateDB(con);                          // Verbindung zur Datenbank
  CreateTables(con);
  { // --- Debug-Ausgaben -------------------------------------------
    #define MELDUNG ">> %s()#%d: Datenbank OK\n"  
    DEBUG(MELDUNG, __FUNCTION__, __LINE__);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__);
    #undef MELDUNG
  } // ---------------------------------------------------------------
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);


  // Zugriffsrechte
  // --------------
  int bits[] =
  {
    S_IRUSR,S_IWUSR,S_IXUSR,   /* Zugriffsrechte User    */
    S_IRGRP,S_IWGRP,S_IXGRP,   /* Zugriffsrechte Gruppe  */
    S_IROTH,S_IWOTH,S_IXOTH    /* Zugriffsrechte der Rest */
  };
  destroyInt(*bits);
  SYSLOG(LOG_NOTICE, ">> %s()#%d", __FUNCTION__, __LINE__);
  
  // Start im Log vermerken
  // ----------------------
  char Logtext[2*ZEILE];
  sprintf(Logtext, ">>> %s()#%d - Init OK ------- %d Elemente", __FUNCTION__, __LINE__, argc);
  syslog(LOG_NOTICE, Logtext);
  DEBUG("%s\n\n", Logtext);
  goto Phase_1;

Phase_1:
//* ===== Phase 1 =================================================================================
//*
//* alle Dateien im Verzeichnis nach Events sortieren und in Event-Verzeichnisse verschieben:
//*  while Dateizahl größer 0
//*  {
//*    avi-Datei finden
//*    daraus Event machen / Verzeichnis
//*    wenn bereits Event-Verzeichnis
//*       Event + 1
//*    alle jpg-Dateien dieses Events in das Verzeichnis
//*  }

//  digitalWrite (LED_ge1, LED_AUS);
//  digitalWrite (LED_gn1, LED_EIN);
//  int newFolder = 0;                  // Verzeichniszähler
//  int newFiles = 0;                   // Dateizähler

  { // --- Debug-Ausgaben ----------------------------------------------------------------------
    #define MELDUNG "\n%s()#%d: ==== Phase 1: Dateien in '%s' ordnen ================================\n\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    #undef MELDUNG
  }// -------------------------------------------------------------------------------------------

	ChannelBlink(Herkunft, BLINK);
  Startzeit(T_GESAMT);                // Zeitmessung über alles starten
  Startzeit(T_ABSCHNITT);             // Zeitmessung Phase 1 starten

  // alle Elemente im Verzeichnis durchlaufen
  // ----------------------------------------
  { // --- Debug-Ausgaben ----------------------------------------------------------------------
    #define MELDUNG   "%s()#%d: ---- alle Elemente im '%s' durchlaufen -----\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    #undef MELDUNG
  } // -----------------------------------------------------------------------------------------

  char** DateiListe = argv;        		// die übergebene Dateiliste
  int newFolder = 0;                  // Verzeichniszähler
  int newFiles = 0;                   // Dateizähler

  while(*++DateiListe)
  {
    char* thisDatei = *DateiListe; 		// eine einzelne Datei
    int thisFiletype = getFiletyp(thisDatei);

    // nächste Filmdatei suchen
    // ------------------------
    if ((thisFiletype == AVI) || (thisFiletype == MKV))
    {
      // Dies ist ein Film: Ereignis-Nummer ermitteln
      // --------------------------------------------
      int thisEventnummer = getEventNo(thisDatei);

      { // -----------------------------------------------------------
        #define MELDUNG   "\n%s()#%d: - Eventnummer '%s': %d\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisDatei, thisEventnummer);
        #undef MELDUNG
      } // -----------------------------------------------------------

      // ein neues Verzeichnis anlegen
      // -----------------------------
      char thisFolder[ZEILE];                   // neues Verzeichnis
      int Eventnummer = thisEventnummer;
      while (makeEventfolder(thisPfad, Eventnummer, thisFolder) == FOLDER_EXIST)
      {
        Eventnummer += 1;
      }
      newFolder++;
      // alle Dateien mit diesem Ereignis in dieses Verzeichnis verschieben
      // -------------------------------------------------------------------
      char** DateiListe = argv;                 // nochmals die übergebene Dateiliste
      while(*++DateiListe)
      {
        char* myDatei = *DateiListe;            // eine einzelne Datei
        int myEventnummer = getEventNo(myDatei);
        if (myEventnummer == thisEventnummer)
        {
          { // -----------------------------------------------------------
            #define MELDUNG   "%s()#%d: '%s' Ev(%d) -> '%s'\n"
            DEBUG(MELDUNG, __FUNCTION__, __LINE__, myDatei, myEventnummer, thisFolder);
            #undef MELDUNG
          } // -----------------------------------------------------------

          MoveFile( myDatei, thisFolder);                   // Datei verschieben
          newFiles++;
        }
      }
      { // -----------------------------------------------------------
        #define MELDUNG   "%s()#%d: --  Eventnummer '%d' fertig --'\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisEventnummer);
        #undef MELDUNG
      } // -----------------------------------------------------------
    }
  }
// ===== Phase 1 beendet===========================================================================

  { // --- Debug-Ausgaben ---------------------------------------
    #define MELDUNG   "\n%s()#%d: ---- Phase 1 fertig: %d Folders, %d Files in %ld msec --------------------------------------\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, newFolder, newFiles, Zwischenzeit(T_ABSCHNITT));
    #undef MELDUNG
    {
			char LogText[ZEILE];  sprintf(LogText, "    %d - Phase 1 done in %ld msec", myPID, Zwischenzeit(T_ABSCHNITT));
      MYLOG(LogText);
    }
    // ---------------------------------------------------------- 
  }
  #if BREAKmain1
  { // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n\n%s()#%d:                           ------< weiter mit ENTER >------\n", __FUNCTION__, __LINE__);
    char dummy;
    scanf ("%c", &dummy);

  }
  #endif
  goto Phase_2;

Phase_2:
//* ===== Phase 2 ==================================================================================
//*
//* jetzt nochmal alle Dateien in allen Verzeichnissen durchsuchen:
//*
//*  alle Verzeichnisse
//*  {
//*    alle Dateien
//*    {
//*      zu kleine Dateien löschen
//*      Dateien zählen
//*      Speicher aufsummieren
//*    }
//*    neue Eventnummer = PrimID Datenbank
//*    Verzeichnis als Event in Datenbank
//*  }

	ChannelBlink(Herkunft, BLINK);
  Startzeit(T_ABSCHNITT);             // Zeitmessung Phase 2 starten
  float SizeTotal = 0;                // gesamte Speicherbelegung

  { // --- Debug-Ausgaben ------------------------------------------------------------------------
    #define MELDUNG "\n%s()#%d: ==== Phase 2: nochmal alle Verzeichnisse in '%s' durchsuchen ========\n"
    DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, thisPfad);
    #undef MELDUNG
  }
  // -------------------------------------------------------------------------------------------

  DIR* pdir = opendir(thisPfad);                // das oberste Verzeichnis
  struct dirent* pdirzeiger;
  if (pdir == NULL)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText,"Error Verzeichnis '%s': %i(%s)",
                    thisPfad, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }

  // alle Unter-Verzeichnisse durchsuchen
  // ------------------------------------
  // int cnt = 0; --> siehe '#if __SQLMOTION_DEBUG__2'
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char myPath[ZEILE];                                   // Name des aktuellen Unterverzeichnisses
    struct stat myAttribut;
    sprintf(myPath,"%s%s", thisPfad, (*pdirzeiger).d_name);
    if (stat(myPath, &myAttribut) == -1)                  // Datei-Attribute holen
    { // -- Error
      char ErrText[ERRBUFLEN];                            // Buffer für Fehlermeldungen
      sprintf(ErrText,"Attribut '%s' nicht lesbar", myPath);
      showMain_Error( ErrText,__FUNCTION__,__LINE__);
    }
    unsigned long myFileSize = myAttribut.st_size;        // Dateilänge
    { // --- Debug-Ausgaben ---------------------------------------------- 
      #define MELDUNG "\n%s()#%d: %d. ---  '%s': %ld Bytes ---\n"
      DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, cnt++, myPath, myFileSize);
      #undef MELDUNG
      // -----------------------------------------------------------------
    }
    SizeTotal += myFileSize;
    
    if (strstr(myPath, _FOLDER) != NULL)                  // ist dies ein Eventverzeichnis?
    { // -- ja! Dies ist ein Eventverzeichnis
      // -------------------------------------
      { // --- Debug-Ausgaben ---------------------------------------------- 
        #define MELDUNG "\n%s()#%d: --- Verzeichnis '%s' durchsuchen ---\n"
        DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, myPath);
        #undef MELDUNG
      // ------------------------------------------------------------------
      }

      Startzeit(T_FOLDER);            // Zeitmessung starten
      DIR* udir = opendir(myPath);    // das Eventverzeichnis öffnen
      struct dirent* udirzeiger;
      if (udir == NULL)
      { // -- Error
        char ErrText[ERRBUFLEN];      // Buffer für Fehlermeldungen
        int errsv = errno;
        sprintf(ErrText,"Error Verzeichnis '%s': %i(%s)",
                        myPath, errsv, strerror(errsv));
        showMain_Error( ErrText,__FUNCTION__,__LINE__);
      }
      // alle Dateien in diesem Eventverzeichnis durchsuchen
      // -----------------------------------------------------
      unsigned long SizeFolder=0;               // Speicherbelegung dieses Unterverzeichnisses
      int cntJPGs = 0;                          // Anzahl Bilder
      int cntAVIs = 0;                          // Anzahl Filme (AVI und MKV)
      time_t EventDatum = -99999999;            // Ereignisdatum: das höchst Dateidatum 
      while((udirzeiger=readdir(udir)) != NULL) // Zeiger auf den Inhalt diese Unterverzeichnisses
      {
        Startzeit(T_FILES);                     // Zeitmessung starten
        char myFilename[ZEILE];
        strcpy(myFilename, (*udirzeiger).d_name);
        int myType   = (*udirzeiger).d_type;

        { // --- Debug-Ausgaben ------------------------------------------------------------------------
          #define MELDUNG   "%s()#%d: Datei '%s' untersuchen\n"
          DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, myFilename);
          #undef MELDUNG
          DEBUG_2("             reclen    %d\n",  (*udirzeiger).d_reclen);
          DEBUG_2("             type      %d\n",  myType);
          DEBUG_2("             offset    %d\n",  (int)(*udirzeiger).d_off);
          DEBUG_2("             inode     %d\n",  (int)(*udirzeiger).d_ino);
          // file attributes such as size, modification times etc., are part of the file itself,
          // not of any particular directory entry. See File Attributes.
        } // ----------------------------------------------------------------------

        if (myType == DT_REG)           //  struct dirent ('dirent.h'): A regular file.
        { // reguläre Datei
          // --------------
          struct stat myAttribut;
          char longFilename[ZEILE];
          sprintf(longFilename,"%s/%s", myPath, myFilename);
          if (stat(longFilename, &myAttribut) == -1)      // Datei-Attribute holen
          { // -- Error
            char ErrText[ERRBUFLEN];                      // Buffer für Fehlermeldungen
            sprintf(ErrText,"Lesefehler Datei '%s'!",
                            longFilename);
            showMain_Error( ErrText,__FUNCTION__,__LINE__);
          }
          
          time_t myFileDatum = myAttribut.st_mtime;       // Datum der Datei
            DEBUG_2("             FileDate  %ld\n",  myFileDatum);
          unsigned long myFileSize = myAttribut.st_size;  // Dateilänge
            DEBUG_2("             FileSize  %ld\n",  myFileSize);
          // etwas Statistik
          // ---------------------------------------
          int myType = getFiletyp(longFilename);
            DEBUG_2("             FileType  %d\n",  myType);
            DEBUG_2("              minimum  %d\n",  MIN_FILESIZE);
            DEBUG_2("                 JPG?  %d\n",  JPG);
          EventDatum = myFileDatum > EventDatum ? myFileDatum : EventDatum;
          SizeFolder += myFileSize;
          if (myType == JPG) cntJPGs++;
          else if (myType == AVI) cntAVIs++;
          else if (myType == MKV) cntAVIs++;
          // zu kurze Bild-Dateien aussortieren
          // ---------------------------------------
          if ((myFileSize < MIN_FILESIZE) && (myType == JPG))
          { // Bilddatei zu klein
            // ------------------
            { // --- Debug-Ausgaben ---------------------------------------------------
              #define MELDUNG   "%s()#%d: -- Datei '%s' zu kurz: %ld!\n"
              DEBUG_2(MELDUNG, __FUNCTION__, __LINE__,  longFilename, myFileSize);
              #undef MELDUNG
            } // ----------------------------------------------------------------------

            // Datei löschen
            // ------------*
            remove(longFilename);
          }
          else
          {
            SizeTotal += myFileSize;
          }
        }
        else
        { // keine reguläre Datei
          // ---------------------
          SizeTotal += myFileSize;
          DEBUG_2("             .........%s\n", myFilename);
        } // ---- Ende reguläre Datei
				ChannelBlink(Herkunft, BLINK/3);
      }  // --- Ende Eventverzeichnis durchsuchen T_FILES

      { // --- Zeitmessung Dateien prüfen ---------------------------------------
        #define MELDUNG   "%s()#%d: ------ Zwischenzeit >Dateien pruefen<: '%ld' msec ------\n"
        DEBUG_z(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_FILES));
        #undef MELDUNG
        { // --- Log-Ausgabe ---------------------------------------------------------
    			char LogText[ZEILE];  sprintf(LogText, "    %d   - Phase 2: Check Files in %ld msec", myPID, Zwischenzeit(T_FILES));
          MYLOG(LogText);
        }
      } // ---------------------------------------------------------------

      // das Eventverzeichnis in der Datenbank vermerken
      // ------------------------------------------------
      struct stat dAttribut;
      char EventKey[ZEILE] = {'\0'};                               	// Eventname
      char Remark[ZEILE] = {'\0'};                                  // Bemerkung

      Startzeit(T_DBASE);                                           // Zeitmessung starten

      if(stat(myPath, &dAttribut) == -1)
      { // -- Error
        char ErrText[ERRBUFLEN];                                    // Buffer für Fehlermeldungen
        sprintf(ErrText,"Verzeichnis '%s' nicht lesbar", myPath);
        showMain_Error( ErrText,__FUNCTION__,__LINE__);
      }

//      time_t dFaDatum = dAttribut.st_mtime;                       // Datum des Verzeichnisses
      int PrimaryID = -1;                                        		// PrimärID des Datensatzes
      sprintf( Remark, "JPGs=%i - AVIs=%i - Mem: %i kB - %s", 			// Bemerkungs-Text
                      cntJPGs,  cntAVIs, (int)(SizeFolder+1024/2)/1024, Herkunft==PIX ? SOURCE1 : SOURCE2);      
      { // --- Debug-Ausgaben ---------------------------------------------- 
        #define MELDUNG "%s()#%d: \n"
        DEBUG_2(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
        // -----------------------------------------------------------------
      }
      if (getEventKey(con, myPath, EventKey) == true)               // Eventnamen für Schlüssel erzeugen
      { // in die Datenbank eintragen
        // ----------------------------
        { // --- Debug-Ausgaben ---------------------------------------------- 
          #define MELDUNG "%s()#%d: === in die Datenbank ===> EventKey: '%s'\n"
          DEBUG_2(MELDUNG, __FUNCTION__, __LINE__, EventKey);
          #undef MELDUNG
          // -----------------------------------------------------------------
        }
       	{ // --- Log-Ausgabe ---------------------------------------------------------
    			char LogText[ZEILE];  sprintf(LogText, "    %d   - Phase 2: Zwischenzeit %ld msec", myPID, Zwischenzeit(T_DBASE));
          MYLOG(LogText);
        }
        PrimaryID = AddEvent(con, EventKey, &EventDatum, SizeFolder, Remark);
        { // Eventkey als Datei
          // -------------------
      		Startzeit(T_FILE);                              					// Zeitmessung starten
          FILE *Datei;  
          char KeyFile[ZEILE];
          if (Herkunft == PIX)
           	sprintf(KeyFile,"%s/%s-pix.info", myPath, EventKey); 
          else if (Herkunft == USB)
           	sprintf(KeyFile,"%s/%s-usb.info", myPath, EventKey); 
          else
           	sprintf(KeyFile,"%s/%s-error.info", myPath, EventKey); 
          Datei = fopen (KeyFile, "w");
          fprintf(Datei, "%s %s\r\n", PROGNAME, Version);
          fprintf(Datei, "   Source = %s\r\n", argv[1]); 
          fprintf(Datei, "PrimaryID = %d\r\n", PrimaryID);
          fprintf(Datei, " EventKey = '%s'\r\n", EventKey);
          fclose (Datei);
         	{ // --- Log-Ausgabe ---------------------------------------------------------
      			char LogText[ZEILE];  sprintf(LogText, "    %d     - Info-File '%s' in %ld msec", myPID, KeyFile, Zwischenzeit(T_FILE));
            MYLOG(LogText);
          }
        }
       	{ // --- Log-Ausgabe ---------------------------------------------------------
    			char LogText[ZEILE];  sprintf(LogText, "    %d   - Phase 2: Zwischenzeit %ld msec", myPID, Zwischenzeit(T_DBASE));
          MYLOG(LogText);
        }
      }
      else
      { // -- Error
        char ErrText[ERRBUFLEN];                                    // Buffer für Fehlermeldungen
        sprintf(ErrText,"Error '%s' --> '%s'", myPath, EventKey);
        showMain_Error( ErrText,__FUNCTION__,__LINE__);
      }
      { // --- Debug-Ausgaben ---------------------------------------------- 
        #define MELDUNG "%s()#%d: \n"
        DEBUG_2(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
       	{ // --- Log-Ausgabe ---------------------------------------------------------
    			char LogText[ZEILE];  sprintf(LogText, "    %d   - Phase 2: to Database in %ld msec", myPID, Zwischenzeit(T_DBASE));
          MYLOG(LogText);
        }

        // -----------------------------------------------------------------
      }
      // Aus der PrimärID einen neuen Event-Namen erzeugen
      // -------------------------------------------------
      {
        char neuerEventName[ZEILE] = {'\0'};
        sprintf(neuerEventName, "%s/%s%d", thisPfad, _EVENT_, PrimaryID);
        if( (rename(myPath, neuerEventName)) < 0)
        { // -- Error
          char ErrText[ERRBUFLEN];                  // Buffer für Fehlermeldungen
          int errsv = errno;
          sprintf(ErrText, "Error rename '%s' ->  '%s'- = %i(%s)",
                           myPath, neuerEventName, errsv, strerror(errsv));
          showMain_Error( ErrText,__FUNCTION__,__LINE__);
        }
      }
    }     // --- ist dies ein Eventverzeichnis? -------------------
  }

  // Lesezeiger wieder schließen
  // ---------------------------
  if(closedir(pdir) == -1)
  { // -- Error
    char ErrText[ERRBUFLEN];                    // Buffer für Fehlermeldungen
    int errsv = errno;
    sprintf(ErrText,"Error Verzeichnis '%s' - = %i(%s)",
                    thisPfad, errsv, strerror(errsv));
    showMain_Error( ErrText,__FUNCTION__,__LINE__);
  }

  { // --- Debug-Ausgaben ---------------------------------------
    #define MELDUNG   "\n%s()#%d: ==== Phase 2 fertig in %ld msec ---------------------------------------------------------\n"
    DEBUG_z(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_ABSCHNITT));
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_ABSCHNITT));
    #undef MELDUNG
  } // ----------------------------------------------------------

  mysql_close(con);                             // Datenbank schließen
	saveEventNummer(EventNummer);									// Eventnummer wieder speichern

// ===== Phase 2 beendet===========================================================================

  char sTotal[50] = {'\0'};
  sprintf(sTotal, "%3.1f MB", (SizeTotal+((1024*1024)/2))/(1024*1024));
  sprintf(Logtext, ">>> %s()#%d: Speicherbelegung '%s': %s\n", __FUNCTION__,__LINE__, thisPfad, sTotal);
  SYSLOG(LOG_NOTICE, Logtext);
  strcat(MailBody, Logtext);

  { // --- Debug-Ausgaben ---------------------------------------
    #define MELDUNG   "%s()#%d: Speicherbelegung '%s': %s\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, thisPfad, sTotal);
    #undef MELDUNG
    { // --- Log-Ausgabe ---------------------------------------------------------
			char LogText[ZEILE];  sprintf(LogText, "    %d - Phase 2 done in %ld msec", myPID, Zwischenzeit(T_ABSCHNITT));
      MYLOG(LogText);
    }
  } // ----------------------------------------------------------

  #if BREAKmain2
  { // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n%s()#%d:                           ------< weiter mit ENTER >------\n", __FUNCTION__, __LINE__);
    char dummy;
    scanf ("%c", &dummy);
  }
  #endif

//* ===== Phase 3 ==================================================================================
//*
//* die Ereignisse in den externen Buffer übertragen:
//*  die Daten werden über einen Fifo weitergereicht.
//*

  { // --- Debug-Ausgaben ------------------------------------------------------------------------
    #define MELDUNG "\n%s()#%d: ==== Phase 3: Dateien per Fifo übertragen =============================================\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__);
    SYSLOG(LOG_NOTICE, MELDUNG, __FUNCTION__, __LINE__);
    #undef MELDUNG
  }

  Startzeit(T_ABSCHNITT);             // Zeitmessung Phase 3 starten

  digitalWrite (LED_gn1, LED_AUS);
  digitalWrite (LED_ge1, LED_AUS);
  digitalWrite (LED_bl1, LED_EIN);
  toFIFO(thisPfad);                   // >>>>>>>>> über FIFO (named pipe) senden >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  digitalWrite (LED_rt, LED_AUS);
  SYSLOG(LOG_NOTICE, ">>> %s()#%d: %s --- done in %ld msec", __FUNCTION__,__LINE__, PROGNAME, Zwischenzeit(T_GESAMT));


  { // --- Debug-Ausgaben ---------------------------------------
    #define MELDUNG   "%s()#%d: ---- Phase 3 fertig in %ld msec ---------------------------------------------------------\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_ABSCHNITT));
    #undef MELDUNG
    { // --- Log-Ausgabe ---------------------------------------------------------
			char LogText[ZEILE];  sprintf(LogText, "    %d - Phase 3 done in %ld msec", myPID, Zwischenzeit(T_ABSCHNITT));
      MYLOG(LogText);
    }
    #define MELDUNG   "%s()#%d: ---- Gesamtzeit: %ld msec -----\n"
    DEBUG(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_GESAMT));
    #undef MELDUNG
    { // --- Log-Ausgabe ---------------------------------------------------------
			char LogText[ZEILE];  sprintf(LogText, "    %d - Total done in %ld msec", myPID, Zwischenzeit(T_GESAMT));
      MYLOG(LogText);
    }
  } // ----------------------------------------------------------

  closelog();
  return EXIT_SUCCESS;
}
//************************************************************************************//
