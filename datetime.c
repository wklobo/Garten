//***********************************************************************************************************//
//*                                                                                                         *//
//* File:          datetime.c                                                                               *//
//* Author:        Alexander Müller                                                                         *//
//*                aus 'http://www.a-m-i.de/tips/datetime/datetime.php'                                     *//
//* Creation date: 2018-11-01;                                                                              *//
//* Last change:   2021-09-10 - 13:49:15: neuer Ansatz                                                      *//
//* Description:   In diesem Artikel habe ich eine Sammlung verschiedener Funktionen zusammengestellt,      *//
//*                mit denen man alle möglichen Arten von Kalender- und Zeitberechnungen durchführen kann.  *//
//*                Es ist eine Sammlung einzelner Routinen. Es findet relativ wenig Fehlerbehandlung von    *//
//*                unsinnigen Eingangsdaten statt, nur soviel wie nötig ist, um stabil zu laufen.           *//
//*                Ansonsten gilt: "Garbage in, Garbage out!"                                               *//
//*                                                                                                         *//
//*                2018-11-02 - An eigene Bedürfnisse angepasst -- wkh                                      *//
//*                                                                                                         *//
//***********************************************************************************************************//

#define __DATETIME_DEBUG__ false

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/time.h>

#include "./datetime.h"

//***************************************************************************//
/*
 * Define debug function.
 * ---------------------
 */
#if __DATETIME_DEBUG__
#define DEBUG(...)  printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

//**********************************************************************************//

// Laufzeitmessung
// ================

struct timeval t1[TIMER], t2;

// Zeitmessung starten
// ---------------------
bool Startzeit(int Timer)
{
  if (Timer < TIMER)
  {
    gettimeofday(&t1[Timer], NULL);   // Startwert
    return true;
  }
  return false;
}

// Zeit nehmen
// ------------
// gibt Zeit im [msec] zurück
long Zwischenzeit(int Timer)
{
  long long elapsedTime;
  gettimeofday(&t2, NULL);    // Endwert

  if (Timer < TIMER)
  {
  // verbrauchte Zeit in Microsekunden
    // ---------------------------------
    elapsedTime = ((t2.tv_sec * 1000000) + t2.tv_usec)
                - ((t1[Timer].tv_sec * 1000000) + t1[Timer].tv_usec);

    // Rückgabe in Millisekunden
    // -------------------------
    return ((elapsedTime+500) / 1000);
  }
  return 0;
}
//**********************************************************************************//

// aktuelles Datum als String
// -------------------------
// Rückgabe: 'tt.mm.jj'
char* heute(char* buf)
{
  time_t tnow;
  struct tm *tmnow;

  time(&tnow);
  tmnow = localtime(&tnow);
  sprintf(buf, "%02d.%02d.%02d",
      tmnow->tm_mday, tmnow->tm_mon + 1, tmnow->tm_year + 1900);
  return buf;
}

//**********************************************************************************//

// aktuelle Uhrzeit als String
// ---------------------------
// Rückgabe: 'hh:mm:ss'
char* jetzt(char* buf)
{
  time_t tnow;
  struct tm *tmnow;

  time(&tnow);
  tmnow = localtime(&tnow);
  sprintf(buf, "%02d:%02d:%02d%c",
      tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec, '\0');
  return buf;
}
//**********************************************************************************//

