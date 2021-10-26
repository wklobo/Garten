//***************************************************************************//
//*                                                                         *//
//* File:          fifomotion.c                                             *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2014-08-23  --  2016-02-04;                              *//
//* Last change:   2021-10-24 - 17:18:20                                    *//
//* Description:   Weiterverarbeitung von 'motion'-Dateien:                 *//
//*                kopieren auf einen anderen Rechner                       *//
//*                                                                         *//
//* Copyright (C) 2014-21 by Wolfgang Keuch                                 *//
//*                                                                         *//
//* Kompilation:                                                            *//
//*    gcc -o Fifo fifomotion.c                                             *//
//*    make fifo                                                            *//
//*                                                                         *//
//* Aufruf:                                                                 *//
//*    ./FifoMotion &    (Daemon)                                           *//
//*                                                                         *//
//***************************************************************************//

#define _MODUL0
#define __FIFOMOTION_MYLOG__         true
#define __FIFOMOTION_DEBUG__         true
#define __FIFOMOTION_DEBUG__c__      false    // calcSize
#define __FIFOMOTION_DEBUG__t__      true    // FileTransfer
#define __FIFOMOTION_DEBUG__d__      false    // delOldest


#include "./version.h"
#include "./fifomotion.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <wiringPi.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../error.h"
#include "../datetime.h"
#include "../../sendmail/sendMail.h"

#define FLAGS   O_WRONLY|O_CREAT|O_EXCL
#define MODE    S_IRWXU|S_IRWXG|S_IROTH
#define MODUS  0774

// statische Variable
// ----------------
static char Hostname[ZEILE];          // der Name dieses Rechners
static char meineIPAdr[NOTIZ];        // die IP-Adresse dieses Rechners
static char* IPnmr;                   // letzte Stelle der IP-Adresse
static time_t ErrorFlag = 0;          // Steuerung rote LED

static int newFiles   = 0;            // neue Dateien
static int newFolders = 0;            // neue Verzeichnisse
static int delFiles   = 0;            // gelöschte Dateien
static int delFolders = 0;            // gelöschte Verzeichnisse

//***************************************************************************//
/*
 * Define debug functions.
 * ----------------------
 */
#if __FIFOMOTION_MYLOG__
#define MYLOG(...)  MyLog(PROGNAME, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define MYLOG(...)
#endif

// main()
// ------
#if __FIFOMOTION_DEBUG__
 #define DEBUG(...)  printf(__VA_ARGS__)
 #define BREAKmain   true /* alle Dateien bearbeitet */
 #define SYSLOG(...)  syslog(__VA_ARGS__)
#else
 #define DEBUG(...)
 #define SYSLOG(...)
#endif

#if __FIFOMOTION_DEBUG__1__
#define DEBUG_1(...)  printf(__VA_ARGS__)
#else
#define DEBUG_1(...)
#endif

#if __FIFOMOTION_DEBUG__c__
#define DEBUG_c(...)  printf(__VA_ARGS__)
#else
#define DEBUG_c(...)
#endif

// FileTransfer
// -------------
#if __FIFOMOTION_DEBUG__t__
#define DEBUG_t(...)  printf(__VA_ARGS__)
#define BREAK_FileTransfer1   false   /* FileTransfer  */
#define BREAK_FileTransfer2   false   /* FileTransfer  */
#else
#define DEBUG_t(...)
#endif

// delOldest
// ---------
#if __FIFOMOTION_DEBUG__d__
#define DEBUG_d(...)  printf(__VA_ARGS__)
#define BREAK_delOldest1   false      /* delOldest  */
#define BREAK_delOldest2   false      /* delOldest  */
#else
#define DEBUG_d(...)
#endif

//***************************************************************************//

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
  digitalWrite (LED_GELB,   LED_AUS);
  digitalWrite (LED_GRUEN,  LED_AUS);
  digitalWrite (LED_BLAU,   LED_AUS);
  digitalWrite (LED_ROT,    LED_EIN);

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText,
       "<<< %s: Exit!",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

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

  sprintf( ErrText, ">>> %s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Fehler);

  DEBUG("   -- Fehler -->  %s\n", ErrText);     // lokale Fehlerausgabe

  digitalWrite (LED_ROT,    LED_EIN);
  ErrorFlag = time(0) + BRENNDAUER;             // Steuerung rote LED

  if (errsv == 24)                              // 'too many open files' ...
    report_Error(ErrText, false);               // Fehlermeldung ohne Mail ausgeben
//    report_Error(ErrText, true);                // Fehlermeldung mit Mail ausgeben   --- vorläufig
  else
    report_Error(ErrText, false);               // Fehlermeldung ohne Mail ausgeben

  {// --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText,
       "<<< %s",  ErrText);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  return errsv;
}
//***********************************************************************************************

