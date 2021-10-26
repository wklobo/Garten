//*********************************************************************************************//
//*                                                                                           *//
//* File:          mqtt.h                                                                     *//
//* Author:        siehe mqtt.c                                                               *//
//* Creation date: 2019-05-26;                                                                *//
//* Last change:   2021-09-20 - 08:35:44                                                      *//
//* Description:   MQTT-Funktionen                                                            *//
//*                                                                                           *//
//*                                                                                           *//
//*********************************************************************************************//

#ifndef _MQTT_H
#define _MQTT_H  1

// der Mosquitto-Objekt-Typ
// ========================
struct MqttInfo
{
  struct mosquitto* m;
  pid_t             pid;
  uint32_t          tick_ct;
};

// MQTT-Trennzeichen
// ------------------
#ifndef DELIM
#define DELIM   '/'
#endif

// TOPIC
// =================================================================

// Topic-Format
// ---------------
// das Topic-Format ist immer gleich
//  0.Raspi-Name (erforderlich)
//  1.IP-letzte Stelle (erforderlich)
//  2.Sensor-ID (erforderlich)
//  3.Sensor-Name (oder "-")
//  4.Datum/Uhrzeit - als timestamp
//  5.Messagetyp (Art der Message)
//  6.Messageart (Textteil Message)
#define MQTT_TOPIC "/%s/%d/%d/%s/%li/%d/%d"

// Abfrage-Masken
// ---------------
#define SUBSCR_RAS "/%s/#"                // Maske für Raspi-Name
#define SUBSCR_IPA "/+/%d/#"              // Maske für IP (letzte Stelle)
#define SUBSCR_SID "/+/+/%d/#"            // Maske für Sensor-ID
#define SUBSCR_NAM "/+/+/+/%s/#"          // Maske für Sensor-Name
#define SUBSCR_TIM "/+/+/+/+/%li/#"       // Maske für Datum/Uhrzeit
#define SUBSCR_TYP "/+/+/+/+/+/%d/#"      // Maske für Messagetyp
#define SUBSCR_INF "/+/+/+/+/+/+/+/%d"    // Maske für Messageart

// Abfrage-Positionen
// -------------------
#define TPOS_RAS  0
#define TPOS_IPA  1
#define TPOS_SID  2
#define TPOS_NAM  3
#define TPOS_TIM  4
#define TPOS_TYP  5
#define TPOS_INF  6

// Messagetypen   (Position 5)
// --------------------------
enum MSGTYPE { MSG_LEER=-10000,
               MSG_INFO,           // allmeine Information
               MSG_STATE,          // aktueller Status
               MSG_VALUE,          // Messwert
               MSG_ADMIN,          // Verwaltung
               MSG_COMMAND,        // Schaltbefehl
               MSG_ERROR};         // Fehlermeldung

//#ifndef _MQTTTYPEN
//#define _MQTTTYPEN  1
//
//char* MassageTyp[] =
//{	"",
//	"allmeine Information",
//	"aktueller Status",
//	"Messwert",
//	"Verwaltung",
//	"Schaltbefehl",
//	"Fehlermeldung",
//	NULL
//};
//#endif

// Messageinformation  (Position 6)
// ---------------------------------
enum MSGINFO { INF_LEER=-8000,
               INF_REQUEST,         // Anfrage
               INF_RESPONSE,        // Antwort auf Anfrage
               INF_ACK,             // Quittung auf Anfrage
               INF_NAK,             // negative Quittung auf Anfrage
               INF_REGULAR,         // normale Message
               INF_REQFACK,         // Bitte um Quittung
               INF_NOSUPPORT,       // wird nicht unterstützt
               INF_FEHLERHAFT,      // war fehlerhaft
               INF_WEISSNICHT };    //

//char* msgInfoi[] =																			
//{	"",
//	"Anfrage",                                         
//	"Antwort auf Anfrage",                             
//	"Quittung auf Anfrage",                            
//	"negative Quittung auf Anfrage",                   
//	"normale Message",                                 
//	"Bitte um Quittung",                               
//	"wird nicht unterstützt",                          
//	"war fehlerhaft",                                  
//	NULL																	
//};

