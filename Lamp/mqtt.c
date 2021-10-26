//*********************************************************************************************//
//*                                                                                           *//
//* File:          mqtt.c                                                                     *//
//* Author:        siehe unten                                                                *//
//* Creation date: 2019-05-26;                                                                *//
//* Last change:   2021-09-21 - 11:29:17                                                      *//
//* Description:   MQTT-Funktionen                                                            *//
//*                2019-06-05; angepasst aus siehe unten                                      *//
//*                                                                                           *//
//*********************************************************************************************//
/*
 * Copyright (c) 2014 Scott Vokes <vokes.s@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define __MQTT_DEBUG__ false

#include <time.h>
#include <mysql.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <mosquitto.h>
#include <sys/types.h>
#include <assert.h>

#include "./mqtt.h"
#include "../error.h"
#include "../common.h"
#include "../datetime.h"
//#include "../checkproc.h"


#define BROKER_HOSTNAME   "Broker"
#define BROKER_PORT       1883

#define PLBUF        600    /* maximale Payload-Länge    */
#define PLLINE       120    /* maximale Payload-Zeilen   */
#define MAXSENSOR     55    /* maximale Anzahl Sensoren  */


/* How many seconds the broker should wait between sending out
 * keep-alive messages. */
#define KEEPALIVE_SECONDS         60

// Test-Message(Empfang):
//  mosquitto_sub -h Broker -v -t Solar/#

static struct mosquitto_message newMessage={'\0'};        // die empfangene Message
static struct mosquitto_message prevMessage={'\0'};       // die Message davor
static int ErrCount=0;                                    // Fehlerzähler
static time_t lastMsg=0;                                  // Zeitpunkt der letzten Message

//static struct MqttInfo thisClient;                     		// dieser Client

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

#if __MQTT_DEBUG__
 #define DEBUG(...)  printf(__VA_ARGS__)
#else
 #define DEBUG(...)
 #define DEBUGneu(...)
#endif

//***************************************************************************//
//
//#define  NORMAL   50
//#define 	TEXT   	51
//
//// Topic und Payload dürfen keine ungeschütztes ' ' enthalten
//// -------------------------------------------------------------------
//bool stringtest(char* istr)
//{
//	do
//	{
//		int mode = NORMAL;
//    switch (mode)
//    {
//      case NORMAL:    
//    		if      (*istr == '\"') 
//    			mode = TEXT;   
//    		else if (*istr == ' ') 
//    			exit false;
//        break;
//    
//      case TEXT:  
//    		if      (*istr == '\"') 
//    			mode = NORMAL;   
//        break;   
//    } // --- end switch ---
//   	istr++;
//	}
//	while(*istr >= '\0');
//}
//***************************************************************************//

char* MsgClassi[] =
{	"",
	"allmeine Information",
	"aktueller Status",
	"Messwert",
	"Verwaltung",
	"Schaltbefehl",
	"Fehlermeldung",
	NULL
};

/*********************************************************************************************************************/

// lokale Fehlermeldung
// ------------------------
// fügt Informationen ein und ruft Standard-Fehlermeldung auf
void mqtt_Error(char* Message, const char* Func, int Zeile)
{
  char ErrText[ERRBUFLEN];
  sprintf( ErrText, "%s - %s() in %s#%d \"%s\"",
                                         __NOW__, Func,__FILE__, Zeile, Message);
//  finish_with_Error(ErrText, NULL);
}
/*********************************************************************************************************************/
 