// Protokoll gelöschte Dateien schreiben
// --------------------------------------
int Deleted(const char* ItemName)
{
  DEBUG_d("=======> %s()#%d: %s('%s')\n", __FUNCTION__, __LINE__, __FUNCTION__, ItemName);
  int status = 1;

  { // --- Log-Ausgabe --------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText,
       "    gelöscht: '%s'",  ItemName);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  DEBUG_d("<------- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// Protokoll neue Dateien schreiben
// --------------------------------
int Added(const char* ItemName)
{
  DEBUG_d("=======> %s()#%d: %s('%s')\n", __FUNCTION__, __LINE__, __FUNCTION__, ItemName);
  int status = 1;

  { // --- Log-Ausgabe --------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText,
       "           neu: '%s'",  ItemName);
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  DEBUG_d("<------- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// aus Dateinamen einen Datums-Verzeichnisnamen erstellen
// ------------------------------------------------------
// Rückgabe: FolderName 'YYYY-MM-DD'

int makeDatumsFoldername(const time_t Zeit, char* FolderName)
{
  DEBUG_t("===> %s()#%d:  %s('%ld',\n"\
         "                                                   ----> '%s')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Zeit, FolderName);
  int status = 0;
  struct tm *tmnow;
  tmnow = localtime(&Zeit);
  sprintf(FolderName, "%04d-%02d-%02d",
                      tmnow->tm_year + 1900, tmnow->tm_mon + 1, tmnow->tm_mday);

  { // --- Debug-Ausgaben ------------------------------------------
    DEBUG_t("     %s()#%d: Name '%s' erstellt!\n",
                         __FUNCTION__, __LINE__, FolderName);
  } // --------------------------------------------------------------

  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, status);
  return status;
}
//***********************************************************************************************

// aus Dateinamen einen Event-Verzeichnisnamen erstellen
// ------------------------------------------------------------
//  '.../09-20210107_132755.mkv' --> 'Kamera_Event-09'

int makeEventFoldername(const char* FileName, const char* Vorspann, char* FolderName)
{
  DEBUG_t("===> %s()#%d:  %s('%s',\n"\
         "                                                       '%s',\n"\
         "                                                 ----> '%s')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, FileName, Vorspann, FolderName);

  strcat(FolderName, Vorspann);
  strcat(FolderName, "_");
  strcat(FolderName, "Event-");
  char* beg = strrchr(FileName, '/');           // Event-Markierung suchen ...
  strncat(FolderName, beg+1, 2);                // Event-Nummer
  strcat(FolderName, "\0");

  { // --- Debug-Ausgaben ------------------------------------------
    DEBUG_t("     %s()#%d: Name '%s' erstellt!\n",
                         __FUNCTION__, __LINE__, FolderName);
  } // --------------------------------------------------------------

  int retval = 1;
  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//***********************************************************************************************

// Verzeichnis erstellen
// ----------------------
// neues Verzeichnis erstellen, wenn noch nicht vorhanden
int makeFolder(const char* Folder)
{
  DEBUG_t("===> %s()#%d: %s(%s)\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Folder);
  int errsave = 0;
  char ErrText[ERRBUFLEN];
  int verz = mkdir(Folder, MODUS);              // Datumsverzeichnis erstellen
  if (verz != 0)
  {  // -- Error
    errsave = errno;
    if (errsave != EEXIST)                      // ... der darf!
    { // -- Error
      sprintf(ErrText, "mkdir('%s')", Folder);
      showMain_Error(ErrText, __FUNCTION__, __LINE__);
    }
  }

  // --- Debug-Ausgaben ------------------------------------------
  if (errsave == 0)
  {
    DEBUG_t("     %s()#%d: Verzeichnis '%s' angelegt!\n",
                         __FUNCTION__, __LINE__, Folder);
    char TmpText[NOTIZ];
    sprintf(TmpText, "%s/", Folder);            // als Verzeichnis kennzeichnen
    newFolders += Added(TmpText);             // im Log vermerken
  }
  else
    DEBUG_t("     %s()#%d: Verzeichnis '%s' existiert!\n",
                         __FUNCTION__, __LINE__, Folder);
  // --------------------------------------------------------------


  int retval = (errsave == 0) ? 1 : 0;
  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//***********************************************************************************************

// Datei auf Alter püfen und ggf. löschen
// ------------------------------------------
//  Dateiname: ganzer Pfad
//  maxAlter:  Dateialter in Stunden
int remFile(const char* Dateiname, int maxAlter)
{
  DEBUG_d("=====> %s()#%d: %s('%s', '%d')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Dateiname, maxAlter);
  int deleted = 0;                              // Datei-Status
  struct stat Attribut;                         // Attribute der Datei
  char ErrText[ERRBUFLEN];

  if (stat(Dateiname, &Attribut) == -1)         // Attribute auslesen
  { // -- Error
    if (errno != 17)                            // ' Err 17-File exists': falscher Fehler
    {
      sprintf(ErrText, "Error read attribut('%s'): %d", Dateiname, errno);
      return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
    }
  }

  if(Attribut.st_mode & S_IFREG)
  {
    // Reguläre Datei
    // --------------
    time_t FcDatum = Attribut.st_mtime;         // Dateidatum
    long Alter = (time(NULL) - FcDatum);        // Alter der Datei [sec]

    { // --- Debug-Ausgaben ------------------------------------------
      int std = Alter / 3600;
      int min = (Alter % 3600) / 60;
      int sec = ((Alter % 3600) % 60);
      #define MELDUNG   "       %s()#%d: '%s':\n           "\
      "                                   Alter %3d:%02d:%02d Std - max: %d Std\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__,
                                   Dateiname, std, min, sec, maxAlter );
      #undef MELDUNG
      destroyInt(std);
      destroyInt(min);
      destroyInt(sec);
    } // --------------------------------------------------------------

    if (Alter > (maxAlter*3600))
    { // Datei nach Verfalldatum löschen
      // -------------------------------
      remove(Dateiname);
      { // --- Debug-Ausgaben ------------------------------------------
        DEBUG_d("       %s()#%d: --- '%s' gelöscht!\n",
                             __FUNCTION__, __LINE__, Dateiname);
      } // --------------------------------------------------------------
      delFiles += Deleted(Dateiname);       // im Log vermerken
      deleted++;
    }
  }
  DEBUG_d("<----- %s()#%d -<%d>- \n\n",  __FUNCTION__, __LINE__ , deleted);

  return deleted;
}
//***********************************************************************************************

// Verzeichnis  auf Alter püfen und ggf. löschen
// ---------------------------------------------
//  Foldername: ganzer Pfad
//  maxAlter:  Dateialter in Stunden
int remFolder(const char* Foldername, int maxAlter)
{
  DEBUG_d("===> %s()#%d: %s('%s', '%d')\n",
          __FUNCTION__, __LINE__, __FUNCTION__, Foldername, maxAlter);

  int deleted = 0;                              // Verzeichnis-Status
  int delFiles= 0;                              // Anzahl der gelöschten Dateien
  struct stat Attribut;                         // Attribute des Verzeichnisses
  char ErrText[ERRBUFLEN];
  char TmpText[ERRBUFLEN];

  // Prüfung auf Sytemdateien ( '.  ...' usw.)
  // ------------------------------------
  char* nam = strrchr(Foldername, '/')+1;       // letztes Glied im Namen
  if (*nam == '.')
  {
    DEBUG_d("<--- %s()#%d -!<%s>- \n",  __FUNCTION__, __LINE__ , nam);
    return deleted;
  }

  if (stat(Foldername, &Attribut) == -1)        // Attribute auslesen
  {
    if (errno != 17)                            // ' Err 17-File exists': falscher Fehler
    { // -- Error
      #if _DEBUGd
        sprintf(ErrText, "read attribut '%s'", Foldername);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      #else
        return(0);
      #endif
    }
  }

  if(Attribut.st_mode & S_IFDIR)
  {
    // Verzeichnis
    // --------------
    time_t FcDatum = Attribut.st_mtime;       // Dateidatum
    long Alter = (time(NULL) - FcDatum);      // Alter der Datei [sec]

    { // --- Debug-Ausgaben -------------------------------------------
      #define MELDUNG   "     %s()#%d: '%s':\n              "\
      "                       Alter %3ld:%02ld:%02ld Std - max: %d Std\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, Foldername,
                                      Alter / 3600,
                                     (Alter % 3600) / 60,
                                    ((Alter % 3600) % 60), maxAlter );
      #undef MELDUNG
    } // ---------------------------------------------------------------

    if (Alter > (maxAlter*3600))
    { // Verzeichnis nach Verfalldatum löschen
      // -------------------------------
      DIR* pdir = opendir(Foldername);
      if (pdir == NULL)
      { // -- Error
        sprintf(ErrText, "Error opendir(%s): %d", Foldername, errno);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      // Verzeichnis muss leer sein: alle enthaltenen Dateien löschen
      // ------------------------------------------------------------
      struct dirent* pdirzeiger;
      while((pdirzeiger=readdir(pdir)) != NULL)
      {
        if (((*pdirzeiger).d_type) == DT_REG)
        { // reguläre Datei
          // --------------
          char Filename[ZEILE];                       // Pfad der Datei
          sprintf(Filename,"%s/%s", Foldername, (pdirzeiger)->d_name);
          delFiles += remFile(Filename, MAXALTER);
        }
      }

      // nun auch noch das Verzeichnis löschen
      // ------------------------------------
      if (rmdir(Foldername) == 0)
      { // gelöscht!
        deleted = 1;
        sprintf(TmpText, "%s/", Foldername);    // als Verzeichnis kennzeichnen
        delFolders += Deleted(TmpText);         // im Log vermerken
      }
      else
      { // -- Error
        sprintf(ErrText, "rmdir('%s'):"\
        "              Err %d - '%s'", Foldername, errno, strerror(errno));
        // keine Fehlerausgabe, kann '..'-Verzeichnis sein
        //return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }

      if (closedir(pdir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", Foldername);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }

      if (deleted > 0)
      { // ------------------------------------------------------------
        #define MELDUNG   "     %s()#%d: ------ '%s' gelöscht!\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, Foldername);
        #undef MELDUNG
      } // ------------------------------------------------------------
    }
  }
  int retval = FFAKTOR*delFiles + deleted; // um beide Werte zuückzugeben können

  DEBUG_d("<--- %s()#%d -<%d>- \n\n",  __FUNCTION__, __LINE__ , retval);

  return retval;
}
//***********************************************************************************************

// Datei kopieren
// --------------
// bestehende Dateien werden nicht überschrieben
// Rückgabe: kopierter Datei-Typ
enum Filetype copyFile(char* destination, const char* source)
{
  DEBUG_t("===> %s()#%d: %s(%s,\n                       <----- %s) ========\n",
          __FUNCTION__, __LINE__, __FUNCTION__, destination, source);

  int in_fd;
  int out_fd;
  int n_chars;
  char buf[BUFFER];
  char ErrText[ERRBUFLEN];

//  chown(source, 1000, 1000);

  // Quell- und Zieldatei öffnen
  // ---------------------------
  if((out_fd = open(destination, FLAGS, MODE)) == -1 )    // Ziel-Datei öffnen
  { // -- Error
    if (errno == EEXIST)                                  // - Datei ist schon vorhanden
    {
      { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "     %s()#%d: '%s' schon vorhanden!\n"
      DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, destination);
      #undef MELDUNG
      } // --------------------------------------------------------------

      // die Quelldatei löschen
      // ----------------------
      remFile(source, SOFORT_h);

      DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__ , EXIT_SUCCESS);
      return OHNE;
    }
    else if (errno = 21)  // '.' und '..'
      return OHNE;

    sprintf(ErrText, "create('%s')", destination);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  if((in_fd = open(source, O_RDONLY)) == -1 )             // Quell-Datei öffnen
  { // -- Error
    sprintf(ErrText, "open '%s'", source);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  // Inhalt übertragen
  // =====================
  long lng = 0;
  while( (n_chars = read(in_fd, buf, BUFFER)) > 0 )       // Quell-Datei lesen ...
  {                                                       // ... und in Ziel-Datei schreiben
    if( write(out_fd, buf, n_chars) != n_chars )
    { // -- Error
      sprintf(ErrText, "write '%s'", destination);
      return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
    }
    { // --- Debug-Ausgaben ------------------------------------------
      #define MELDUNG   "       %s()#%d: '%d' chars kopiert!\n"
      DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, n_chars);
      #undef MELDUNG
    } // --------------------------------------------------------------
    lng += n_chars;
  } // =============== fertig =====================

  if( n_chars == -1 )                                     // Lesefehler
  { // -- Error
    sprintf(ErrText, "read '%s'", source);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  if( close(in_fd) == -1)                                 // Quell-Datei schließen
  { // -- Error
    sprintf(ErrText, "close '%s'", destination);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  if( close(out_fd) == -1 )                               // Ziel-Datei schließen
  { // -- Error
    sprintf(ErrText, "close '%s'", source);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
    return OHNE;
  }

  char tmpbuf[NOTIZ];
  sprintf(tmpbuf, "%s(%ld)", destination, lng);
  newFiles = Added(tmpbuf);

  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG   "     %s()#%d: Datei kopiert!\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__);
    #undef MELDUNG
  } // --------------------------------------------------------------

  if(getFiletyp(source) == AVI)
  {
    // Namen für Mail-Message merken
    // -----------------------------
    char datum[ZEILE/8];
    char event[ZEILE/8];
    char* datbeg = strrchr(destination, '/');             // Dateinamen suchen
    strcpy(datum, datbeg+1);
    char* datend = strchr(datum, '-');                    // Trenner suchen
    *datend = '\0';
    replace_character(datum, '.', ':');
    strcpy(event, datend+1);
    char* eventend = strrchr(event, '.');
    *eventend = '\0';
    sprintf("newItem", "Event %s - %s", event, datum);
  }

  DEBUG_t("<--- %s()#%d -<%d>- \n",  __FUNCTION__, __LINE__ , getFiletyp(source));

  return getFiletyp(source);
}
//***********************************************************************************************

//// Dateien mit externen Buffer synchronisieren
//// -------------------------------------------
static int SyncoFiles (const char *Pfad)
{
//  DEBUG(" ==> %s()#%d: function %s(%s) ======================\n",
//             __FUNCTION__, __LINE__, __FUNCTION__, Pfad);
//  char Logtext[ZEILE];
//  time_t Start;
//  int Dauer;
//  int Status = 0;
  int retval = EXIT_SUCCESS;
//
//  Start = time(NULL);
//  Status = copyFile(target, Pfad);
//  Dauer = time(NULL) - Start;
//
//  sprintf(Logtext, ">> Save-Command done in %d sec (%d)\n", Dauer, Status);
//  syslog(LOG_NOTICE, "%s(): %s",__FUNCTION__, Logtext);
//
//  if (Status != 0)
//  { // Übertragung hat nicht geklappt !
//    // --------------------------------
//    sprintf(Logtext, ">> Save-Command failed: Status %d\n", Status);
//    syslog(LOG_NOTICE, "%s(): %s",__FUNCTION__, Logtext);
//    retval = EXIT_FAILURE;
//  }
//  DEBUG("<- %s()#%d  \n",  __FUNCTION__, __LINE__);
  return retval;
}
//***********************************************************************************************

// belegten Speicherplatz ermitteln
// ---------------------------------
long calcSize(char* Pfad)
{
  DEBUG_c("=> %s()#%d: %s(%s) =============\n",
                              __FUNCTION__, __LINE__, __FUNCTION__, Pfad);

  long SizeTotal = 0;                          // gesamte Speicherbelegung
  char ErrText[ERRBUFLEN];

  // alle Dateien in allen Verzeichnisse durchsuchen
  // =============================================================
  DIR *pdir = opendir(Pfad);                    // '.../pics' öffnen
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }

  // das komplette Verzeichnis auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    // Datumsverzeichnis-Ebene: '2021-05-09' !
    // ---------------------------------------
    char dDirname[ZEILE];                       // Pfad des Datumsverzeichnisses
    sprintf(dDirname,"%s%s", Pfad, (*pdirzeiger).d_name);

    char datdir[ZEILE];                         // Name der Datumsverzeichnisses
    strcpy(datdir, strrchr(dDirname, '/')+1);   // ... das letzte Glied
    if (isDatum(datdir))                        // soll diese Verzeichnis angesehen werden?
    {
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n== %s()#%d: '%s'\n"
        DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, dDirname);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
      DIR* edir = opendir(dDirname);            // Unterverzeichnis öffnen
      if (edir == NULL)
      { // -- Error
        sprintf(ErrText, "opendir '%s'", dDirname);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      // das komplette Verzeichnis auslesen
      // ----------------------------------
      struct dirent* edirzeiger;
      while((edirzeiger=readdir(edir)) != NULL)
      {
        // Eventverzeichnis-Ebene!
        // -----------------------
        char eDirname[ZEILE];                   // Pfad des Datumsverzeichnisses
        sprintf(eDirname,"%s/%s", dDirname, (*edirzeiger).d_name);

        if (strstr(eDirname, _EVENT_) != NULL)  // soll diese Verzeichnis angesehen werden?
        {
          { // --- Debug-Ausgaben ----------------------------------------------------------------
            #define MELDUNG   "== %s()#%d: '%s'\n"
            DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, eDirname);
            #undef MELDUNG
          } // ------------------------------------------------------------------------------------

          DIR* udir = opendir(eDirname);            // Unterverzeichnis öffnen
          if (udir == NULL)
          { // -- Error
            sprintf(ErrText, "opendir '%s'", eDirname);
            return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
          }
          // hier stehen alle Dateien eines Unterverzeichnisses zur Verfügung
          // ----------------------------------------------------------------
          unsigned long cSizeofall=0;           // Speicherbelegung dieses Unterverzeichnisses
          struct dirent* udirzeiger;
          while((udirzeiger=readdir(udir)) != NULL) // Zeiger auf den Inhalt diese Unterverzeichnisses
          {
            // jede Datei ansehen
            // ------------------
            if (((*udirzeiger).d_type) == DT_REG)
            { // reguläre Datei
              // --------------
              struct stat cAttribut;
              char Filename[ZEILE+8];
              sprintf(Filename,"%s/%s", eDirname, (*udirzeiger).d_name);
              { // --- Debug-Ausgaben ----------------------------------------------------------------
                #define MELDUNG   "== %s()#%d: '%s'\n"
                DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, Filename);
                #undef MELDUNG
              } // ------------------------------------------------------------------------------------
              if(stat(Filename, &cAttribut) == -1)            // Datei-Attribute holen
              { // -- Error
                sprintf(ErrText, "read attribut '%s'", Filename);
                return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
              }
              // hier stehen die Attribute jeder Datei einzeln zur Verfügung
              // -----------------------------------------------------------
              unsigned long cFSize = cAttribut.st_size;       // Dateilänge
              cSizeofall += cFSize;                           // Gesamtlänge [kB]
              { // --- Debug-Ausgaben ----------------------------------------------------------------
                #define MELDUNG   "== %s()#%d:   cSizeofall = '%ld' chars += '%ld' chars\n"
                DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, cSizeofall, cFSize);
                #undef MELDUNG
              } // ------------------------------------------------------------------------------------
            }
          }
          if (closedir(udir) != 0)
          { // -- Error
            sprintf(ErrText, "closedir '%s'", eDirname);
            return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
          }
          SizeTotal += cSizeofall;                            // gesamte Speicherbelegung
          { // --- Debug-Ausgaben ----------------------------------------------------------------
            #define MELDUNG   "== %s()#%d:   SizeTotal = '%ld' chars\n\n"
            DEBUG_c(MELDUNG, __FUNCTION__, __LINE__, SizeTotal);
            #undef MELDUNG
          } // ------------------------------------------------------------------------------------
        }
      }
      if (closedir(edir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", dDirname);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
    }
  }
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }
//
//  sprintf(Logtext, ">>> %s()#%d: belegt gesamt: %3.3f MB\n",
//                       __FUNCTION__, __LINE__, ((float)SizeTotal+((1024*1024)/2))/(1024*1024));
//  syslog(LOG_NOTICE, "%s", Logtext);
//
#if BREAK22
  { // // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n   %s()#%d:   <--- Funktion '%s' fertig! -- weiter mit ENTER -->\n",
                __FUNCTION__, __LINE__, __FUNCTION__);
    char dummy;
    scanf ("%c", &dummy);
  }
#endif

  DEBUG_c("<- %s()#%d -(%ld)-\n",  __FUNCTION__, __LINE__, SizeTotal);
  return SizeTotal;
}
//***********************************************************************************************
//
// On Linux, the dirent structure is defined as follows:
//
//    struct dirent {
//        ino_t          d_ino;       /* inode number */
//        off_t          d_off;       /* offset to the next dirent */
//        unsigned short d_reclen;    /* length of this record */
//        unsigned char  d_type;      /* type of file; not supported
//                                       by all file system types */
//        char           d_name[256]; /* filename */
//    };

// ----------------------------------------------------
// alles, was älter als 'MAXALTER_h' ist, wird gelöscht
// ----------------------------------------------------

int delOldest(char* Pfad)
{
  DEBUG_d("=> %s()#%d: function %s('%s') ====\n",
                __FUNCTION__, __LINE__, __FUNCTION__, Pfad);

  int FileKill = 0;                             // Zähler gelöschte Dateien
  int FoldKill = 0;                             // Zähler gelöschte Verzeichnisse
  char ErrText[ERRBUFLEN];

  // alle Dateien in allen Verzeichnissen durchsuchen
  // =============================================================
  DIR* pdir = opendir(Pfad);
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }

  // alle Datumsverzeichnisse auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char kurzname[NOTIZ];                       // Name des Datumsverzeichnisses
    char cDirname[ZEILE];                       // Pfad des Datumsverzeichnisses
    strcpy(kurzname, (pdirzeiger)->d_name);
    sprintf(cDirname,"%s/%s", Pfad, kurzname);

    { // --- Debug-Ausgaben -----------------------------------------------------------------------
      #define MELDUNG   "\n   %s()#%d: === Datums-Verzeichnis '%s' ===\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, cDirname);
      #undef MELDUNG
    } // ------------------------------------------------------------------------------------------

    if (isDatum(kurzname))                      // Format 'YYYY-MM-DD' ?
    // dies sind die Datumsverzeichnisse !
    // ---------------------------------
    {
      DIR* cdir = opendir(cDirname);            // Datumsverzeichnis öffnen
      if (cdir == NULL)
      { // -- Error
        sprintf(ErrText, "Error opendir(%s): %d", cDirname, errno);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      // hier stehen alle Dateien eines Datumsverzeichnisses zur Verfügung
      // ----------------------------------------------------------------
      struct dirent* cdirzeiger;
      while((cdirzeiger=readdir(cdir)) != NULL) // Zeiger auf den Inhalt
      {
        if (((*cdirzeiger).d_type) == DT_DIR)
        {   // ------- Verzeichnis -------
          { // --- Debug-Ausgaben -------------------------------------------------------
            #define MELDUNG   "   %s()#%d: --- Verzeichnis '%s' ---\n"
            DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, (cdirzeiger)->d_name);
            #undef MELDUNG
          } // --------------------------------------------------------------------------
          char Folder[NOTIZ];                             // Name des Verzeichnisses
          sprintf(Folder,"%s/%s", cDirname, (cdirzeiger)->d_name);
          int Killed = remFolder(Folder, MAXALTER);       // beide Werte enthalten
          FileKill += Killed / FFAKTOR;
          FoldKill += Killed % FFAKTOR;

        }
        else  if (((*cdirzeiger).d_type) == DT_REG)
        {   // ----- reguläre Datei (Film) ------
          { // --- Debug-Ausgaben -------------------------------------------------------
            #define MELDUNG   "   %s()#%d: --- Datei '%s' ------\n"
            DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, (cdirzeiger)->d_name);
            #undef MELDUNG
          } // --------------------------------------------------------------------------
          char Datei[NOTIZ];                    // Name der Datei
          sprintf(Datei,"%s/%s", cDirname, (cdirzeiger)->d_name);
          FileKill += remFile(Datei, MAXALTER);
        }
      }
    }

    // Versuch, das Datumsverzeichnis zu löschen (wenn leer)
    // -----------------------------------------------------
    FoldKill += remFolder(cDirname, SOFORT_h);

    #define MELDUNG   "   %s()#%d: FoldKill=%d, FileKill=%d!\n"
    DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, FoldKill, FileKill);
    #undef MELDUNG

    #if BREAK_delOldest1
    { // STOP! -- weiter mit ENTER
      // -------------------------------
      printf("\n   %s()#%d:   <--- das war Datums-Verzeichnis '%s'! -- weiter mit ENTER -->\n",
                  __FUNCTION__, __LINE__ , cDirname);
      char dummy;
      scanf ("%c", &dummy);
    }
    #endif

  } // das waren die Datums-Verzeichnisse


  // jetzt noch die Statistik ausgeben
  // =====================================
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }
  if ((FileKill == 0) && (FoldKill == 0))
  {
    { // --- Debug-Ausgaben ----------------------------------------------------------------
      #define MELDUNG   "   %s()#%d: keine Dateien gelöscht !\n"
      DEBUG_d(MELDUNG, __FUNCTION__, __LINE__);
      #undef MELDUNG
    } // ------------------------------------------------------------------------------------
  }
  else
  {
    if (FileKill > 0)
    {
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "   %s()#%d: %d Dateien gelöscht !\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, FileKill);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
    }
    if (FoldKill > 0)
    {
      if (FoldKill == 1)
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "   %s()#%d: LED_EIN Verzeichnis gelöscht !\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
      else if (FoldKill > 1)
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "   %s()#%d: %d Verzeichnisse gelöscht !\n"
        DEBUG_d(MELDUNG, __FUNCTION__, __LINE__, FoldKill);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------
    }
  }
  int retval = FFAKTOR*FileKill + FoldKill;

#if BREAK_delOldest2
  { // STOP! -- weiter mit ENTER
    // -------------------------------
    printf("\n   %s()#%d:   <--- Funktion '%s' fertig! -- weiter mit ENTER -->\n",
                __FUNCTION__, __LINE__, __FUNCTION__);
    char dummy;
    scanf ("%c", &dummy);
  }
#endif

  DEBUG_d("<- %s()#%d--<%d>--\n",  __FUNCTION__, __LINE__, retval);
  return retval;
} // ------------ delOldest
//***********************************************************************************************

// ---------------------------------------------------
// Dateien übertragen
// ---------------------------------------------------
// Pfad: kompletter Quell-Verzeichnispfad
// Ziel: kompletter Ziel-Verzeichnispfad

long FileTransfer(const char* Pfad, const char* Ziel)
{
  DEBUG_t("=> %s()#%d: >%s('%s',\n"\
     "                               ===>  '%s')\n",
                 __FUNCTION__, __LINE__, __FUNCTION__, Pfad, Ziel);
  int fldCount=0;
  int aviCount=0;
  int jpgCount=0;

  // alle Dateien in allen Verzeichnisse durchsuchen
  // =============================================================
  { // --- Debug-Ausgaben ------------------------------------------
    #define MELDUNG  "   %s()#%d: Verzeichnis '%s' untersuchen\n"
    DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, Pfad);
    #undef MELDUNG
  } // --------------------------------------------------------------

  DIR *pdir = opendir(Pfad);                                        // '.../pix' öffnen
  if (pdir == NULL)
  { // -- Error
    char ErrText[ZEILE];
    sprintf(ErrText, "opendir('%s')", Pfad);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }

  // das komplette Quell-Verzeichnis auslesen
  // ----------------------------------------
  bool Toggle=0;
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    Toggle = !Toggle;

    char QuellPfad[ZEILE];                                          // Gesamt-Pfad des Unterverzeichnisses
    char QuellVerzeichnis[ZEILE];                                   // Name des Unterverzeichnisses
    strcpy(QuellVerzeichnis, (*pdirzeiger).d_name);                 // Name des Unterverzeichnisses
    sprintf(QuellPfad,"%s%s", Pfad, QuellVerzeichnis);
    if (strstr(QuellVerzeichnis, _EVENT_) != NULL)                  // soll diese Verzeichnis angesehen werden?
    {

      { // --- Debug-Ausgabe ------------------------------------------
        #define MELDUNG   "   %s()#%d: - Unterverzeichnis '%s' untersuchen\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, QuellVerzeichnis);
        #undef MELDUNG
      } // --------------------------------------------------------------

//*                       !------------- QuellPfad -----------------!
//*                                              !-QuellVerzeichnis-!
//*      SOURCE           /home/pi/Garten/pix/ - Event_2879/*
//*
//*      DESTINATION            /media/kamera/ - 2021-02-08/Event_2879/*
//*                                              !--Datum--!---Event---!
//*                                              !-- ZielVerzeichnis --!
//*                             !------------- ZielPfad ---------------!
//*        =         DISKSTATION/surveillance/ -  .....................
//*

      // Datum der Quelle ermitteln
      // ---------------------------
      struct stat attribut;
      if(stat(QuellPfad, &attribut) == -1)
      { // -- Error
        char ErrText[ZEILE];
        sprintf(ErrText, "attribut('%s')", QuellPfad);
        return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
      }
      time_t ZeitStempel = attribut.st_atime;

      // Zielverzeichnis gleichen Namens anlegen
      // ----------------------------------------
      char ZielPfad[ZEILE]={'\0'};                                  // Datums-Zielpfad
      char ZielVerzeichnis[ZEILE]={'\0'};                           // Datums-Zielverzeichnis
      makeDatumsFoldername(ZeitStempel, ZielVerzeichnis);           // Namen für Datums-Zielverzeichnis
      strcpy(ZielPfad, Ziel);                                       // Ziel-Verzeichnis im Raspi: 'media/kamera/'
      strcat(ZielPfad, ZielVerzeichnis);                            // an Pfad dranhängen
      fldCount += makeFolder(ZielPfad);                             // Datums-Zielverzeichnis erstellen

      { // --- Debug-Ausgabe --------------------------------------------
        #define MELDUNG   "   %s()#%d: --> ZielPfad '%s' (No.%d)\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, ZielPfad, fldCount);
        #undef MELDUNG
      } // --------------------------------------------------------------

      // Unterverzeichnis für das Ereignis anlegen
      // -----------------------------------------
      char EventVerzeichnis[ZEILE]={'\0'};                          // Ereignis-Zielverzeichnis
      strcpy(EventVerzeichnis, ZielPfad);                           // DatumsVerzeichnis als Vorspann
      strcat(EventVerzeichnis, "/");
      char* Originalname=strrchr(QuellPfad,'/') + 1;                // Originalname vm QuellVerzeichnis
      strcat(EventVerzeichnis, Originalname);                       // Ereignis-Zielverzeichnis ergänzen
      fldCount += makeFolder(EventVerzeichnis);                     // Ereignis-Zielverzeichnis erstellen

      { // --- Debug-Ausgabe -----------------------------------------------
        #define MELDUNG   "   %s()#%d: EventVerzeichnis '%s' (No.%d)\n"
        DEBUG_t(MELDUNG, __FUNCTION__, __LINE__, EventVerzeichnis, fldCount);
        #undef MELDUNG
      } // ------------------------------------------------------------------

      { // 3. Dateien übertragen
        // ---------------------
        DIR* QuellDir = opendir(QuellPfad);                         // QuellVerzeichnis öffnen
        if (QuellDir == NULL)
        { // -- Error
          char ErrText[ZEILE];
          sprintf(ErrText, "opendir('%s')", QuellPfad);
          return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
        }

        struct dirent* QuellZeiger;
        while((QuellZeiger=readdir(QuellDir)) != NULL)              // alle Dateien dieses Verzeichnisses
        {
          char Dateiname[ZEILE];
          strcpy(Dateiname, (*QuellZeiger).d_name);                   // zu kopierende Datei
          char QuellDateiname[ZEILE];
          sprintf(QuellDateiname,"%s/%s", QuellPfad, Dateiname);      // Quell-Datei
          char ZielDateiname[ZEILE];
          sprintf(ZielDateiname,"%s/%s", EventVerzeichnis, Dateiname);// Ziel-Datei

          switch(copyFile(ZielDateiname, QuellDateiname))             // -- Datei übertragen --
          {
            case OHNE:
               break;
            case JPG:
              jpgCount++;
              break;
            case AVI:
              aviCount++;
              break;
            case MKV:
              aviCount++;
              break;
            default:
              break;
          }
//          newFiles = Added(ZielDateiname);
        } // --- alle Dateien

        // QuellVerzeichnis wieder schließen
        // ----------------------------------
        if (closedir(QuellDir) != 0)
        { // -- Error
          char ErrText[ZEILE];
          sprintf(ErrText, "closedir '%s'", QuellPfad);
          return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
        }
      }

      #if BREAK_FileTransfer1
      { // STOP! -- Verzeichnis fertig -- weiter mit ENTER
        // ------------------------------------------------------------
        printf("\n   %s()#%d:   --- Verzeichnis '%s' fertig! -- weiter mit ENTER --\n\n",
                    __FUNCTION__, __LINE__, QuellPfad);
        char dummy;
        scanf ("%c", &dummy);
      }
      #endif

      // das QuellVerzeichnis löschen
      // ----------------------------
      remFolder(QuellPfad, SOFORT_h);

    }  // --- dieses Eventverzeichnis
  } // --. Gesamt-Verzeichnis --
  // Gesamt-Verzeichnis schließen
  // ------------------------------
  if (closedir(pdir) != 0)
  { // -- Error
    char ErrText[ZEILE];
    DEBUG(ErrText, "closedir '%s'", Pfad);
    Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
  }

  #if BREAK_FileTransfer2
  { // STOP! -- Funktionsende -- weiter mit ENTER
    // ------------------------------------------------
    printf("\n   %s()#%d:   <--- Funktion '%s' fertig! -- weiter mit ENTER -->\n",
                __FUNCTION__, __LINE__, __FUNCTION__);
    char dummy;
    scanf ("%c", &dummy);
  }
  #endif

  int retval = (jpgCount*DFAKTOR) + (aviCount*FFAKTOR) + fldCount;

  DEBUG_t("<- %s()#%d --<%d>-- \n",  __FUNCTION__, __LINE__, retval);
  return retval;
}
//***********************************************************************************************
//                                                                                              *
//                                    main()                                                    *
//                                                                                              *
//***********************************************************************************************