// Datum/Uhrzeit aus time_t(=long)
// ------------------------------
// Rückgabe: 'tt.mm.jjjj hh:mm:ss'
char* mkdatum(time_t zeit, char* buf)
{
  struct tm *tmnow;

  tmnow = localtime(&zeit);
  sprintf(buf, "%02d.%02d.%04d %02d:%02d:%02d",
      tmnow->tm_mday, tmnow->tm_mon + 1, tmnow->tm_year + 1900,
      tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
  return buf;
}
//**********************************************************************************//

// liegt aktuelle Zeit in einem Zeitfenster?
// -----------------------------------------
// Format Beginn/Ende: 'hh:mm'
bool Zeitfenster(char* Beginn, char* Schluss)
{
  time_t tnow;
  struct tm *tmnow;
  char buf[20];

  time(&tnow);
  tmnow = localtime(&tnow);
  int jetzt = (tmnow->tm_hour * 60) + tmnow->tm_min;    // aktuelle Zeit in [min]
  int beg = (atoi(strncpy(buf, Beginn, 2)) * 60) + atoi(strncpy(buf, Beginn+3, 2));
  int end = (atoi(strncpy(buf, Schluss, 2)) * 60) + atoi(strncpy(buf, Schluss+3, 2));
  bool retval = ((jetzt >= beg) && (jetzt <= end));
  DEBUG("+ %s()#%d: jetzt: '%d', beg: '%d', end: '%d' --> '%d'\n",
                                    __FUNCTION__, __LINE__, jetzt, beg, end, retval);

  return retval;
}
//**********************************************************************************//

// Grundlagen: Berechnen, ob ein bestimmtes Jahr ein Schaltjahr ist:
// -----------------------------------------------------------------
bool istSchaltjahr(const UINT uJahr)
{
  // Die Regel lautet: Alles, was durch 4 teilbar ist, ist ein Schaltjahr.
  // Es sei denn, das Jahr ist durch 100 teilbar, dann ist es keins.
  // Aber wenn es durch 400 teilbar ist, ist es doch wieder eins.

  if ((uJahr % 400) == 0)
    return true;
  else if ((uJahr % 100) == 0)
    return false;
  else if ((uJahr % 4) == 0)
    return true;

  return false;
}
//**********************************************************************************//
//
//// Grundlagen: Simpler Einzeiler für Visual Basic (Tip von Andy Grothe):
//
//
//Public Function istSchaltjahr(ByVal strJahr As String) As Boolean
//' Übergabe:  strJahr, das zu überprüfende Jahr im Format YYYY
//' RÜckgabe:  True - wenn Schaltjahr, sonst False
//On Error Resume Next
//
//  If isDate("29.02." & strJahr) Then
//    IsSchaltjahr = True
//  Else
//    IsSchaltjahr = False
//  End If
//
//End Function
//**********************************************************************************//

// Grundlagen: Bestimmung der Anzahl Tage pro Monat:
// -------------------------------------------------
short getAnzahlTageImMonat(const UINT uMonat, const UINT uJahr)
{
  //                     ungült,Jan,Feb,Mrz,Apr,Mai,Jun,Jul,Aug,Sep,Okt,Nov,Dez
  int arrTageImMonat[13] = {  0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  if (uMonat == 2)
  {
    // Februar: Schaltjahr unterscheiden
    if (istSchaltjahr(uJahr))
      return 29;
    else
      return 28;
  }

  if ((uMonat >= 1) && (uMonat <= 12))
    return arrTageImMonat[uMonat];
  else
  {
    _ASSERT(0); // ungültiger Monat !
    return 0;
  }
}
//**********************************************************************************//

// ermittelt den nächsten Tag des übergebenen SQL-DATETIME-Werts
// -------------------------------------------------------------
// DATETIME   '0000-00-00 00:00:00'
// DATE       '0000-00-00'

bool nextDay(const DATETIME Input, DATE Nextday)
{
  UINT uTag=0;
  UINT uMonat=0;
  UINT uJahr=0;
  const DATETIME Thisday=Input;

#ifdef _DEBUG_
  fprintf(stdout, "%s()#%d: - Thisday='%s', Nextday='%s'\n",__FUNCTION__,__LINE__, Thisday, Nextday);
#endif

  while(*Thisday)
  {
    if(isdigit(*Thisday))
      uJahr = uJahr*10 + (*Thisday & 0x0f);
    else
      break;
    Thisday++;
  }
  Thisday++;
  while(*Thisday)
  {
    if(isdigit(*Thisday))
      uMonat = uMonat*10 + (*Thisday & 0x0f);
    else
      break;
    Thisday++;
  }
  Thisday++;
  while(*Thisday)
  {
    if(isdigit(*Thisday))
      uTag = uTag*10 + (*Thisday & 0x0f);
    else
      break;
    Thisday++;
  }
  uTag++;
  if (uTag > getAnzahlTageImMonat(uMonat, uJahr))
  {
    uTag = 1;
    uMonat++;
    if(uMonat > 12)
    {
      uMonat = 1;
      uJahr++;
    }
  }
  sprintf(Nextday, "%04d-%02d-%02d", uJahr, uMonat, uTag);

#ifdef _DEBUG_
  fprintf(stdout, "%s()#%d: - Thisday='%s' -> %d.%d.%d -> Nextday='%s'\n",__FUNCTION__,__LINE__, Input, uTag, uMonat, uJahr, Nextday);
#endif

  return true;
}
//**********************************************************************************//

// Jetzt wirds kompliziert: Die Anzahl der Tage eines Jahres:
// ----------------------------------------------------------
short getAnzahlTageImJahr(const UINT uJahr)
{
  return (istSchaltjahr(uJahr)) ? 366 : 365;
}
//**********************************************************************************//

// Bestimmung des Wochentags für ein Datum:
// ----------------------------------------
short getWochentag(const UINT uTag, const UINT uMonat, const UINT uJahr)
{
//                       ungült Jan Feb Mrz Apr Mai Jun Jul Aug Sep Okt Nov Dez
BYTE arrMonatsOffset[13] = {  0,  0,  3,  3,  6,  1,  4,  6,  2,  5,  0,  3,  5};

  short nErgebnis = 0;

  _ASSERT(uTag > 0);
  _ASSERT(uTag <= 31);
  _ASSERT(uMonat > 0);
  _ASSERT(uMonat <= 12);

  // Monat / Tag - Plausi pr?fen:
  if ((uTag > 31) || (uMonat > 12) || (uMonat <= 0)
      || (uTag <= 0) || (uJahr <= 0))
  {
    return -1;
  }

  BYTE cbTagesziffer = (uTag % 7);
  BYTE cbMonatsziffer = arrMonatsOffset[uMonat];
  BYTE cbJahresziffer = ((uJahr % 100) + ((uJahr % 100) / 4)) % 7;
  BYTE cbJahrhundertziffer = (3 - ((uJahr / 100) % 4)) * 2;

  // Schaltjahreskorrektur:
  if ((uMonat <= 2) && (istSchaltjahr(uJahr)))
    cbTagesziffer = cbTagesziffer + 6;

  nErgebnis = (cbTagesziffer + cbMonatsziffer + cbJahresziffer + cbJahrhundertziffer) % 7;

  // Ergebnis:
  // 0 = Sonntag
  // 1 = Montag
  // 2 = Dienstag
  // 3 = Mittwoch
  // 4 = Donnerstag
  // 5 = Freitag
  // 6 = Samstag
  return nErgebnis;
}
//**********************************************************************************//

// Der wievielte Tag des Jahres ist ein bestimmtes Datum :
// -------------------------------------------------------
short getTagDesJahres(const UINT uTag, const UINT uMonat, const UINT uJahr)
{
  // Der wievielte Tag des Jahres ist dieser Tag
  if ((uMonat == 0) || (uMonat > 12))
  {
    _ASSERT(0);
    return -1;
  }

  UINT uLokalTag = uTag;
  UINT uLokalMonat = uMonat;

  while (uLokalMonat > 1)
  {
    uLokalMonat--;
    uLokalTag += getAnzahlTageImMonat(uLokalMonat, uJahr);
  }

  return uLokalTag;
}
//**********************************************************************************//

// Fortschrittliches: Kalenderwoche, Ostern, Advent
// Die Kalenderwoche wird nach der Industrienorm DIN 1355 berechnet:

short getKalenderwoche(short uTag, short uMonat, short uJahr)
{
  // Berechnung erfolgt analog DIN 1355, welche besagt:
  // Der erste Donnerstag im neuen Jahr liegt immer in der KW 1.
  // "Woche" ist dabei definiert als [Mo, ..., So].

  short nTagDesJahres = getTagDesJahres(uTag, uMonat, uJahr);

  // Berechnen des Wochentags des 1. Januar:
  short nWochentag1Jan = getWochentag(1, 1, uJahr);

  // Sonderfälle Freitag und Samstag
  if (nWochentag1Jan >= 5)
    nWochentag1Jan = nWochentag1Jan - 7;

  // Sonderf?lle "Jahresanfang mit KW - Nummer aus dem Vorjahr"
  if ( (nTagDesJahres + nWochentag1Jan) <= 1)
  {
    return getKalenderwoche(31, 12, uJahr - 1);
  }

  short nKalenderWoche = ((nTagDesJahres + nWochentag1Jan + 5) / 7);

  _ASSERT(nKalenderWoche >= 1);
  _ASSERT(nKalenderWoche <= 53);

  // 53 Kalenderwochen hat grundsätzlich nur ein Jahr,
  // welches mit einem Donnerstag anfängt !
  // In Schaltjahren ist es auch mit einem Mittwoch möglich, z.B. 1992
  // Andernfalls ist diese KW schon die KW1 des Folgejahres.
  if (nKalenderWoche == 53)
  {
    bool bIstSchaltjahr = istSchaltjahr(uJahr);

    if (  (nWochentag1Jan  ==  4) // Donnerstag
      ||  (nWochentag1Jan  == -3) // auch Donnerstag
      ||  ((nWochentag1Jan ==  3) && bIstSchaltjahr)
      ||  ((nWochentag1Jan == -4) && bIstSchaltjahr)
      )
    {
      ; // Das ist korrekt und erlaubt
    }
    else
      nKalenderWoche = 1; // Korrektur des Wertes
  }

  return nKalenderWoche;
}
//**********************************************************************************//

// Auch der Ostersonntag kann mathematisch berechnet werden,
// ---------------------------------------------------------
// und damit auch alle anderen Feiertage, die um den Ostersonntag drumherum liegen:
void getOsterdatum(const UINT uJahr, UINT* uTag, UINT* uMonat)
{
  // Berechnet für ein beliebiges Jahr das Osterdatum.

  // Quelle des Gauss - Algorithmus: Stefan Gerth,
  // "Die Gauß'sche Osterregel", Nürnberg, Februar 2003.
  // http://krapfen.org/content/paper/Schule/Facharbeit/Berechnung_des_Osterfestes.pdf

  UINT a = uJahr % 19;
  UINT b = uJahr %  4;
  UINT c = uJahr %  7;

  int k = uJahr / 100;
  int q = k / 4;
  int p = ((8 * k) + 13) / 25;
  UINT Egz = (38 - (k - q) + p) % 30; // Die Jahrhundertepakte
  UINT M = (53 - Egz) % 30;
  UINT N = (4 + k - q) % 7;

  UINT d = ((19 * a) + M) % 30;
  UINT e = ((2 * b) + (4 * c) + (6 * d) + N) % 7;

  // Ausrechnen des Ostertermins:
  if ((22 + d + e) <= 31)
  {
    *uTag = 22 + d + e;
    *uMonat = 3;
  }
  else
  {
    *uTag = d + e - 9;
    *uMonat = 4;

    // Zwei Ausnahmen berücksichtigen:
    if (*uTag == 26)
      *uTag = 19;
    else if ((*uTag == 25) && (d == 28) && (a > 10))
      *uTag = 18;
  }

  // Offsets für andere Feiertage:

  // Schwerdonnerstag / Weiberfastnacht -52
  // Rosenmontag -48
  // Fastnachtsdienstag -47
  // Aschermittwoch -46
  // Gründonnerstag -3
  // Karfreitag -2
  // Ostersonntag 0
  // Ostermontag +1
  // Christi Himmelfahrt +39
  // Pfingstsonntag +49
  // Pfingstmontag +50
  // Fronleichnam +60

  // Mariä Himmelfahrt ist stets am 15. August (Danke an Michael Plugge!)

}
//**********************************************************************************//

// Hier noch die Bestimmung der Adventssonntage und des Buß- und Bettages,
// -----------------------------------------------------------------------
// danke an Andy Grothe für den Hinweis:

void getViertenAdvent(const UINT uJahr, UINT* uTag, UINT* uMonat)
{
  // Berechnet f?r ein beliebiges Jahr das Datum des 4. Advents-Sonntags.
  // Der 4. Adventssonntag ist stets der Sonntag vor dem 1. Weihnachtsfeiertag,
  // mu? also stets in der Periode [18. - 24.12.] liegen:

  *uMonat = 12; // Das steht jedes Jahr fest :-)

  short nWoTag = getWochentag(24, 12, uJahr); // Wochentag des 24.12. ermitteln

  *uTag = 24 - nWoTag;

  // Offsets: Der Buß- und Bettag liegt stets 32 Tage vor dem  4. Advent
}
// ********************************************************************************** //

/*
Diese Funktionen wurden für eine Windows-Umgebung geschrieben, d.h. SYSTEMTIME muss
noch für LINUX angepasst werden. Deshalb hier erstmal auskommentiert.

2018-11-02 -- Wolfgang Keuch


// Messung von Zeitdifferenzen
// ---------------------------
// Mit der Windows - API - Funktion GetSystemTime() kann man sich die exakte Uhrzeit
// mitsamt Datum beschaffen, bis auf die Millisekunde genau. Leider gibt es keine
// Unterstützung für die Differenzbildung zweier solcher ermittelten Zeiten, daher
// gebe ich hier zwei Beispiele, wie dies programmiert werden kann. Dies funktioniert
// analog auch für GetLocalTime(), die Funktion, die auch noch Sommer- und Winterzeit
// berücksichtigt, und ebenfalls mit der SYSTEMTIME - Struktur arbeitet.

long ZeitDifferenzInJahren(const SYSTEMTIME* Startzeit,
                           const SYSTEMTIME* Endezeit)     // const ?
{
  if (Endezeit.wMonth > Startzeit.wMonth)
    return Endezeit.wYear - Startzeit.wYear;

  if (Endezeit.wMonth < Startzeit.wMonth)
    return Endezeit.wYear - Startzeit.wYear - 1;

  // Monate sind identisch.

  if (Endezeit.wDay > Startzeit.wDay)
    return Endezeit.wYear - Startzeit.wYear;

  if (Endezeit.wDay < Startzeit.wDay)
    return Endezeit.wYear - Startzeit.wYear - 1;

  // Tag ist bei beiden identisch.

  if (Endezeit.wHour > Startzeit.wHour)
    return Endezeit.wYear - Startzeit.wYear;

  if (Endezeit.wHour < Startzeit.wHour)
    return Endezeit.wYear - Startzeit.wYear - 1;

  // Stunde ist identisch.

  if (Endezeit.wMinute > Startzeit.wMinute)
    return Endezeit.wYear - Startzeit.wYear;

  if (Endezeit.wMinute < Startzeit.wMinute)
    return Endezeit.wYear - Startzeit.wYear - 1;

  if (Endezeit.wSecond > Startzeit.wSecond)
    return Endezeit.wYear - Startzeit.wYear;

  if (Endezeit.wSecond < Startzeit.wSecond)
    return Endezeit.wYear - Startzeit.wYear - 1;

  if (Endezeit.wMilliseconds >= Startzeit.wMilliseconds)
    return Endezeit.wYear - Startzeit.wYear;
  else
    return Endezeit.wYear - Startzeit.wYear - 1;
}
// ********************************************************************************** //


//////////////////////////////////////////////////////////////////////////////
// Die Zeitdifferenz in Monaten läßt sich nicht exakt ausrechnen,
// weil ein Monat nicht immer gleich lang ist. Beispiel:
// Zwischen dem 28.2. und dem 28.3. liegen nur 28 Tage.
// Ist das schon ein Monat (28. bis 28.) ????
// Der März ist erst am 31.3. zu Ende ! Ist das ein Monat (28.2. - 31.3.) ????
//////////////////////////////////////////////////////////////////////////////

long ZeitDifferenzInTagen(const SYSTEMTIME* Startzeit,
                          const SYSTEMTIME* Endezeit)
{
  // Ist die Ende-Uhrzeit auch hinter der Startuhrzeit? Das ist der Normalfall.
  // Andernfalls müssen wir nachher noch eins abziehen!

  long lAbzug = 0;

  if (  (Endezeit.wHour < Startzeit.wHour)
    ||  ((Endezeit.wHour == Startzeit.wHour)
          && (Endezeit.wMinute  < Startzeit.wMinute))
    ||  ((Endezeit.wHour == Startzeit.wHour)
          && (Endezeit.wMinute == Startzeit.wMinute)
          && (Endezeit.wSecond  < Startzeit.wSecond))
    ||  ((Endezeit.wHour == Startzeit.wHour)
          && (Endezeit.wMinute == Startzeit.wMinute)
          && (Endezeit.wSecond == Startzeit.wSecond)
          && (Endezeit.wMilliseconds < Startzeit.wMilliseconds))
     )
  {
    lAbzug = 1;
  }

  // Im gleichen Jahr ? Dann nutzen wir "getTagDesJahres":
  if (Endezeit.wYear == Startzeit.wYear)
    return getTagDesJahres(Endezeit.wDay, Endezeit.wMonth, Endezeit.wYear)
           - getTagDesJahres(Startzeit.wDay, Startzeit.wMonth, Startzeit.wYear)
           - lAbzug;

  else if (Endezeit.wYear > Startzeit.wYear)
  {
    // Wir starten mit der Anzahl Tage im Endejahr:
    long lErgebnis = getTagDesJahres(Endezeit.wDay, Endezeit.wMonth,
                                       Endezeit.wYear) - lAbzug;

    UINT uJahr = Endezeit.wYear - 1;

    // Jetzt summieren wir alle dazwischenliegenden Jahre:
    while (uJahr > Startzeit.wYear)
    {
      lErgebnis += getAnzahlTageImJahr(uJahr);
      uJahr--;
    }

    // und addieren schlie?lich noch die Anzahl der Tage aus dem Start - Jahr:
    lErgebnis += (getAnzahlTageImJahr(Startzeit.wYear)
                  - getTagDesJahres(Startzeit.wDay,
                                    Startzeit.wMonth, Startzeit.wYear));

    return lErgebnis;
  }

  else
    return -1; // Die Endezeit liegt vor der Startzeit !
}
// ********************************************************************************** //

long ZeitDifferenzInSekunden(const SYSTEMTIME* Startzeit, const SYSTEMTIME* Endezeit)
{
  __int64 lErgebnis = Endezeit.wSecond - Startzeit.wSecond;
  // lErgebnis kann durchaus negativ sein ! Wird aber gleich korrigiert.
  lErgebnis += (Endezeit.wMinute - Startzeit.wMinute) * 60;
  lErgebnis += (Endezeit.wHour - Startzeit.wHour) * 3600;

  // Im gleichen Jahr ? Dann nutzen wir "getTagDesJahres":
  if (Endezeit.wYear == Startzeit.wYear)
    return (long)
      (((getTagDesJahres(Endezeit.wDay, Endezeit.wMonth, Endezeit.wYear)
      - getTagDesJahres(Startzeit.wDay, Startzeit.wMonth, Startzeit.wYear)) * 86400)
      + lErgebnis);

  else if (Endezeit.wYear > Startzeit.wYear)
  {
    // Wir starten mit der Anzahl Tage im Endejahr
    lErgebnis += getTagDesJahres(Endezeit.wDay, Endezeit.wMonth, Endezeit.wYear) * 86400;

    UINT uJahr = Endezeit.wYear - 1;

    // Jetzt summieren wir alle dazwischenliegenden Jahre:
    while (uJahr > Startzeit.wYear)
    {
      lErgebnis += (getAnzahlTageImJahr(uJahr) * 86400);
      uJahr--;
    }

    // und addieren schließlich noch die Anzahl der Tage aus dem Start - Jahr:
    lErgebnis += ((getAnzahlTageImJahr(Startzeit.wYear)
            - getTagDesJahres(Startzeit.wDay,
                    Startzeit.wMonth, Startzeit.wYear)) * 86400);

    return (long) lErgebnis;
  }

  else
    return -1; // Die Endezeit liegt vor der Startzeit !
}
// ********************************************************************************** //

*/
