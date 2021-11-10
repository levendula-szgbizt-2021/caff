LIB             = libcaff.a
CLI             = caff

MAN1            =
MAN3            =

SRCS            = caff.c cli.c
HDRS            = caff.h
OBJS            = ${SRCS:.c=.o}
LIBS            = -ljpeg -lciff
LIBS           += -lMagickWand-6.Q16 -lMagickCore-6.Q16

CFLAGS         += -O2 -pipe -pedantic -std=c99
CFLAGS         += -Wall -Wextra
CFLAGS         += -Wstrict-prototypes -Wmissing-prototypes
CFLAGS         += -Wmissing-declarations
CFLAGS         += -Wshadow -Wpointer-arith
CFLAGS         += -Wsign-compare -Wcast-qual

CFLAGS         += -DMAGICKCORE_QUANTUM_DEPTH=16
CFLAGS         += -DMAGICKCORE_HDRI_ENABLE=0

LDFLAGS        += ${LIBS}

PREFIX         ?= /usr/local


all: ${LIB} ${CLI}

${LIB}: ${OBJS}
	${AR} rcs ${LIB} caff.o

${CLI}: ${OBJS}
	${CC} -o ${CLI} ${CFLAGS} ${OBJS} ${LDFLAGS}

install-lib: ${LIB}
	install ${LIB} ${PREFIX}/lib/
	install ${HDRS} ${PREFIX}/include/

install-cli: ${CLI}
	install ${CLI} ${PREFIX}/bin/

install-man: ${MAN}
	install -m 0644 ${MAN1} ${PREFIX}/man/man1/
	install -m 0644 ${MAN3} ${PREFIX}/man/man3/

deinstall-lib:
	rm -f ${PREFIX}/lib/${LIB}
	@sh -xc \
	    'for h in ${HDRS}; do rm -f "${PREFIX}/include/$$h"; done'

deinstall-cli:
	rm -f ${PREFIX}/bin/${CLI}

deinstall-man:
	rm -f ${PREFIX}/man/man1/${MAN1} ${PREFIX}/man/man3/${MAN3}

install: install-lib install-cli install-man

deinstall: deinstall-lib deinstall-cli deinstall-man

clean:
	rm -f ${LIB} ${CLI} ${OBJS}

.PHONY: clean
.PHONY: install deinstall
.PHONY: install-lib deinstall-lib
.PHONY: install-cli deinstall-cli
.PHONY: install-man deinstall-man
