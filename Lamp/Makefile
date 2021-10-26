##*********************************************************************************************//
##*                                                                                           *//
##* File:          Makefile                                                                   *//
##* Author:        Wolfgang Keuch                                                             *//
##* Creation date: 2021-04-05;                                                                *//
##* Last change:   2021-09-04 - 12:19:45                                                      *//
##* Description:   Makefile für 'LampMotion'                                                  *//
##*                                                                                           *//
##*********************************************************************************************//

DIR_MAIL = ../../sendmail
DIR_BIN = ./obj
DIR_HOME = .
DIR_SRC = ..

OBJ_C = $(wildcard ${DIR_FONTS}/*.c ${DIR_SRC}/*.c ${DIR_MAIL}/*.c ./*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

MAINFILE = lampmotion
TARGET  = LampMotion
VERSION = ./version.h

CC = gcc

INCLUDE	= -I/usr/include -I/usr/local/include -I/usr/include/mysql -I../../sendmail
DEBUG = -g -O0  -Wall -Wno-format-overflow -Wno-unused-function  -Wno-unused-value -Wno-comment '-D PROGNAME="${TARGET}"'
CFLAGS += $(DEBUG) -lmysqlclient  -lmosquitto  -li2c
LIBS = -lwiringPi -lmysqlclient  -lmosquitto  -lm  -li2c

# die OBJs zusammenbinden:
# ------------------------
${TARGET} : ${OBJ_O} $(MAINFILE).o $(MAINFILE).h 
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIBS) $(INCLUDE)
	./version.py

# Mail:
# -----
${DIR_BIN}/%.o : $(DIR_MAIL)/%.c
	$(CC) $(CFLAGS) -c  $< -o $@

# meine Dateien:
# --------------
${DIR_BIN}/$(MAINFILE).o : $(MAINFILE).c $(MAINFILE).h Makefile
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)

${DIR_BIN}/%.o : ${DIR_SRC}/%.c  ${DIR_SRC}/%.h
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)

${DIR_BIN}/%.o : ${DIR_HOME}/%.c ${DIR_HOME}/%.h ${DIR_SRC}/%.h ${DIR_SRC}/common.h
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)

${DIR_BIN}/%.o : ${DIR_HOME}/%.c ${DIR_HOME}/%.h  ${DIR_SRC}/common.h
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS) $(INCLUDE)


# saubermachen:
# -------------
clean :
	$(RM) $(DIR_BIN)/*.* $(TARGET) $(DIR_OBJ)/.*.sw?
	/home/pi/Sicherung.sh

##*********************************************************************************************//
