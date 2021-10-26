//*********************************************************************************************//
//*                                                                                           *//
//* File:          error.c                                                                    *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2018-07-14;                                                                *//
//* Last change:   2021-09-15 - 09:50:35                                                      *//
//* Description:   Nistkastenprogramm: Standard-Fehlerausgänge                                *//
//*                                                                                           *//
//* Copyright (C) 2018-21 by Wolfgang Keuch                                                   *//
//*                                                                                           *// 
//*********************************************************************************************//

#include <stdio.h>
#include <mysql.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <stdbool.h>

#include "./error.h"
#include "./datetime.h"
#include "../sendmail/sendMail.h"

EXTERN bool DBError_Flag;
EXTERN int  DBError_Count;

#undef      MIT_DISPLAY

//#define _DEBUG

//*********************************************************************************************//

#define FATAL_MESSAGE        "Programm-Abbruch in '%s':\n%s\n-wkh-\n"
#define NONFATAL_MESSAGE     "Fehler in '%s':\n%s\n-wkh-\n"

// Fehlermeldung ausgeben
// ------------------------

void show_Error(char* ErrorMessage, char* MailMessage)
{
  char Header[SUBJECT] = {'\0'};
  char MailBody[BODYLEN] = {'\0'};

  char ErrText[ERRBUFLEN]={'\0'};
  sprintf(ErrText, "Error! <%s>", ErrorMessage);
  sprintf(ErrText, "%s<%s>", strlen(MailMessage) == 0 ? "" : "Error! ", ErrorMessage);
  syslog(LOG_NOTICE, ErrText);                            // im Log vermerken

  if (strlen(MailMessage) > 0)
  {
    // Fehler-Mail abschicken
    // ----------------------
    sprintf(Header, "Error in >%s<", PROGNAME);
    Header[SUBJECT-1]='\0';                               // Begrenzung
    #ifdef _DEBUG                                         // Fehler ausgeben
    fprintf(stdout, "-- %s()#%d - Header: '%s'\n", __FUNCTION__, __LINE__, Header);
    #endif
  
    sprintf(MailBody, MailMessage, PROGNAME, ErrorMessage);
    MailBody[BODYLEN-1] = '\0';                           // Begrenzung
    #ifdef _DEBUG                                         // Fehler ausgeben
    fprintf(stdout, "-- %s()#%d - Body: >>>\n%s\n<<<\n", __FUNCTION__, __LINE__, MailBody);
    fprintf(stdout, "-- ----------------------------------------------\n");
    #endif
  
    sendmail(Header, MailBody);                           // Mail-Message absetzen
  }
}
//*********************************************************************************************//

// Fatale Fehlermeldung
// ------------------------
// Programm wird beendet

void finish_with_Error(char* ErrorMessage)
{

#ifdef _DEBUG                                             // Fehler ausgeben
  fprintf(stdout, "\n-- ---------- finish_with_Error() ---------------\n");
  fprintf(stdout, "-- %s()#%d: '%s'\n", __FUNCTION__, __LINE__, ErrorMessage);
#endif

  // Fehlerausgabe
  // --------------
  show_Error(ErrorMessage, FATAL_MESSAGE);
  syslog(LOG_NOTICE, "Exit!");                            // im Log vermerken
  closelog();                                             // Verbindung zum Dämon Syslog schließen

#ifdef MIT_DISPLAY
  // Damit die Anzeige erhalten bleibt: Dauerschleife
  // ------------------------------------------------
  DO_FOREVER
  {
    sleep(1000);
  }
#endif

  exit(EXIT_FAILURE);
}
//*********************************************************************************************//

// Nicht-Fatale Fehlermeldung
// ------------------------
// Fehler wird gemeldet, Programm läuft weiter

void report_Error(char* ErrorMessage, bool withMail)
{
  // Fehlerausgabe
  // --------------
  if (withMail)
    show_Error(ErrorMessage, NONFATAL_MESSAGE);
  else
  {
    show_Error(ErrorMessage, "");   
  }
}
//*********************************************************************************************//