// Debug: Fehlertexte anzeigen  
// ---------------------------
void showErrors()
{                             
#ifdef _DEBUG
  fprintf(stdout, " Error  MOSQ_ERR_SUCCESS        : %d - %s\n", MOSQ_ERR_SUCCESS        ,  mosquitto_strerror(  MOSQ_ERR_SUCCESS       ));
  fprintf(stdout, " Error  MOSQ_ERR_INVAL          : %d - %s\n", MOSQ_ERR_INVAL          ,  mosquitto_strerror(  MOSQ_ERR_INVAL         ));
  fprintf(stdout, " Error  MOSQ_ERR_NOMEM          : %d - %s\n", MOSQ_ERR_NOMEM          ,  mosquitto_strerror(  MOSQ_ERR_NOMEM         ));
  fprintf(stdout, " Error  MOSQ_ERR_PAYLOAD_SIZE   : %d - %s\n", MOSQ_ERR_PAYLOAD_SIZE   ,  mosquitto_strerror(  MOSQ_ERR_PAYLOAD_SIZE  ));
  fprintf(stdout, " Error  MOSQ_ERR_MALFORMED_UTF8 : %d - %s\n", MOSQ_ERR_MALFORMED_UTF8 ,  mosquitto_strerror(  MOSQ_ERR_MALFORMED_UTF8));
  fprintf(stdout, " Error  MOSQ_ERR_SUCCESS        : %d - %s\n", MOSQ_ERR_SUCCESS        ,  mosquitto_strerror(  MOSQ_ERR_SUCCESS       ));
  fprintf(stdout, " Error  MOSQ_ERR_NOT_SUPPORTED  : %d - %s\n", MOSQ_ERR_NOT_SUPPORTED  ,  mosquitto_strerror(  MOSQ_ERR_NOT_SUPPORTED ));
  fprintf(stdout, " Error  MOSQ_ERR_PROTOCOL       : %d - %s\n", MOSQ_ERR_PROTOCOL       ,  mosquitto_strerror(  MOSQ_ERR_PROTOCOL      ));
  fprintf(stdout, " Error  MOSQ_ERR_ERRNO          : %d - %s\n", MOSQ_ERR_ERRNO          ,  mosquitto_strerror(  MOSQ_ERR_ERRNO         ));
#endif
}                                                   
/***************************************************cccc**************************************************************/

char* MQTT_Version()
{
  int major, minor, revision;
  static char Version[20];
  mosquitto_lib_version(&major, &minor, &revision );
  sprintf(Version,"%i.%02i.%02i",major, minor, revision);
  
  return Version;
}
/***************************************************cccc**************************************************************/

// wie lange ist die letzte Nachricht her?
// ---------------------------------------
// in [sec]
const time_t lastMessage(void)
{
  return time(NULL)-lastMsg;
}
/***************************************************cccc**************************************************************/

/*** Callback 'on_connect': Callback for successful connection: add subscriptions. ***/
/*** ----------------------------------------------------------------------------- ***/
/***                                  on_connect                                   ***/

static void on_connect(struct mosquitto* mqtt, void* udata, int res)
{
  if (res == 0)   // dies ist der 'connect'-Status
  { /* ---------- success --------- */
    DEBUG("   - %s-%s:%s()#%d: -- on_connect() OK\n",
                            __NOW__,__FILE__,__FUNCTION__,__LINE__);

    // hier die Subscriptions eintragen
    // --------------------------------
  	char Subscript[ZEILE];
  	sprintf( Subscript, SUBSCR_TYP, MSG_ERROR);			// Fehlermeldungen
    int res = mosquitto_subscribe(mqtt, NULL, Subscript, 0);
    if (res == 0)
    	DEBUG("   - %s-%s:%s()#%d: -- mosquitto_subscribe('%s') OK\n",
                            __NOW__,__FILE__,__FUNCTION__,__LINE__, Subscript);
    else
    { // Subscribe misslungen
      // ----------------------
      char ErrText[ERRBUFLEN];
      int errnum = res;
      if (res == MOSQ_ERR_ERRNO)
        errnum = errno;
      sprintf( ErrText, "mosquitto_subscribe('%s') - Error %d - '%s'\n",
                                  Subscript, res, mosquitto_strerror( errnum ));
      mqtt_Error(ErrText,__FUNCTION__,__LINE__);
    }
  }
  else
  { 
  	// Connect misslungen
    // ------------------
    char ErrText[ERRBUFLEN];
    int errnum = res;
    if (res == MOSQ_ERR_ERRNO)
      errnum = errno;
    sprintf( ErrText, "on_connect() - Error %d - '%s'\n", res, mosquitto_strerror( errnum ));
    mqtt_Error(ErrText,__FUNCTION__,__LINE__);
  }
  
  DEBUG("   - %s-%s:%s()#%d: -- on_connect done!\n", __NOW__,__FILE__,__FUNCTION__,__LINE__);
}
// LED_AUS <https://mosquitto.org/api/files/mosquitto-h.html>
// -----------------------------------------------------
//  Parameters
//    mosq  a valid mosquitto instance.
//    mid   a pointer to an int.  If not NULL, the function will set this to the message id of this particular message.
//          This can be then used with the subscribe callback to determine when the message has been sent.
//    sub   the subscription pattern.
//    qos   the requested Quality of Service for this subscription.
// Returns
//  MOSQ_ERR_SUCCESS  on success.
//  MOSQ_ERR_INVAL            if the input parameters were invalid.
//  MOSQ_ERR_NOMEM            if an out of memory condition occurred.
//  MOSQ_ERR_NO_CONN          if the client isn’t connected to a broker.
//  MOSQ_ERR_MALFORMED_UTF8   if the topic is not valid UTF-8
//  MOSQ_ERR_ERRNO            if a system call returned an error.
//                            The variable errno contains the error code, even on Windows.
//                            Use strerror_r() where available or FormatMessage() on Windows.
//
/*********************************************************************************************************************/