int main(int argc, char *argv[])
{
  sprintf (Version, "Vers. %d.%d.%d - %s", MAXVERS, MINVERS, BUILD, __DATE__);
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 ); // Verbindung zum Dämon Syslog aufbauen
  syslog(LOG_NOTICE, ">>>>> %s - %s - PID %d - User %d/%d, Group %d/%d/ <<<<<<",
                          PROGNAME, Version, getpid(), geteuid(), getuid(), getegid(), getgid());

  char puffer[BUFFER];
  char LogText[ZEILE];
  char ErrText[ERRBUFLEN];
  int fd;
  int status;

  // Dieses Programm
  // ----------------
  sprintf(LogText, "'%s %s' - User %d/%d, Group %d/%d\n",
                     argv[0], Version, geteuid(),getuid(), getegid(),getgid());
  printf(LogText);
  {// --- Log-Ausgabe ---------------------------------------------------------
    sprintf(LogText, ">>> %s - PID %d, User %d/%d, Group %d/%d",
                      Version, getpid(), geteuid(),getuid(), getegid(),getgid());
    MYLOG(LogText);
  } // ------------------------------------------------------------------------

  // schon mal den Watchdog füttern
  // ------------------------------
  feedWatchdog(PROGNAME);

  // Host ermitteln
  // ---------------
  status = gethostname(Hostname, NOTIZ);
  if (status < 0)
  { // -- Error
    sprintf(ErrText, "gethostname '%s'", Hostname);
    return (Error_NonFatal(ErrText, __FUNCTION__, __LINE__));
  }
  sprintf(LogText,"    %s()#%d: Hostname: '%s'",  __FUNCTION__, __LINE__, Hostname);
  MYLOG(LogText);

  // Speicherziel
  // -------------
  sprintf(LogText,"    %s()#%d: Speicherziel: '%s'",  __FUNCTION__, __LINE__, DESTINATION);
  MYLOG(LogText);

  // PID ablegen
  // -------------
  sprintf(LogText,"    %s()#%d: meine PID = '%ld'",  __FUNCTION__, __LINE__, savePID(FPID));
  MYLOG(LogText);


  // IP-Adresse ermitteln
  // ----------------------
  // nur das letzte Glied wird gebraucht
  {
    readIP(meineIPAdr, sizeof(meineIPAdr));
    sprintf(LogText,"    %s()#%d: meine IPs: '%s'",  __FUNCTION__, __LINE__, meineIPAdr);
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


  // named pipe(Fifo) erstellen
  // --------------------------
  {
    umask(0);
    status = mkdir(FDIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (mkfifo (FIFO, 0666) < 0)
    {
      if(errno == EEXIST)                       // FIFO bereits vorhanden - kein fataler Fehler
        ;
      else
      {
        sprintf(ErrText, "mkfifo(%s)", FIFO);
        showMain_Error(ErrText, __FUNCTION__, __LINE__);
        exit (EXIT_FAILURE);
      }
    }
    sprintf(LogText,"    %s()#%d: Fifo OK: '%s'",  __FUNCTION__, __LINE__, FIFO);
    MYLOG(LogText);
	}

  // Ist GPIO klar?
  // --------------
  {
    wiringPiSetup();
    pinMode (LED_ROT,    OUTPUT);
    pinMode (LED_GELB,   OUTPUT);
    pinMode (LED_GRUEN,  OUTPUT);
    pinMode (LED_BLAU,   OUTPUT);
    pullUpDnControl (LED_ROT, PUD_UP) ;
    pullUpDnControl (LED_GELB, PUD_UP) ;
    pullUpDnControl (LED_GRUEN, PUD_UP) ;
    pullUpDnControl (LED_BLAU, PUD_UP) ;
    #define ANZEIT  44 /* msec */
    digitalWrite (LED_ROT,   LED_EIN);
    delay(3*ANZEIT);
    digitalWrite (LED_ROT,   LED_AUS);
    for (int ix=0; ix < 12; ix++)
    {
      digitalWrite (LED_GELB,   LED_EIN);
      delay(ANZEIT);
      digitalWrite (LED_GELB,   LED_AUS);
      digitalWrite (LED_GRUEN,  LED_EIN);
      delay(ANZEIT);
      digitalWrite (LED_GRUEN,  LED_AUS);
      digitalWrite (LED_BLAU,   LED_EIN);
      delay(ANZEIT);
      digitalWrite (LED_BLAU,   LED_AUS);
    }
    DEBUG(">> %s()#%d @ %s ----- GPIO OK -------\n", __FUNCTION__, __LINE__, __NOW__);
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "    ----- GPIO OK -------");
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
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
    sprintf(Zeile,"RaspBerry Pi  No. %s - '%s' -- IP-Adresse '%s:%d'\n",
               readRaspiID(Buf), Hostname, readIP(meineIPAdr, sizeof(meineIPAdr)), STREAM_PORT);
    strcat(MailBody, Zeile);
    DEBUG( "MailBody: %s\n", MailBody);

    sendmail(Betreff, MailBody);  // vorläufig
  } // -----  Bereitmeldung per Mail -----------------------------------


  DEBUG(">> %s()#%d @ %s\n\n", __FUNCTION__, __LINE__, __NOW__);
  { // --- Log-Ausgabe ---------------------------------------------------------
    char LogText[ZEILE];  sprintf(LogText, "    ----- Init OK -------");
    MYLOG(LogText);
  } // ------------------------------------------------------------------------


  // Fifo aktivieren und auf ersten Datenblock warten
  // ------------------------------------------------
  {
    DEBUG(">> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);
    digitalWrite (LED_GRUEN, LED_EIN);
    fd = open (FIFO, O_RDONLY);                   // Empfänger liest nur aus dem FIFO
    if (fd == -1)
    {
      sprintf(LogText, ">> %s()#%d: Error Open Fifo !",  __FUNCTION__, __LINE__);
      DEBUG(LogText);
      showMain_Error(LogText, __FUNCTION__, __LINE__);
      exit (EXIT_FAILURE);
    }
    sprintf(LogText, ">>> %s()#%d: FIFO '%s' open !",  __FUNCTION__, __LINE__ , FIFO);
    syslog(LOG_NOTICE, "%s", LogText);
    DEBUG(LogText);
    DEBUG(">>> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);

    char Betreff[ZEILE] = {'\0'};
    char Zeitbuf[NOTIZ];
    sprintf( Betreff, "FIFO open >%s< @ %s",  FIFO, mkdatum(time(0), Zeitbuf));
    DEBUG( "Betreff: %s\n", Betreff);
    DEBUG(">> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);

    char MailBody[BODYLEN] = {'\0'};
    char Zeile[ZEILE];
    char Buf[NOTIZ];
    char* Path=NULL;
    DEBUG(">> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);
    sprintf(Zeile,"Programm '%s/%s' %s\n", getcwd(Path, ZEILE), PROGNAME, Version);
    strcat(MailBody, Zeile);
    sprintf(Zeile,"RaspBerry Pi  No. %s - '%s' -- IP-Adresse '%s'",
               readRaspiID(Buf), Hostname, readIP(meineIPAdr, sizeof(meineIPAdr)));
    DEBUG(">> %s()#%d @ %s\n", __FUNCTION__, __LINE__, __NOW__);
    strcat(MailBody, Zeile);
    DEBUG( "MailBody: %s\n", MailBody);

//    sendmail(Betreff, MailBody);
  }

  bool ShowReady = true;
  DO_FOREVER // *********************** Endlosschleife **************************************
  {
    feedWatchdog(PROGNAME);
    if (ShowReady)    // nur einmalig anzeigen
    { // --- Debug-Ausgaben ----------------------------------------------------------------
      #define MELDUNG   "\n>>> %s()#%d: -------------------- bereit  --  @ %s ---------------------\n"
      DEBUG(MELDUNG, __FUNCTION__, __LINE__, __NOW__);
      #undef MELDUNG
    } // ------------------------------------------------------------------------------------
    ShowReady = false;
    digitalWrite (LED_BLAU, LED_AUS);
    digitalWrite (LED_GRUEN, LED_EIN);

    if ( read(fd, puffer, BUFFER) )             // == > auf Aufträge von 'motion' warten
    {
      ShowReady = true;
      Startzeit(T_GESAMT);                      // Zeitmessung starten
      syslog(LOG_NOTICE, ">>> %s()#%d: <###---    neuer Auftrag    ---###>", __FUNCTION__, __LINE__);
      digitalWrite (LED_BLAU, LED_EIN);
      digitalWrite (LED_GRUEN, LED_AUS);
      newFiles   = 0;                           // neue Dateien
      newFolders = 0;                           // neue Verzeichnisse
      delFiles   = 0;                           // gelöschte Dateien
      delFolders = 0;                           // gelöschte Verzeichnisse

      // === Dateien übertragen ==================================================================

      Startzeit(T_FOLDER);

      // === Daten einlesen ==========================================================================
      { // --- Log-Ausgabe ---------------------------------------------------------
        char LogText[ZEILE];  sprintf(LogText, "    ----- neuer Auftrag -------");
        MYLOG(LogText);
      } // ------------------------------------------------------------------------

      {                                                   // Zeitmessung starten
        #define MELDUNG   "\n>>> %s()#%d: ################ neuer Auftrag @ %s ###############\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, __NOW__);
        #undef MELDUNG
      }
      long Items = FileTransfer(puffer, DESTINATION);     // Daten einlesen
      { // --- Debug-Ausgaben ----------------------------------------------------------------
      	char target[] = "target";
        #define MELDUNG   "\n>>> %s()#%d: Items=%ld\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__,Items);
        #undef MELDUNG
        int fldCount  = (Items % DFAKTOR) % FFAKTOR;      // Verzeicnisse Filmdateien
        int aviCount  = (Items % DFAKTOR) / FFAKTOR;      // Zähler Filmdateien
        int jpgCount  = Items / DFAKTOR;                  // Zähler Bilddateien
        #define MELDUNG   "    %s()#%d: -- %d Verzeichnis%s, %d Film%s, %d Bild%s kopiert\n"\
               "                    von '%s' ---> '%s' in %ld msec --"
        DEBUG( MELDUNG, __FUNCTION__, __LINE__, fldCount,(fldCount>1 ? "se" : ""),
                                                aviCount,(aviCount>1 ? "e" : ""),
                                                jpgCount,(jpgCount>1 ? "er" : ""),
                                                SOURCE, target, Zwischenzeit(T_GESAMT));
        #undef MELDUNG
        SYSLOG(LOG_NOTICE, ">>> %s()#%d: %d Verzeichnis%s, %d Film%s, %d Bild%s kopiert in %ld msec",
                        __FUNCTION__, __LINE__, fldCount,(fldCount>1 ? "se" : ""),
                                                aviCount,(aviCount>1 ? "e" : ""),
                                                jpgCount,(jpgCount>1 ? "er" : ""),
                                                Zwischenzeit(T_GESAMT));
        UNUSED (fldCount);
        UNUSED (aviCount);
        UNUSED (jpgCount);
      } // ------------------------------------------------------------------------------------
      UNUSED(Items);

      // === aufräumen ==========================================================================
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n\n>>> %s()#%d: ################## es wird aufgeraeumt ##################\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------

      Startzeit(T_FOLDER);                                // Zeitmessung starten
      delOldest(DESTINATION);                                   // aufräumen
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n    %s()#%d: -- %d Verzeichnis%s, %d Datei%s in %ld msec geloescht! --\n"
        DEBUG(MELDUNG,  __FUNCTION__, __LINE__, delFolders,(delFolders!=1 ? "se" : ""),
                                                delFiles,(delFiles!=1 ? "en" : ""),
                                                Zwischenzeit(T_FOLDER));
        #undef MELDUNG
        SYSLOG(LOG_NOTICE, ">>> %s()#%d: %d Verzeichnis%s, %d Datei%s in %ld msec geloescht!",
                        __FUNCTION__, __LINE__, delFolders,(delFolders!=1 ? "se" : ""),
                                                delFiles,(delFiles!=1 ? "en" : ""),
                                                Zwischenzeit(T_FOLDER));
      } // ------------------------------------------------------------------------------------

      // === berechnen ===========================================================================
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n\n>>> %s()#%d: ################## es wird berechnet ####################\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__);
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------


      Startzeit(T_FOLDER);                                // Zeitmessung starten
      double Speicherplatz = (float)calcSize(DESTINATION)/(1024*1024);
      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n    %s()#%d: -- Berechnung Speicherplatz: %2.3f MBytes in %ld msec --\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, Speicherplatz, Zwischenzeit(T_FOLDER));
        #undef MELDUNG
        sprintf(LogText, ">>> %s()#%d: Speicherplatz: %3.1f MB\n", __FUNCTION__, __LINE__, Speicherplatz);
        syslog(LOG_NOTICE, "%s", LogText);
      } // ------------------------------------------------------------------------------------
      UNUSED(Speicherplatz);

      // === fertig ==============================================================================

      { // --- Debug-Ausgaben ----------------------------------------------------------------
        #define MELDUNG   "\n\n>>> %s()#%d: ###### done in %ld msec ####################\n\n"
        DEBUG(MELDUNG, __FUNCTION__, __LINE__, Zwischenzeit(T_GESAMT));
        #undef MELDUNG
      } // ------------------------------------------------------------------------------------

      syslog(LOG_NOTICE, ">>> %s()#%d: <###---  done in %ld msec  ---###>",
                                                      __FUNCTION__, __LINE__, Zwischenzeit(T_GESAMT));
      { // --- Log-Ausgabe ---------------------------------------------------------
        char LogText[ZEILE];  sprintf(LogText, "    ----- done in %ld msec", Zwischenzeit(T_GESAMT));
        MYLOG(LogText);
      } // ------------------------------------------------------------------------


      digitalWrite (LED_GELB, LED_AUS);             // wurde von SqlMotion eingeschaltet!
    } // ======== Auftrag erledigt =================================================================


    // Überwachung, ob 'motion' noch läuft
    // -----------------------------------
    // es wird in regelmäßigen Abständen ein 'snapshot' erzeugt.
    #define SNAPSHOT    "/home/pi/Garten/pix/lastsnap.jpg"
    static bool ganzneu = true;
    {
      struct stat attribut;
      static bool zualt = true;
      if(stat(SNAPSHOT, &attribut) == -1)
      {
        char ErrText[ERRBUFLEN];
        if (ErrorFlag == 0)
        {
          sprintf(ErrText, "'%s' missing!", SNAPSHOT);
          Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
          zualt = true;
        }
      }
      else
      {
        errno = 0;
        time_t Alter = time(0) - attribut.st_mtime;
        //  DEBUG(">> %s()#%d @ %s; Alter '%s' = %ld sec\n",
        //           __FUNCTION__, __LINE__, __NOW__, SNAPSHOT, Alter);
        if (Alter > 4*REFRESH)
        { // Meldung nur einmal anzeigen
          // ----------------------------
          if (!zualt)
          {
            sprintf(ErrText, "'%s' fehlt seit %ld sec!", SNAPSHOT, Alter);
            Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
            zualt = true;
          }
        }
        else
        {
          if (zualt)
          {
            if (!ganzneu)
            {
              sprintf(ErrText, "'%s' wieder da", SNAPSHOT);
              Error_NonFatal(ErrText, __FUNCTION__, __LINE__);
            }
            ganzneu = false;
            zualt = false;
          }
        }
      }
    }


    // Blinken grüne LED als Lebenszeichen! <
    // ------------------------------------
    {
      static int blink = 0;
      blink++;
      if (blink % 7 == 0)
      {
        digitalWrite (LED_GRUEN, LED_AUS);
        delay(100);
        digitalWrite (LED_GRUEN, LED_EIN);
      }
    }

    // nach einem Fehler ...
    // -----------------------------
    if (ErrorFlag > 0)
    { // ... rote LED wieder ausschalten
      // ---------------------------
      if (time(0) > ErrorFlag)
      { // wenn Zeit abgelaufen
        // --------------------
        ErrorFlag = 0;
        digitalWrite (LED_ROT, LED_AUS);
      }
    }

    // kleine Pause
    // -------------
    delay(100);

  } // =================================================================================

//  // Fehler-Mail abschicken (hier nutzlos)
//  // -------------------------------------
//  digitalWrite (LED_ROT, LED_EIN);
//  sprintf(Logtext, ">> %s()#%d: Error %s ---> '%s' OK\n",__FUNCTION__, __LINE__, PROGNAME, "lastItem");
//  syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
//
//  strcat(MailBody, Logtext);
//  char Betreff[ERRBUFLEN];
//  sprintf(Betreff, "Error-Message von %s: >>%s<<", PROGNAME, "lastItem");
//  sendmail(Betreff, MailBody);                  // Mail-Message absetzen
}
//***********************************************************************************************