// PAYLOAD
// =================================================================
// Payload-Werte müssen immer mindestens ein Zeichen enthalten !

// Message-Payload Typ VALUE
// ==================================
// Payload-Format abhängig vom Topic
// 0.Messwert
// 1.Maßeinheit
// 2.Status bzw. Bemerkung
#define MQTT_VALUE "/%9.3f/%s/%s"

// Message-Payload Topic[5] = MSG_COMMAND
// ========================================
// Payload-Format abhängig vom Topic
// 0.Steuerbefehl
// 1.Schaltwert ('LED_EIN','LED_AUS', oder '-')
// 2.Analogwert (oder '-')
// 3.Anzeigewert (oder "-")
#define MQTT_COMMAND "/%d/%s/%s/%s"

// Steuerbefehl-Werte
// ------------------
#define CMD_SCHALTEN 10
#define CMD_MESSEN   11
#define CMD_ABFRAGE  12
#define CMD_ANZEIGE  13

// Abfrage-Positionen
// ------------------
#define PPOS_CMD  0
#define PPOS_VAL  1
#define PPOS_ANA  2
#define PPOS_DIS  3


// Message-Payload Typ MSG_STATE oder MSG_ERROR
// =========================================
// Payload-Format abhängig vom Topic
// 0.Meldung (START, ERROR o.ä.)
#define MQTT_STATE "/%s"

#define PPOS_STT  0


// STATUS
// =================================================================

// Status-Rückgaben von 'loop'
// -----------------------------
enum TOPICMODE { MODE_NOTHING,
                 MODE_LEER=100,
                 MODE_INFO,
                 MODE_STATE,
                 MODE_VALUE,
                 MODE_ADMIN,
                 MODE_ERROR,
                 MODE_START,
                 MODE_FINISH,
                 MODE_COMMAND,
                 MODE_TIMEOUT,
                 MODE_LASTWILL};


// FUNKTIONEN
// =================================================================

// MQTT wird initiert und die Callbacks aktiviert
// ----------------------------------------------
struct MqttInfo* MQTT_Start();

// Subscription eintragen
// -----------------------------------------------
int MQTT_Subscribe(struct MqttInfo* mqtt, char* thisSubscript);

// Schleifenaufruf, um Callbacks zu erfassen
// ------------------------------------------
enum TOPICMODE MQTT_Loop(struct MqttInfo* info, char* thisTopic, char* thisPayload);

// Nachricht absetzen
// ------------------
int MQTT_Publish(struct MqttInfo* info, char* topic, char* payload);

// Version der Lib
// ------------------
char* MQTT_Version();

// wie lange ist die letzte Nachricht her?
// ----------------------------------------
const time_t lastMessage(void);

//*********************************************************************************************//
// MQTT-Fehlercodes
// ----------------
// Error  MOSQ_ERR_SUCCESS        :  0 - No error.
// Error  MOSQ_ERR_NOMEM          :  1 - Out of memory.
// Error  MOSQ_ERR_PROTOCOL       :  2 - A network protocol error occurred when communicating with the broker.
// Error  MOSQ_ERR_INVAL          :  3 - Invalid function arguments provided.
// Error  MOSQ_ERR_PAYLOAD_SIZE   :  9 - Payload too large.
// Error  MOSQ_ERR_NOT_SUPPORTED  : 10 - This feature is not supported.
// Error  MOSQ_ERR_ERRNO          : 14 - Success
// Error  MOSQ_ERR_MALFORMED_UTF8 : 18 - Malformed UTF-8
//
//*********************************************************************************************//

#define UNBENUTZT           1000
#define NO_TIMEOUT             0
#define MAXERRCOUNT            8                /* maximale Fehlerzahl    */


#endif //_MQTT_H

//*********************************************************************************************//
