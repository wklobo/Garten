//***************************************************************************//
//*                                                                         *//
//* File:          fifomotion.c                                             *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2014-08-23  --  2016-02-04;;                             *//
//* Version:       $REV$                                                    *//
//* Last change:   2020-12-29;                                              *//
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
//*    ./Fifo &    (Daemon)                                                 *//
//*                                                                         *//
//***************************************************************************//

#define _MODUL0

#include "../version.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>

#include "../sqlmotion.h"
#include "../motLED.h"
#include "../error.h"
#include "../../sendmail/sendMail.h"

#define FLAGS   O_WRONLY|O_CREAT|O_EXCL
#define MODE    S_IRWXU|S_IRWXG|S_IROTH

#define BUF      16384

char MailBody[BODYLEN] = {'\0'};
char lastItem[ZEILE];
char newItem[ZEILE];
char ErrText[ZEILE];
char target[2*ZEILE];
char* ProgName;
int jpgCount;
int aviCount;

#define EVBEG '/'      /* Markierung */
#define EVEND '_'      /* Markierung */
#define MODUS  0774

#define _DEBUG

//************************************************************************************//

// String kopieren 
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
//************************************************************************************//

//// lokale Fehlermeldung
//// ------------------------
//// fügt Informationen ein und ruft Standard-Fehlermeldung auf
//void showMain_Error( char* Message, const char* Func, int Zeile)
//{
//  char ErrText[ERRBUFLEN];
//  sprintf( ErrText, "%s()#%d @%s in %s: \"%s\"", Func, Zeile, __NOW__, __FILE__, Message);
//
//  printf("   -- Fehler -->  %s\n", ErrText);    // lokale Fehlerausgabe
//  finish_with_Error(ErrText);
//}
////************************************************************************************//
//
//// lokale Datenbank-Fehlermeldung
//// ------------------------
//// fügt Informationen ein und ruft Standard-Fehlermeldung auf
//void showMain_SQL_Error( char* Label, const char* Func, int Zeile, MYSQL *con)
//{
//  char ErrText[ERRBUFLEN];
//  mysql_close(con);                             // Datenbank schließen
//  sprintf(ErrText, "%s -- SQL-Error %d(%s)\n", 
//                    Label, mysql_errno(con), mysql_error(con));
//  showMain_Error( ErrText, Func, Zeile);        // Meldung weiterreichen
//}
//************************************************************************************//
//************************************************************************************//
//
// nicht-fataler Fehler
// ------------------------
// Fehler wird nur geloggt
int LogError( char* Message, const char* Func, int Zeile)
{
  int errsv = errno;
	switchLED(FIFOMOTION, ROT);                       	// -- LED Alarm ein
#ifdef _DEBUG
  perror(Message);
#endif
  char thisErr[ZEILE];
  sprintf(thisErr, ">* %s - Error %i: %s\n", Message, errsv, strerror(errsv));
  syslog(LOG_NOTICE, thisErr);

  char Logtext[ZEILE];
  time_t t = time(NULL);
  struct tm *ts = localtime(&t);
  sprintf(Logtext, "-- Error @ %s -- %s\n", asctime(ts), thisErr);
  strcat(MailBody, Logtext);										// Fehler in der Mail vermerken

  return errsv;
}


// fataler Fehler
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showFifo_Error( char* Message, const char* Func, int Zeile)
{
  char ErrText[ERRBUFLEN];
  sprintf( ErrText, "%s()#%d in %s: \"%s\"", Func, Zeile,__FILE__, Message);

//  finish_with_Error(ErrText);
}
//***********************************************************************************************

// PID ablegen
// -----------
void savePID(void)
{
  FILE *piddatei;
  piddatei = fopen(FPID, "w");
  if (piddatei != NULL)
  {
    char myPID[30];
    sprintf(myPID,"%d", getpid());
    fprintf (piddatei, myPID);
    fclose (piddatei);
  }
}
//***********************************************************************************************

// Dateityp ermitteln
// ------------------
enum Filetype getFiletyp(const char* Filename)
{
  char *ptr;
  enum Filetype retval = ANDERE;

