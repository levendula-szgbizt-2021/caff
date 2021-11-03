LIB             = libcaff.a
CLI             = caff

MAN1            =
MAN3            =

SRCS            = caff.c cli.c
OBJS            = ${SRCS:.c=.o}

CFLAGS         += -O2 -pipe
CFLAGS         += -Wall
CFLAGS         += -Wstrict-prototypes -Wmissing-prototypes
CFLAGS         += -Wmissing-declarations
CFLAGS         += -Wshadow -Wpointer-arith
CFLAGS         += -Wsign-compare -Wcast-qual

LDFLAGS        += ${LIBS}

MANPREFIX      ?= /usr/local/man
PREFIX         ?= /usr/local

ifdef DEBUG
CFLAGS += -g -O0
endif


all: ${LIB} ${CLI}

${LIB}: ${OBJS}
	${AR} rcs ${LIB} ciff.o

${CLI}: ${OBJS}
	${CC} -o ${CLI} ${CFLAGS} ${LDFLAGS} ${OBJS}

install-lib: ${LIB}
	install ${LIB} ${PREFIX}/lib/

install-cli: ${CLI}
	install ${CLI} ${PREFIX}/bin/

install-man: ${MAN}
	install -m 0644 ${MAN1} ${MANPREFIX}/man1/
	install -m 0644 ${MAN3} ${MANPREFIX}/man3/

deinstall-lib:
	rm -f ${PREFIX}/lib/${LIB}

deinstall-cli:
	rm -f ${PREFIX}/bin/${CLI}

deinstall-man:
	rm -f ${MANPREFIX}/man1/${MAN1} ${MANPREFIX}/man3/${MAN3}

install: install-lib install-cli install-man

deinstall: deinstall-lib deinstall-cli deinstall-man

clean:
	rm -f ${LIB} ${CLI} ${OBJS}

.PHONY: clean
.PHONY: install deinstall
.PHONY: install-lib deinstall-lib
.PHONY: install-cli deinstall-cli
.PHONY: install-man deinstall-man
