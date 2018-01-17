# see LICENSE file for copyright and license information.

NAME=sstat
VERSION = 0.2

CC = gcc
SRC = ${NAME}.c
OBJ = ${SRC:.c=.o}
CFLAGS = `pkg-config --libs libpulse x11 alsa` -Wno-discarded-qualifiers -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wall -Wextra -Os -DVERSION=\"${VERSION}\" -D_GNU_SOURCE
DESTDIR = /usr/local

all: options ${NAME}

options:
	@echo ${NAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h 

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

${NAME}: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${CFLAGS}

clean:
	@echo cleaning
	@rm -f ${NAME} *.o

install: ${NAME}
	@echo installing executable file to ${DESTDIR}/bin
	@mkdir -p ${DESTDIR}/bin
	@cp -f ${NAME} ${DESTDIR}/bin/${NAME}
	@chmod 755 ${DESTDIR}/bin/${NAME}
uninstall: ${NAME}
	@echo removing executable file from ${DESTDIR}/bin
	@rm -f ${DESTDIR}/bin/${NAME}
