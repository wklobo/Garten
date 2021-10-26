##*********************************************************************************************//
##*                                                                                           *//
##* File:          Makefile                                                                   *//
##* Author:        Wolfgang Keuch                                                             *//
##* Creation date: 2020-12-27;                                                                *//
##* Last change:   2021-09-10 - 13:49:15: neuer Ansatz                                        *//
##* Description:   Makefile für 'SqlMotion'                                                   *//
##*                                                                                           *//
##*********************************************************************************************//

DIR_MAIL = ../../sendmail
DIR_FONTS = ../Fonts
DIR_BIN = ./obj
DIR_SRC = ..

OBJ_C = $(wildcard ${DIR_FONTS}/*.c ${DIR_SRC}/*.c ${DIR_MAIL}/*.c ./*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

MAINFILE = sqlmotion
TARGET  = SqlMotion
VERSION = ./version.h

CC = gcc

INCLUDE	= -I/usr/include -I/usr/local/include -I/usr/include/mysql -I../../sendmail
#DEBUG = -g -O0  -Wall -Wno-format-overflow -Wno-unused-function  -Wno-deprecated-function  '-D PROGNAME="${TARGET}"'
DEBUG = -g -O0  -Wall  -Wno-format-overflow  '-D PROGNAME="${TARGET}"'
CFLAGS += $(DEBUG) -lmysqlclient  -lmosquitto
LIBS = -lwiringPi -lmysqlclient  -lmosquitto

## neue Versionsnummer
## --------------------
#&(VERSION) : ${TARGET}
#	./version.py

# die OBJs zusammenbinden:
# ------------------------
${TARGET} : ${OBJ_O} $(MAINFILE).o
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIBS) $(INCLUDE)
	./version.py

# Mail:
# -----
${DIR_BIN}/%.o : $(DIR_MAIL)/%.c
	$(CC) $(CFLAGS) -c  $< -o $@

# meine Dateien:
# --------------
${DIR_BIN}/$(MAINFILE).o : $(MAINFILE).c   $(MAINFILE).h  Makefile
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)

${DIR_BIN}/%.o : ${DIR_SRC}/%.c  ${DIR_SRC}/%.h
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


# saubermachen:
# -------------
clean :
	$(RM) $(DIR_BIN)/*.* $(TARGET) $(DIR_OBJ)/.*.sw?
	/home/pi/Sicherung.sh

##*********************************************************************************************//
