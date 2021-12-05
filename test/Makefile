TESTS != ls test-*

all: ${TESTS}
	@for t in ${TESTS}; do \
	    echo \\n================ $$t ================; \
	    if [ "${VERBOSE}" -eq 1 ]; then args=-v; else args=; fi; \
	    ./$$t $$args; \
	done
	@echo
