 //************ Kamera ******************************************************//
//*                                                                         *//
//* File:          sqlmotion.h                                              *//
//* Author:        Wolfgang Keuch                                           *//
//* Creation date: 2014-07-20  --  2016-02-18                               *//
//* Last change:   2021-11-07 - 11:03:578                                    *//
//* Description:   Weiterverarbeitung von 'motion'-Dateien:                 *//
//*                Event ermitteln, daraus ein Verzeichnis erstellen,       *//
//*                zugehörige Dateien in dieses Verzeichnis verschieben     *//
//*                                                                         *//
//* Copyright (C) 2014-21 by Wolfgang Keuch                                 *//
//*                                                                         *//
//***************************************************************************//

#ifndef _SQLMOTION_H
#define _SQLMOTION_H  1

#include "../common.h"

#define NO_ERROR 0

#define FOLDER_NEW     101
#define FOLDER_EXIST   102


// Datenbank neu aufgesetzt
// ------------------------
#define THISUSER            "root"
#define THISPW              "geheim"
#define THISHOST            "localhost"

#define EVENTNUMMER         "/home/pi/Garten/aux/EventNummer.save"

//--
//-- Tabellenstruktur für Tabelle `tabEvent`
//--
//CREATE TABLE `tabEvent` (
//  `evKeyID` int(11) NOT NULL COMMENT 'Primärindex',
//  `evSaved` timestamp NOT NULL DEFAULT current_timestamp() COMMENT 'Speicher-Zeitpunkt',
//  `evEvent` tinytext NOT NULL COMMENT 'Eventnummer',
//  `evDate` date NOT NULL DEFAULT '0000-00-00' COMMENT 'Ereignis-Datum',
//  `evTime` time NOT NULL DEFAULT '00:00:00' COMMENT 'Ereignis-Uhrzeit',
//  `evSize` int(11) NOT NULL COMMENT 'Speicherbelegung',
//  `evRemark` tinytext DEFAULT NULL COMMENT 'Bemerkung'
//) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


#define MYDATABASE          "DB_Motion"
#define MYEVENTTABLE        "tabEvent"

#define  EVKEYID     "evKeyID"
#define  EVSAVED     "evSaved"
#define  EVEVENT     "evEvent"
#define  EVDATE      "evDate"
#define  EVTIME      "evTime"
#define  EVSIZE      "evSize"
#define  EVREMARK    "evRemark"

#endif //_SQLMOTION_H

//***********************************************************************************************