  ptr = strrchr(Filename, '.');
  if (ptr != NULL)
  {
    if (strncmp(ptr, _JPG, strlen(_JPG)) == 0)
      retval = JPG;
    else if (strncmp(ptr, _AVI, strlen(_AVI)) == 0)
      retval = AVI;
  }
  return retval;
}
//***********************************************************************************************

// Datei kopieren
// --------------
// bestehende Dateien werden nicht überschrieben
int copyFile(char *destination, const char *source)
{
  int in_fd;
  int out_fd;
  int n_chars;
  char buf[BUF];
#ifdef _DEBUG
  printf("%s()#%d: ------ '%s'\n                  <-- '%s'\n", __FUNCTION__, __LINE__, destination, source);
#endif

  if((out_fd = open(destination, FLAGS, MODE)) == -1 )    // Ziel-Datei öffnen
  { // -- Error
    if (errno == EEXIST)                                  // - Datei ist schon vorhanden
    {
      lastItem[0] = '\0';                                 // - Flag darum löschen
      return EXIT_SUCCESS;
    }
    sprintf(ErrText, "create '%s'", destination);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
  if((in_fd = open(source, O_RDONLY)) == -1 )             // Quell-Datei öffnen
  { // -- Error
    sprintf(ErrText, "%s()#%d: open '%s'", source);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }

  while( (n_chars = read(in_fd, buf, BUF)) > 0 )          // Quell-Datei lesen ...
  {                                                       // ... und in Ziel-Datei schreiben
#ifdef _DEBUG
    printf(" --------- gelesen '%d' chars \n", n_chars);
#endif
   if( write(out_fd, buf, n_chars) != n_chars )
    { // -- Error
      sprintf(ErrText, "write '%s'", destination);
      return (LogError(ErrText, __FUNCTION__, __LINE__));
    }
#ifdef _DEBUG
    printf(" --------- geschrieben '%d' chars \n", n_chars);
#endif
  }
  if( n_chars == -1 )                                     // Lesefehler
  { // -- Error
    sprintf(ErrText, "read '%s'", source);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }

  if( close(in_fd) == -1)                                 // Quell-Datei schließen
  { // -- Error
    sprintf(ErrText, "close '%s'", destination);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
#ifdef _DEBUG
    printf(" --------- geschlossen '%s'\n", destination);
#endif
  if( close(out_fd) == -1 )                               // Ziel-Datei schließen
  { // -- Error
    sprintf(ErrText, "close '%s'", source);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
#ifdef _DEBUG
    printf(" --------- geschlossen '%s'\n", source);
#endif

//  char Logtext[ZEILE];
//  sprintf(Logtext, ">> %s(): kopiert '%s'\n    --> '%s'\n", __FUNCTION__, source, destination);
//  if (strlen(MailBody)+strlen(Logtext) < MAIL)
//    strcat(MailBody, Logtext);
//
//#ifdef _DEBUG
//  printf(" ---- OK!  -- strlen(MailBody)=%d\n", strlen(MailBody));
//#endif

  if(getFiletyp(source) == AVI)
  {
    // Namen für Mail-Message merken
    // -----------------------------
    char datum[ZEILE];
    char event[ZEILE];
    char* datbeg = strrchr(destination, '/');             // Dateinamen suchen
    strcpy(datum, datbeg+1);
    char* datend = strchr(datum, '-');                    // Trenner suchen
    *datend = '\0';
    replace_character(datum, '.', ':');
    strcpy(event, datend+1);
    char* eventend = strrchr(event, '.');
    *eventend = '\0';
    sprintf(newItem, "Event %s - %s", event, datum);
  }

  return EXIT_SUCCESS;
}
//***********************************************************************************************

// Dateien mit externen Buffer synchronisieren
// -------------------------------------------
static int SyncoFiles (const char *Pfad)
{
  char SaveCommand[BUF];
  char Logtext[ZEILE];
  time_t Start;
  int Dauer;
  int status;
  int Status = 0;
  int retval = EXIT_SUCCESS;

	switchLED(FIFOMOTION, ORANGE);                       	// -- LED Alarm ein

  Start = time(NULL);
	char hostname[ZEILE];
	Status = copyFile(target, Pfad);
  Dauer = time(NULL) - Start;
  
	switchLED(FIFOMOTION, GRUEN);                       	// -- LED Alarm aus

  sprintf(Logtext, ">> Save-Command done in %d sec (%d)\n", Dauer, Status);
  syslog(LOG_NOTICE, "%s(): %s",__FUNCTION__, Logtext);

  if (Status != 0)
  { // Übertragung hat nicht geklappt !
    // --------------------------------
    sprintf(Logtext, ">> Save-Command failed: Status %d\n", Status);
    syslog(LOG_NOTICE, "%s(): %s",__FUNCTION__, Logtext);
    retval = EXIT_FAILURE;
  }
  return retval;
}
//***********************************************************************************************

// belegten Speicherplatz ermitteln
// ---------------------------------
long calcSize(char* Pfad)
{
#ifdef _DEBUG
	printf("=== %s()#%d: start function =====================================\n", __FUNCTION__, __LINE__);
#endif

  long SizeTotal = 0;                          // gesamte Speicherbelegung
  char Logtext[ZEILE];

  // alle Dateien in allen Verzeichnisse durchsuchen
  // =============================================================
  DIR *pdir = opendir(Pfad);                    // '.../pics' öffnen
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
  // das komplette Verzeichnis auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char cDirname[250];                         // Name der Unterverzeichnisses
    sprintf(cDirname,"%s%s", Pfad, (*pdirzeiger).d_name);
    if (strstr(cDirname, _FOLDER) != NULL)      // soll diese Verzeichnis angesehen werden?
    {
      DIR* cdir = opendir(cDirname);            // Unterverzeichnis öffnen
      if (cdir == NULL)
      { // -- Error
        sprintf(ErrText, "opendir '%s'", cDirname);
        return (LogError(ErrText, __FUNCTION__, __LINE__));
      }
      // hier stehen alle Dateien eines Unterverzeichnisses zur Verfügung
      // ----------------------------------------------------------------
      unsigned long cSizeofall=0;               // Speicherbelegung dieses Unterverzeichnisses
      struct dirent* cdirzeiger;
      while((cdirzeiger=readdir(cdir)) != NULL) // Zeiger auf den Inhalt diese Unterverzeichnisses
      {
        // jede Datei ansehen
        // ------------------
        if (((*cdirzeiger).d_type) == DT_REG)
        { // reguläre Datei
          // --------------
          struct stat cAttribut;
          char Filename[250];
          sprintf(Filename,"%s/%s", cDirname, (*cdirzeiger).d_name);
          if(stat(Filename, &cAttribut) == -1)            // Datei-Attribute holen
          { // -- Error
            sprintf(ErrText, "read attribut '%s'", Filename);
            return (LogError(ErrText, __FUNCTION__, __LINE__));
          }
          // hier stehen die Attribute jeder Datei einzeln zur Verfügung
          // -----------------------------------------------------------
          unsigned long cFSize = cAttribut.st_size;       // Dateilänge
          cSizeofall += ((cFSize+(1024/2))/1024);         // Gesamtlänge [kB]
#ifdef _DEBUG
//    			printf(" ---- %s: '%d' chars \n", Filename, cFSize);
#endif
        }
      }
      SizeTotal += cSizeofall;                            // gesamte Speicherbelegung
      if (closedir(cdir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", cDirname);
        return (LogError(ErrText, __FUNCTION__, __LINE__));
      }
    }
  }
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }

  sprintf(Logtext, ">> %s()#%d: belegt Gesamt: %3.1f MB\n",
                       __FUNCTION__, __LINE__, ((float)SizeTotal+(1024/2))/1024);
  syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
//  strcat(MailBody, Logtext);

#ifdef _DEBUG
	printf("+++ %s()#%d: exit function +++++++++++++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);
#endif
  return EXIT_SUCCESS;
}
//***********************************************************************************************

// alles, was älter als 'MAXALTER_h' ist, wird gelöscht
// -------------------------------------------------
int delOldest(char* Pfad)
{
#ifdef _DEBUG
	printf("=== %s()#%d: start function =====================================\n", __FUNCTION__, __LINE__);
#endif

  int FileCount = 0;                            // Zähler gelöschte Dateien
  int FoldCount = 0;                            // Zähler gelöschte Verzeichnisse
  char Logtext[ZEILE];

  // alle Dateien in allen Verzeichnissen durchsuchen
  // =============================================================
  DIR *pdir = opendir(Pfad);                    // '.../pics' öffnen
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
  // das komplette Verzeichnis auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char cDirname[250];                         // Name der Unterverzeichnisses
    sprintf(cDirname,"%s%s", Pfad, (*pdirzeiger).d_name);
    if (strstr(cDirname, _FOLDER) != NULL)      // soll diese Verzeichnis angesehen werden?
    {
      DIR* cdir = opendir(cDirname);            // Unterverzeichnis öffnen
      if (cdir == NULL)
      { // -- Error
        sprintf(ErrText, "opendir '%s'", cDirname);
        return (LogError(ErrText, __FUNCTION__, __LINE__));
      }
      // hier stehen alle Dateien eines Unterverzeichnisses zur Verfügung
      // ----------------------------------------------------------------
      struct dirent* cdirzeiger;
      while((cdirzeiger=readdir(cdir)) != NULL) // Zeiger auf den Inhalt diese Unterverzeichnisses
      {
        // jede Datei ansehen
        // ------------------
        if (((*cdirzeiger).d_type) == DT_REG)
        { // reguläre Datei
          // --------------
          struct stat cAttribut;
          char Filename[250];
          sprintf(Filename,"%s/%s", cDirname, (*cdirzeiger).d_name);
          if(stat(Filename, &cAttribut) == -1)           // Datei-Attribute holen
          { // -- Error
            sprintf(ErrText, "read attribut '%s'", Filename);
            return (LogError(ErrText, __FUNCTION__, __LINE__));
          }
          // hier stehen die Attribute jeder Datei einzeln zur Verfügung
          // -----------------------------------------------------------
          time_t FcDatum = cAttribut.st_mtime;            // Dateidatum
          time_t Alter_h = (time(NULL) - FcDatum) / 3600; // Alter der Datei
#ifdef _DEBUG
//          printf(" -- '%s' -> %d s -- %d s\n", Filename, FcDatum, cAttribut.st_mtime);
//          printf(" -- '%s' -> %d Std -- %d Std\n", Filename, Alter_h, MAXALTER_h);
#endif
          if (Alter_h > MAXALTER_h)
          { // Datei nach Verfalldatum löschen
            // -------------------------------
            remove(Filename);
            FileCount++;
#ifdef _DEBUG
          	printf(" -- '%s' -> %d Std -- %d Std\n", Filename, Alter_h, MAXALTER_h);
            printf(" -- '%s' gelöscht!\n", Filename);
#endif
          }
        }
      }
#ifdef _DEBUG
			printf("%s()#%d: closedir '%s'\n", __FUNCTION__, __LINE__, Pfad);
#endif
      if (closedir(cdir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", cDirname);
        return (LogError(ErrText, __FUNCTION__, __LINE__));
      }
      if (rmdir(cDirname) == 0)
      { // leere Verzeichnisse ebenfalls löschen
        // -------------------------------------
        FoldCount++;
      }
    }
  }
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
  if ((FileCount == 0) && (FoldCount == 0))
  {
    sprintf(Logtext, ">> %s()#%d: keine Dateien gelöscht !\n", __FUNCTION__, __LINE__);
//    strcat(MailBody, Logtext);
    syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
  }
  else
  {
    if (FileCount > 0)
    {
      sprintf(Logtext, ">> %s()#%d: %d Dateien gelöscht !\n", __FUNCTION__, __LINE__, FileCount);
//      strcat(MailBody, Logtext);
      syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
    }
    if (FoldCount > 0)
    {
      if (FoldCount == 1)
        sprintf(Logtext, ">> %s()#%d: Ein Verzeichnis gelöscht !\n", __FUNCTION__, __LINE__);
      else if (FoldCount > 1)
        sprintf(Logtext, ">> %s()#%d: %d Verzeichnisse gelöscht !\n", __FUNCTION__, __LINE__, FoldCount);
      syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
//      strcat(MailBody, Logtext);
    }
  }
#ifdef _DEBUG
	printf("+++ %s()#%d: exit function ++++++++++++++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);
#endif
  return EXIT_SUCCESS;
}
//***********************************************************************************************

// avi-Dateien in ein gesondertes Verzeichnis kopieren
// ---------------------------------------------------
int copyAvi(char* Pfad, char* Ziel)
{
#ifdef _DEBUG
	printf("=== %s()#%d: start function =====================================\n", __FUNCTION__, __LINE__);
#endif
  char Logtext[ZEILE];

  // alle Dateien in allen Verzeichnisse durchsuchen
  // =============================================================
#ifdef _DEBUG
  printf(" -- Verzeichnis '%s' durchsuchen\n", Pfad);
#endif
  float SizeTotal = 0;                          // gesamte Speicherbelegung
  DIR *pdir = opendir(Pfad);                    // '.../pics' öffnen
  if (pdir == NULL)
  { // -- Error
    sprintf(ErrText, "opendir '%s'", Pfad);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
  // das komplette Verzeichnis auslesen
  // ----------------------------------
  struct dirent* pdirzeiger;
  while((pdirzeiger=readdir(pdir)) != NULL)
  {
    char cDirname[ZEILE];                             // Name der Unterverzeichnisses
    sprintf(cDirname,"%s%s", Pfad, (*pdirzeiger).d_name);
    if (strstr(cDirname, _FOLDER) != NULL)          // soll diese Verzeichnis angesehen werden?
    {
#ifdef _DEBUG
      printf(" --- Unterverzeichnis '%s' ansehen\n", cDirname);
#endif
      DIR* cdir = opendir(cDirname);                // Unterverzeichnis öffnen
      if (cdir == NULL)
      {     // -- Error
        sprintf(ErrText, "opendir '%s'", cDirname);
        return (LogError(ErrText, __FUNCTION__, __LINE__));
      }
      // hier stehen alle Dateien eines Unterverzeichnisses zur Verfügung
      // ----------------------------------------------------------------
      unsigned long cSizeofall=0;                   // Speicherbelegung dieses Unterverzeichnisses
      struct dirent* cdirzeiger;
      int numjpgs = 0;
      char* eventbeg = strrchr(cDirname, EVBEG);    // Ereignis-Namen suchen ...

      while((cdirzeiger=readdir(cdir)) != NULL)     // Zeiger auf den Inhalt diese Unterverzeichnisses
      {
        // jede Datei ansehen
        // ------------------
        if (((*cdirzeiger).d_type) == DT_REG)
        { // reguläre Datei
          // --------------
          struct stat cAttribut;
          char cFilename[ZEILE];
          sprintf(cFilename,"%s/%s", cDirname, (*cdirzeiger).d_name);
          if(stat(cFilename, &cAttribut) == -1)           // Datei-Attribute holen
          { // -- Error
            sprintf(ErrText, "read attribut '%s'", cFilename);
            return (LogError(ErrText, __FUNCTION__, __LINE__));
          }
          // hier stehen die Attribute jeder Datei einzeln zur Verfügung
          // -----------------------------------------------------------
          if (getFiletyp(cFilename) == AVI)
          // ================================
          { // Format: >/home/pi/motion/pics/event_0061/2014-09-06_09.39.05-61.avi<
            char Verzeichnis[ZEILE];                				// Zielverzeichnis
            strcpy(Verzeichnis, target);
            char Dateikopie[ZEILE];
            char* avibeg = strrchr(cFilename, EVBEG);     // Anfangs-Markierung suchen ...
            char* aviend = strrchr(cFilename, EVEND);     // Ende-Markierung suchen ...
            *aviend = '\0';                               // ... und als Dateiende setzen
            strcat(Verzeichnis, avibeg);                  // neues AVI-Tages-Verzeichnis
            *aviend = EVEND;                              // wieder reparieren

            int errsave = 0;
            int verz = mkdir(Verzeichnis, MODUS);         // Verzeichnis erstellen
            if (verz != 0)
            {  // -- Error
              errsave = errno;
              if (errsave != EEXIST)                        // ... der darf!
              { // -- Error
                sprintf(ErrText, "mkdir '%s'", Verzeichnis);
                return (LogError(ErrText, __FUNCTION__, __LINE__));
              }
            }
            if (errsave == 0)
            {
#ifdef _DEBUG
              printf(" ------ AVI-Verzeichnis angelegt '%s' \n", Verzeichnis);
#endif
              sprintf(Logtext, ">> %s()#%d: Verzeichnis '%s' angelegt\n", __FUNCTION__, __LINE__, Verzeichnis);
              syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
            }

            sprintf(lastItem, "%s", aviend+1);
            sprintf(Dateikopie, "%s%c%s", Verzeichnis, '/', lastItem);  // nur Uhrzeit und Event im Dateinamen
#ifdef _DEBUG
            printf(" ------ Avi-Datei kopieren '%s' \n", Dateikopie);
#endif
            int copy = copyFile(Dateikopie, cFilename);
            if (copy != EXIT_SUCCESS)
            { // -- Error
              sprintf(ErrText, "copyFile '%s'", Dateikopie);
              return (LogError(ErrText, __FUNCTION__, __LINE__));
            }
						aviCount++;
          }
          else if (getFiletyp(cFilename) == JPG)
          // ===================================
          { // Format:  cp -r event_0013 /media/fritz-nas/SMI-USBDISK-01/motion  -  2014-10-23_14.00.23-13.05
            char Originalname[ZEILE];                       // Ereignisverzeichnis
            sprintf(Originalname, "%s", cFilename);
#ifdef _DEBUG
            printf("#%d -- JPG-Datei: '%s' \n", __LINE__, Originalname);
#endif
            numjpgs++;
						char hostname[ZEILE];
            char DVerzeichnis[ZEILE];               				// Datumsverzeichnis
            strcpy(DVerzeichnis, target);
            char EVerzeichnis[ZEILE];                       // Ereignisverzeichnis
            char Dateikopie[ZEILE];
            char* jpgbeg = strrchr(cFilename, '/');       // Anfangs-Markierung suchen ...
            char* jpgend = strrchr(cFilename, '_');       // Ende-Markierung suchen ...
            *jpgend = '\0';                               // ... und als Dateiende setzen
            strcat(DVerzeichnis, jpgbeg);                 // neues JPG-Tages-Verzeichnis
            *jpgend = EVEND;                              // wieder reparieren
            int errsave = 0;
            int verz = mkdir(DVerzeichnis, MODUS);         // Datumsverzeichnis erstellen
            if (verz != 0)
            {  // -- Error
              errsave = errno;
              if (errsave != EEXIST)                        // ... der darf!
              { // -- Error
                sprintf(ErrText, "mkdir '%s'", __FUNCTION__, __LINE__, DVerzeichnis);
                return (LogError(ErrText, __FUNCTION__, __LINE__));
              }
            }
            if (errsave == 0)
            {
#ifdef _DEBUG
              printf(" ---- Datums-Verzeichnis angelegt '%s' \n", DVerzeichnis);
#endif
              sprintf(Logtext, ">> %s()#%d: Verzeichnis '%s' angelegt\n", __FUNCTION__, __LINE__, DVerzeichnis);
              syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
            }
            sprintf(EVerzeichnis, "%s/%s", DVerzeichnis, eventbeg+1);
            errsave = 0;
            verz = mkdir(EVerzeichnis, MODUS);              // Ereignisverzeichnis erstellen
            if (verz != 0)
            {  // -- Error
              errsave = errno;
              if (errsave != EEXIST)                        // ... der darf!
              { // -- Error
                sprintf(ErrText, "mkdir '%s'", EVerzeichnis);
                return (LogError(ErrText, __FUNCTION__, __LINE__));
              }
            }
            if (errsave == 0)
            {
#ifdef _DEBUG
              printf(" ---- Ereignis-Verzeichnis angelegt '%s' \n", EVerzeichnis);
#endif
              sprintf(Logtext, ">> %s()#%d: Verzeichnis '%s' angelegt\n", __FUNCTION__, __LINE__, EVerzeichnis);
              syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
            }
            char* jpgevt = strrchr(jpgend+1, '-');      // Trenner suchen ...
            *jpgevt = 0;
            sprintf(Dateikopie, "%s%c%s", EVerzeichnis, '/', jpgend+1);  // nur Uhrzeit im Dateinamen
            char* jpgnum = strrchr(jpgevt+1, '.');      // Trenner suchen ...
            strcat(Dateikopie, jpgnum);                 // nur Uhrzeit und lfd. Nummer im Dateinamen
#ifdef _DEBUG
            printf("#%d ---- Dateikopie '%s' \n", __LINE__, Dateikopie);
            printf("#%d ---- Quell-Datei: '%s' \n", __LINE__, Originalname);
#endif
            int copy = copyFile(Dateikopie, Originalname);
            if (copy != EXIT_SUCCESS)
            { // -- Error
              sprintf(ErrText, "%s()#%d: copyFile '%s'", Dateikopie);
              return (LogError(ErrText, __FUNCTION__, __LINE__));
            }
#ifdef _DEBUG
            printf(" ------ JPG-Datei kopiert '%s' \n", Dateikopie);
#endif
						jpgCount++;
          }
        }
      }
      if (closedir(cdir) != 0)
      { // -- Error
        sprintf(ErrText, "closedir '%s'", cDirname);
        return (LogError(ErrText, __FUNCTION__, __LINE__));
      }
//      if (strlen(lastItem) > 0)
//      {
//        char Logtext[ZEILE];
//        sprintf(Logtext, ">> %s(): %s + %d JPG-Dateien\n", __FUNCTION__, lastItem, numjpgs);
//        strcat(MailBody, Logtext);
//      }
    }
  }
  if (closedir(pdir) != 0)
  { // -- Error
    sprintf(ErrText, "closedir '%s'", Pfad);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
#ifdef _DEBUG
	printf("%s()#%d: exit function +++++++++++++++++++++++++++++++++++\n", __FUNCTION__, __LINE__);
#endif
  return EXIT_SUCCESS;
}
//***********************************************************************************************
//***********************************************************************************************

int main(int argc, char *argv[])
{
  ProgName = strrchr(argv[0], '/') + 1;
  sprintf (Version, "Vers. %d.%d.%d - %s", MAXVERS, MINVERS, BUILD, __DATE__);
  openlog(ProgName, LOG_PID, LOG_LOCAL7 ); // Verbindung zum Dämon Syslog aufbauen
  syslog(LOG_NOTICE, ">>> %s - %s - PID %d - User %d/%d - Group %d/%d",
                          ProgName, Version, getpid(), geteuid(),getuid(), getegid(),getgid());

  char puffer[BUF];
  char Logtext[ZEILE];
  char Logtext1[ZEILE];
	char hostname[ZEILE];
  int fd;
  int status;

	// Ist GPIO klar?
	// --------------
  int gpioStatus = resetLED(FIFOMOTION);
  if (gpioStatus < 0)
  {
  	fprintf(stderr, "pigpio initialisation failed.\n");
  	return 1;
  }
  switchLED(FIFOMOTION, ORANGE);

  status = gethostname(hostname, ZEILE);					// Host ermitteln
  if (status < 0)
  { // -- Error
    sprintf(ErrText, "gethostname '%s'", hostname);
    return (LogError(ErrText, __FUNCTION__, __LINE__));
  }
	sprintf(target, "%s/%s", FRITZ, hostname);	// Speicherziel
#ifdef _DEBUG
              printf(" ---- Speicherziel '%s' \n", target);
#endif

  savePID();                                	// PID ablegen

  // named pipe(Fifo) erstellen
  // --------------------------
  umask(0);
  status = mkdir(FDIR, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (mkfifo (FIFO, 0666) < 0)
  {
    if(errno == EEXIST)                     	// FIFO bereits vorhanden - kein fataler Fehler
      printf(" -- FIFO bereits vorhanden\n");
    else
    {
      sprintf(ErrText, "mkfifo(%s)", FIFO);
      LogError(ErrText, __FUNCTION__, __LINE__);
      exit (EXIT_FAILURE);
    }
  }
  sprintf(Logtext, ">> %s()#%d: Start '%s %s' - User %d/%d - Group %d/%d\n",
                     __FUNCTION__, __LINE__, argv[0], Version, geteuid(),getuid(), getegid(),getgid());
  syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
	printf(Logtext);

  // Fifo zum Lesen öffnen
  // ---------------------
	sprintf(Logtext, ">> %s()#%d: Open Fifo !\n",  __FUNCTION__, __LINE__);
	syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
	printf(Logtext);
	switchLED(FIFOMOTION, GRUEN);
  sprintf(Logtext, "open !(%s)\n", FIFO);
	printf(Logtext);
  fd = open (FIFO, O_RDONLY);                     // Empfänger liest nur aus dem FIFO
  sprintf(Logtext, "open ?(%s)\n", FIFO);
 	printf(Logtext);
 if (fd == -1)
  {
		sprintf(Logtext, ">> %s()#%d: Errror Open Fifo !\n",  __FUNCTION__, __LINE__);
		printf(Logtext);
    LogError(Logtext, __FUNCTION__, __LINE__);
    exit (EXIT_FAILURE);
  }
  syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
	printf(Logtext);

	sprintf(Logtext, ">> %s()#%d: Ready !\n",  __FUNCTION__, __LINE__);
	syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);
	printf(Logtext);
  while (true) // ========== Endlosschleife ===============================================
  {
    if ( read(fd, puffer, BUF) )                      // auf Aufträge von 'motion' warten
    {
      time_t OrderStart = time(NULL);
  		switchLED(FIFOMOTION, ORANGE);
      char header[ZEILE];
      {
        struct tm *ts = localtime(&OrderStart);
        sprintf(header, ">> %s()#%d: ------ neuer Auftrag @ %d.%d.%d %2d:%2d:%2d -----\n",
                         __FUNCTION__, __LINE__,
                        ts->tm_mday, ts->tm_mon, ts->tm_year+1900, ts->tm_hour, ts->tm_min, ts->tm_sec);
#ifdef _DEBUG
      	printf(header);                                                          // <<<<<<<<<<<<<<<<<<<<
#endif
			}
      sprintf(MailBody, header);
      sprintf(Logtext, header);
      syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);

			aviCount=0;
			jpgCount=0;
      if (copyAvi(puffer, DES) != EXIT_SUCCESS)       // .avi isolieren
        break;
  		switchLED(FIFOMOTION, GRUEN);
      if (delOldest(puffer) != EXIT_SUCCESS)          // aufräumen
        break;
  		switchLED(FIFOMOTION, ORANGE);
      if (calcSize(puffer) != EXIT_SUCCESS)           // anzeigen
        break;

      int Dauer = time(NULL) - OrderStart;
      sprintf(Logtext, ">> %s()#%d: -- kopiert: *.avi: %d -- *.jpg: %d -------\n",
                                    __FUNCTION__, __LINE__, aviCount, jpgCount);
      sprintf(Logtext1, ">> %s()#%d: -- done %s ---> '%s' Dauer %d sec - OK --------\n",
                                    __FUNCTION__, __LINE__, ProgName, newItem, Dauer);
      syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext1);
      strcat(Logtext, Logtext1);

    	sprintf(Logtext1, ">> %s()#%d: Ready !\n",  __FUNCTION__, __LINE__);
    	syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext1);
    	switchLED(FIFOMOTION, GRUEN);
    }
 		sleep(1);
  } // =================================================================================

  // Fehler-Mail abschicken
  // ----------------------
  sprintf(Logtext, ">> %s()#%d: Error %s ---> '%s' OK\n",__FUNCTION__, __LINE__, ProgName, lastItem);
  syslog(LOG_NOTICE, "%s: %s", __FIFO__, Logtext);

  strcat(MailBody, Logtext);
  char Betreff[ZEILE];
  sprintf(Betreff, "Error-Message von %s: >>%s<<", ProgName, lastItem);
  sendmail(Betreff, MailBody);                        // Mail-Message absetzen
	switchLED(FIFOMOTION, ROT);                       	// -- LED Alarm ein
}
//***********************************************************************************************