/*** Callback 'on_publish': A message was successfully published. ***/
/*** ------------------------------------------------------------ ***/
/***                          on_publish                          ***/

static void on_publish(struct mosquitto *m, void *udata, int m_id)
{
  DEBUG("   - %s-%s:%s()#%d: -- published successfully\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);
}
/*********************************************************************************************************************/

/*** Callback 'on_message': Handle a message that just arrived via one of the subscriptions. ***/
/*** --------------------------------------------------------------------------------------- ***/
/***                                on_message                                               ***/

// die erhaltene Nachricht wird in 'payload' abgelegt
// --------------------------------------------------
static void on_message(struct mosquitto* Client, void* udata, const struct mosquitto_message* msg)
{
  // Prüfung, ob Inhalt
  // ------------------
  if ((msg == NULL) || (msg->payloadlen <= 0))
  {
    return;
  }
  // Prüfung auf Buffer-Überlauf
  // ----------------------------
  if (msg->payloadlen >= ZEILE)
  {
    char ErrText[ERRBUFLEN];
    sprintf( ErrText, "on_message() - payloadlen=%d  ** ZU GROSS **\n", msg->payloadlen);
    mqtt_Error(ErrText,__FUNCTION__,__LINE__);
  }
  // Prüfung, ob erste Message nach Start
  // ------------------------------------
  if (prevMessage.payloadlen == 0)
  {
    prevMessage.payload = "noch nix drin";
  }
  DEBUG("%s-%s:%s()#%d ----- got message: ('%s' - %d, QoS: %d, %s - '%s')\n",__NOW__,__FILE__,__FUNCTION__,__LINE__,
                      msg->topic, msg->payloadlen, msg->qos, msg->retain ? "R" : "!r", (char*)msg->payload);

//  if (strcmp((char*)msg->payload, prevMessage.payload) == 0)      // mit der vorigen Message vergleichen
//  { // diese Message ist schon verarbeitet
//    // -----------------------------------
//    newMessage.topic = NULL;                                      // als ungültig markieren
//    DEBUG("%s-%s:%s()#%d ------ old message -----\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);
//  }
//  else
  { // neue Message: noch nicht bearbeitet
    // ---------------------------------------
    mosquitto_message_copy(&newMessage, msg);                     // Kopie anfertigen
    mosquitto_message_copy(&prevMessage, &newMessage);
    DEBUG("%s-%s:%s()#%d ****** new message ******\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);
  }
}   // ----> newMessage
/*********************************************************************************************************************/

/*** Callback: Successful subscription hook. ***/
/*** -----------------------------------------  ***/
static void on_subscribe(struct mosquitto* m, void* udata, int mid,
                         int qos_count, const int* granted_qos)
{
  DEBUG("   - %s-%s:%s()#%d: -- ID %d subscribed successfully\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, mid);
}
/*********************************************************************************************************************/

/** Initialize a mosquitto client. **/
/** =============================  **/
static struct mosquitto *init(struct MqttInfo *info)
{
  void *udata = (void *)info;
  size_t buf_sz = ZEILE;
  char buf[buf_sz];
  if (buf_sz < snprintf(buf, buf_sz, "client_%d", info->pid))
  {
    return NULL;            /* Buffer zu klein */
  }
  char Msg[ZEILE];                             // Text-Buffer
  sprintf(Msg, "new Client ID: >%d<", info->pid);
  DEBUG("   - %s-%s:%s()#%d:%s\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, Msg);

  // Create a new mosquitto client, with the name "client_#{PID}"
  // ------------------------------------------------------------
  struct mosquitto *mqtt = mosquitto_new(buf, true, udata);

  DEBUG("   - %s-%s:%s()#%d: -- init OK\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);
  return mqtt;
}
/*********************************************************************************************************************/

/** Register the callbacks that the mosquitto connection will use. **/
/** =============================================================  **/
static bool set_callbacks(struct mosquitto *mqtt)
{
  mosquitto_connect_callback_set(mqtt, on_connect);
  mosquitto_publish_callback_set(mqtt, on_publish);
  mosquitto_subscribe_callback_set(mqtt, on_subscribe);
  mosquitto_message_callback_set(mqtt, on_message);

  DEBUG("   - %s-%s:%s()#%d: -- set_callbacks() OK\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);
  return true;
}
/*********************************************************************************************************************/

/** Connect to an MQTT broker **/
/** =========================  **/
static bool connect(struct mosquitto *mqtt)
{
//    Parameters:
//    mosq        a valid mosquitto instance.
//    host        the hostname or ip address of the broker to connect to.
//    port        the network port to connect to.  Usually 1883.
//    keepalive   the number of seconds after which the broker should send a PING message
//                to the client if no other messages have been exchanged in that time.
  bool done = false;
  int ErrCount = 0;
  int res =  ~MOSQ_ERR_SUCCESS;
  do
  {
    res = mosquitto_connect(mqtt, BROKER_HOSTNAME, BROKER_PORT, KEEPALIVE_SECONDS);
    if ((res == MOSQ_ERR_SUCCESS) || (ErrCount++ >= MAXERRCOUNT))
      done = true;
  } while (!done);
  DEBUG("   - %s-%s:%s()#%d: -- connect() res=%d / %d Errors\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, res, ErrCount);
  if (res != MOSQ_ERR_SUCCESS)
  { // permanenter Fehler
    // -------------------
    char ErrText[ERRBUFLEN];
    if (res == MOSQ_ERR_ERRNO)
    { // System-Fehler
      syslog(LOG_NOTICE, "Error %d # %d * (*>%s<*)", errno, ErrCount, strerror( errno ));
      sprintf( ErrText, "Error %d - '%s' - %i Errors\n", errno, strerror( errno ), ErrCount);
    }
    else
    { // MQTT-Fehler
      syslog(LOG_NOTICE, "Error %d # %d * (->%s<-)", res, ErrCount, mosquitto_strerror( res ));
      sprintf( ErrText, "Error %d - '%s' - %i Errors\n", res, mosquitto_strerror( res ), ErrCount);
    }
    mqtt_Error(ErrText ,__FUNCTION__,__LINE__); // Feierabend
  }
  return (res == MOSQ_ERR_SUCCESS);
}
// Returns
//  MOSQ_ERR_         SUCCESS on success.
//  MOSQ_ERR_INVAL    if the input parameters were invalid.
//  MOSQ_ERR_ERRNO    if a system call returned an error.
//                    The variable errno contains the error code, even on Windows.
//                    Use strerror_r() where available or FormatMessage() on Windows.
//
/*********************************************************************************************************************/

// Publish -- Nachricht absetzen
// =============================
int MQTT_Publish(struct MqttInfo* info, char* topic, char* payload)
{
	int res=0;
  static int ErrCount=0;                                            // Fehlerzähler
  char Msg[ZEILE];                                                 // Text-Buffer
  sprintf(Msg, "topic: \"%s\" published: \"%s\"", topic, payload);
  DEBUG("   - %s-%s:%s()#%d: -- Message:>%s< --> %d\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, Msg, res);

//	// Topic und Payload dürfen kein ' ' enthalten
//	bool stringtest(char* istr)

	// Nachricht senden
	// ------------------
  res = mosquitto_publish(info->m, NULL, topic, strlen(payload), payload, 0, false);

  if ((res == MOSQ_ERR_NO_CONN) | (res == MOSQ_ERR_CONN_LOST))      // Verbindung verloren
  {
    ErrCount++;
    DEBUG("   - %s-%s:%s()#%d: -- Error %d # %d\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, res, ErrCount);
    if (ErrCount < MAXERRCOUNT)
      res = MOSQ_ERR_SUCCESS;
  }

  if (res != MOSQ_ERR_SUCCESS)                                      // da ist etwas schiefgegangen
  {
    char ErrText[ERRBUFLEN];
    sprintf( ErrText, "Error '%d' - '%s' -- >%s<\n", res, mosquitto_strerror( res ), Msg);
    mqtt_Error(ErrText ,__FUNCTION__,__LINE__);                     // Feierabend
  }
  else
    ErrCount = 0;

  return (res == MOSQ_ERR_SUCCESS);
}

// LED_AUS <https://mosquitto.org/api/files/mosquitto-h.html>
// -----------------------------------------------------
// Publish a message on a given topic.
//
// libmosq_EXPORT int mosquitto_publish(
// struct mosquitto*  mosq,               - a valid mosquitto instance
//              int*  mid,                - If not NULL, the function will set this to the message id of this particular message
//       const char*  topic,              - null terminated string of the topic
//               int  payloadlen,         - the size of the payload (bytes)
//       const void*  payload,            - pointer to the data to send
//               int  qos,                - 0, 1 or 2 indicating the Quality of Service
//              bool  retain)             - set to true to make the message retained.
//
//    Returns
//    MOSQ_ERR_SUCCESS    on success.
//    MOSQ_ERR_INVAL          if the input parameters were invalid.
//    MOSQ_ERR_NOMEM          if an out of memory condition occurred.
//    MOSQ_ERR_NO_CONN        if the client isn’t connected to a broker.
//    MOSQ_ERR_PROTOCOL       if there is a protocol error communicating with the broker.
//    MOSQ_ERR_PAYLOAD_SIZE   if payloadlen is too large.
//    MOSQ_ERR_MALFORMED_UTF8 if the topic is not valid UTF-8
//
/*********************************************************************************************************************/

// MQTT wird initiert und die Callbacks aktiviert
// -----------------------------------------------
struct MqttInfo* MQTT_Start()
{
	// **** das zentrale MQTT-Objekt *****
	// ===================================
	static struct MqttInfo thisClient;                     		 

  pid_t pid = getpid();

  memset(&thisClient, 0, sizeof(thisClient));							// dieses Objekt leeren ...
  thisClient.pid = pid;

  struct mosquitto* mqtt = init(&thisClient);             // ... und  initialisieren
  if (mqtt == NULL)
    mqtt_Error("mqtt-init() failure\n",__FUNCTION__,__LINE__);
  thisClient.m = mqtt;

  if (!set_callbacks(mqtt))                               // Callbacks für das Objekt
    mqtt_Error("mqtt-set_callbacks() failure\n",__FUNCTION__,__LINE__);

  if (!connect(mqtt))                                     // Objekt starten
    mqtt_Error("mqtt-connect() failure\n",__FUNCTION__,__LINE__);

  // Debug: Fehlertexte anzeigen 
  // -------------------------- 
  showErrors();

  return &thisClient;
}
/*********************************************************************************************************************/

// MQTT: Subscription eintragen
// -----------------------------------------------
int MQTT_Subscribe(struct MqttInfo* mqtt, char* thisSubscript)
{
  DEBUG("   - %s-%s:%s()#%d: -- %s(%p,'%s')\n",
                          __NOW__,__FILE__,__FUNCTION__,__LINE__, __FUNCTION__, mqtt, thisSubscript);

  // hier die Subscriptions eintragen
  // --------------------------------
  int res = mosquitto_subscribe(mqtt->m, NULL, thisSubscript, 0);
  if (res == 0)
  { /* success */
  	DEBUG("   - %s-%s:%s()#%d: -- mosquitto_subscribe('%s') OK\n",
                          __NOW__,__FILE__,__FUNCTION__,__LINE__, thisSubscript);
    { // --- Log-Ausgabe ---------------------------------------------------------
      char LogText[ZEILE];  sprintf(LogText, "- MQTT Subscription: '%s'", thisSubscript);
      MYLOG(LogText);
    } // ------------------------------------------------------------------------
  }
  else
  { // Subscribe misslungen
    // ----------------------
    char ErrText[ERRBUFLEN];
    int errnum = res;
    if (res == MOSQ_ERR_ERRNO)
      errnum = errno;
    sprintf( ErrText, "mosquitto_subscribe('%s') - Error %d - '%s'\n",
                                thisSubscript, res, mosquitto_strerror( errnum ));
    mqtt_Error(ErrText,__FUNCTION__,__LINE__);
  }
 
  return res;
}
/*********************************************************************************************************************/
//#define _DEBUG 1

/****************************************************************************************************/

// Fehlerausgabe
// -------------
void showError(int res, const char* Function, const int Line)
{
  // immer in die Log-Datei schreiben
  // ----------------------------------
  if (res == 14)                                      // MOSQ_ERR_ERRNO --> Linux-Error
    syslog(LOG_NOTICE, "%s:%s()#%d: -- %d.) Linux-Error#%d (%s)",
                        __FILE__, Function, Line, ErrCount, errno, strerror( errno ));
  else                                                // MQTT-Fehler
    syslog(LOG_NOTICE, "%s:%s()#%d: -- %d.) MQTT-Error#%d (%s)",
                        __FILE__, Function, Line, ErrCount, res, mosquitto_strerror( res ));
  // Debug-Ausgabe nur auf Wunsch
  // ----------------------------
  if (res == 14)                                      // MOSQ_ERR_ERRNO --> Linux-Error
    DEBUG("   - %s-%s:%s()#%d: -- %d.) Linux-Error#%d (%s)\n",
                    __NOW__,__FILE__, Function, Line, ErrCount, errno, strerror( errno ));
  else                                                // MQTT-Fehler
    DEBUG("   - %s-%s:%s()#%d: -- %d.) MQTT-Error#%d (%s)\n",
                    __NOW__,__FILE__, Function, Line, ErrCount, res, mosquitto_strerror( res ));
}
/****************************************************************************************************/

// Loop - Schleifenaufruf, um Callbacks zu erfassen
// ================================================
// muss regelmäßig bedient werden
// gibt letzten Message-Status ('neue Message')zurück

enum TOPICMODE MQTT_Loop(struct MqttInfo* info, char* thisTopic, char* thisPayload)
{
  DEBUG("\n   - %s-%s:%s()#%d --beginn--\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);

  // die Abfrage als solche !!
  // -------------------------
  int Status = MODE_NOTHING;
  assert( info != NULL );  
  int res = mosquitto_loop(info->m, NO_TIMEOUT, UNBENUTZT);      //  <<<<<<<<<<<<<<<<<<<<<<<<<<
  
  // dieser Aufruf löst die Bedienung der in 'set_callbacks(...)' gesetzten Callback-Routinen aus,
  // so auch 'on_message(...)'. Eine dort erfasste Message ist in 'newMessage' abgelegt und hier zugänglich.
  //    on_message(...)--> newMessage
  
  DEBUG("   - %s-%s:%s()#%d --- callback done ---\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);

  // aber zunächst mal eine Fehlerprüfung:
  // -------------------------------------------------------------------------------------------------
  {
    if (res == MOSQ_ERR_SUCCESS)  
    { // alles gut
      // ---------
      ErrCount = 0;                                       // Fehler-Zähler löschen
    }     // ====> alles klar!
    
    else
    { // es ist ein Fehler aufgetreten !
      // ------------------------------
      newMessage.topic[0] = '\0';                            // kein gutes Ergebnis
      showError(res, __FUNCTION__, __LINE__);
      if (ErrCount++ < MAXERRCOUNT)   
      // vielleicht repariert sich der Fehler selbst
      // -------------------------------------------              
        res = MOSQ_ERR_SUCCESS;                           // zunächst abwarten   
      else
      { // nein, mehrfacher Fehler: Restart-Versuch
        // ----------------------------------------
        syslog(LOG_NOTICE, "%s:%s()#%d: -- mosquitto_reconnect(%ld)",
                                __FILE__,__FUNCTION__,__LINE__, (long)info->m);
        res = mosquitto_reconnect(info->m);               // Restart-Versuch
        if (res == MOSQ_ERR_SUCCESS)
        { // es hat geklappt
          // -------------
          syslog(LOG_NOTICE, "%s:%s()#%d: -- mosquitto_reconnect(%ld) done",
                                __FILE__,__FUNCTION__,__LINE__, (long)info->m);
        }
        else
        { // dauerhafter Fehler: jetzt ist endgültig Feierabend !
          // -----------------------------------------------------
          char ErrText[ERRBUFLEN];
          char Msg[] = "mosquitto_reconnect() failed";
          ErrCount = 999;
          showError(res, __FUNCTION__, __LINE__);
          if (res == 14)                                      // MOSQ_ERR_ERRNO --> Linux-Error
            sprintf( ErrText, "Linux-Error %d: '%s' -- >%s<\n", errno, strerror( errno ), Msg);
          else                                                // MQTT-Fehler
            sprintf( ErrText, "MQTT-Error %d: '%s' -- >%s<\n", res, mosquitto_strerror( res ), Msg);
          mqtt_Error(ErrText ,__FUNCTION__,__LINE__);         // Feierabend
        }  
      }
    }
  }   // --- Ende Fehlerbehandlung -------------------------------------------------------------------

  DEBUG("   - %s-%s:%s()#%d --- errorhandling done ---\n",__NOW__,__FILE__,__FUNCTION__,__LINE__);
  DEBUG("   - %s-%s:%s()#%d --- newMessage.topic: '%s' ---\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, newMessage.topic);

  // ... hier geht's weiter:
  // -----------------------
  if (newMessage.topic != NULL)
  { // die neue Message ist in 'newMessage' abgelegt
    // ---------------------------------------------
    // in die Rückgabewerte umkopieren
    strcpy(thisTopic, newMessage.topic);
    strncpy(thisPayload, (char*)newMessage.payload, newMessage.payloadlen);
    *(thisPayload+newMessage.payloadlen) = '\0';
    DEBUG("   - %s-%s:%s()#%d: --- neu: Topic='%s', Payload='%s'\n",
                     __NOW__,__FILE__,__FUNCTION__,__LINE__, thisTopic, thisPayload);
		
		// nach Messagetyp vorsortieren
		// ----------------------------      -------   -----
    if      (matchn(newMessage.topic, TPOS_TYP,  MSG_INFO)) 
    { // allmeine Information
    	Status = MODE_INFO;
    }
    
    else if (matchn(newMessage.topic, TPOS_TYP, MSG_COMMAND))    
    { // Schaltbefehl
    	Status = MODE_COMMAND;
    }
      
    else if (matchn(newMessage.topic, TPOS_TYP, MSG_STATE))  
    { // aktueller Status
    	Status = MODE_STATE;
    }
      
    else if (matchn(newMessage.topic, TPOS_TYP, MSG_VALUE)) 
    { // Messwert
    	Status = MODE_VALUE;
    }
      
    else if (matchn(newMessage.topic, TPOS_TYP, MSG_ADMIN))  
    { // Verwaltung
    	Status = MODE_ADMIN;
    }
      
    else if (matchn(newMessage.topic, TPOS_TYP, MSG_ERROR))    
    { // Fehlermeldung
    	Status = MODE_ERROR;
    }
    
    newMessage.topic = NULL;          // als gelesen markieren
    lastMsg = time(NULL);             // Zeitpunkt der letzten guten Message
  }

  DEBUG("   - %s-%s:%s()#%d --- ende --- Status=%d\n",__NOW__,__FILE__,__FUNCTION__,__LINE__, Status);

  return Status;
}

// LED_AUS <https://mosquitto.org/api/files/mosquitto-h.html>
// -----------------------------------------------------
//  Parameters
//  mosq          a valid mosquitto instance.
//  timeout       Maximum number of milliseconds to wait for network activity in the select() call before timing out.
//                Set to 0 for instant return.  Set negative to use the default of 1000ms.
//  max_packets   this parameter is currently unused and should be set to 1 for future compatibility.
//
//    Returns
//    MOSQ_ERR_SUCCESS  on success.
//    MOSQ_ERR_INVAL      if the input parameters were invalid.
//    MOSQ_ERR_NOMEM      if an out of memory condition occurred.
//    MOSQ_ERR_NO_CONN    if the client isn’t connected to a broker.
//    MOSQ_ERR_CONN_LOST  if the connection to the broker was lost.
//    MOSQ_ERR_PROTOCOL   if there is a protocol error communicating with the broker.
//    MOSQ_ERR_ERRNO      if a system call returned an error.
//
//#undef _DEBUG

/*********************************************************************************************************************/
//#ifndef _MQTTTYPEN
//#define _MQTTTYPEN  1

char* MessageTyp[] =
{	"",
	"allmeine Information",
	"aktueller Status",
	"Messwert",
	"Verwaltung",
	"Schaltbefehl",
	"Fehlermeldung",
	NULL
};
//#endif
/*********************************************************************************************************************/

//// In Topic ein Glied mit Text ersetzen, wenn vorhanden
//// -----------------------------------------------------
//bool irgendwas(char* OrgTopic, char* NewTopc
//{
//    else if (matchn(newMessage.topic, MSG_COMMAND, TPOS_TYP))    
//}
//// Vergleich, ob die n.ten Glieder der Strings übereinstimmen
//// -----------------------------------------------------------
//// Stringformat: 'xxx/yyy/zzz'
//bool match(const char *topic, const char *key, int n)

/*********************************************************************************************************************/
