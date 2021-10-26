//*********************************************************************************************//
//*                                                                                           *//
//* File:          ventilator.c                                                               *//
//* Author:        Wolfgang Keuch                                                             *//
//* Creation date: 2020-09-19;                                                                *//
//* Last change:   2020-09-20;                                                                *//
//* Description:   Ventilator - Lüftersteuerung für Raspi "Wetter"                            *//
//*                                                                                           *//
//* Copyright (C) 2020 by Wolfgang Keuch                                                      *//
//*                                                                                           *//
//*********************************************************************************************//

#define _POSIX_SOURCE 1   /* POSIX compliant source */
#define _DEFAULT_SOURCE
#define _MODUL0

#include <time.h>
#include <stdio.h>
#include <mysql.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h> 
#include <unistd.h>
#include <stdbool.h>

#include "./gpio.h"
#include "./error.h"
#include "./version.h"
#include "./datetime.h"
#include "./checkproc.h"
#include "./ventilator.h"
#include "../sendmail/sendMail.h"

#define _DEBUG_ 1

// ****************************************************************************************************** //

// lokale Fehlermeldung
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void showMain_Error( char* Message, const char* Func, int Zeile)
{
  char ErrText[ERRBUFLEN];
  sprintf( ErrText, "%s()#%d in %s: \"%s\"", Func, Zeile,__FILE__, Message);

  finish_with_Error(ErrText);
}
// ****************************************************************************************************** //

#define CPU_TEMP  "cat /sys/class/thermal/thermal_zone0/temp"

double get_cputemp()
{
	FILE *pf;
	char command[100];
	char data[BUFLEN];
        
	sprintf(command, CPU_TEMP); 				// Execute a process listing
 	pf = popen(command,"r"); 						// Setup our pipe for reading and execute our command.
	fgets(data, BUFLEN, pf); 						// Get the data from the process execution
	// the data is now in 'data'
	// ------------------------
	if (pclose(pf) != 0)
		fprintf(stderr," Error: Failed to close command stream \n");
#ifdef _DEBUG_
 	fprintf(stdout, "%s-%s#%d - Temperaturwert=%s",jetzt(cNow),__FUNCTION__,__LINE__, data);
#endif
  return (atoi(data));
}
// ****************************************************************************************************** //

int main(int argc, char **argv)
{
  openlog(PROGNAME, LOG_PID, LOG_LOCAL7 );  // Verbindung zum Dämon Syslog aufbauen
  sprintf (Version, "Vers. %d.%d.%d - %s-%s", MAXVERS, MINVERS, BUILD, __DATE__, __TIME__);

  char PName[BUFLEN];                     	// Buffer für Programmnamen
//  time_t now;
//  struct tm *timenow;
  
  // Start-Mail senden
  // -----------------
  sprintf(PName, "Start %s %s @ %s", PROGNAME, Version, __NOW__);
  char MailBody[MAILBUFLEN]={'\0'};
  sprintf(MailBody, "Start %s %s - @ %s\n", argv[0], Version, __NOW__);
  sendmail(PName, MailBody);                    // Mail-Message absetzen

  // Watchdog bedienen
  // -----------------
  feedWatchdog(PROGNAME);

	// Hardware-Initialisierung
	// ------------------------
	gpio_Init();
	switchFan(ON, OBERGRENZE+1);		// nur mal so zur Probe
			
	DO_FOREVER
	{
		// Lüftersteuerung
		// ---------------
    double cpu_temperatur = get_cputemp()/1000;
#ifdef _DEBUG_
 		fprintf(stdout, "%s-%s#%d - CPU-Temperatur='%2.1f'°C\n",jetzt(cNow),__FUNCTION__,__LINE__, cpu_temperatur);
#endif
    if (cpu_temperatur >= OBERGRENZE)
    {
			switchFan(ON, cpu_temperatur);
    }
    else if (cpu_temperatur < UNTERGRENZE)
    {
			switchFan(OFF, cpu_temperatur);
    }

  	// Watchdog bedienen
  	// -----------------
  	feedWatchdog(PROGNAME);
		
		// Nickerchen
		// ------------
		usleep(100*ABFRAGETAKT);
	}
}
// ****************************************************************************************************** //
