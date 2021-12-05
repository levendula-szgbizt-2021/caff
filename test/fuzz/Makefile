AFLCC           = afl-clang
AFLFUZZ         = afl-fuzz

ROOT_DIR        = ../..
INPUT_DIR       = afl_in
OUTPUT_DIR      = afl_out

BIN             = caff
INPUT           = ${INPUT_DIR}/in


all: fuzz

fuzz: ${BIN} ${INPUT}
	@mkdir -p ${OUTPUT_DIR}
	@${AFLFUZZ} -t 16000+ -i ${INPUT_DIR} -o ${OUTPUT_DIR} -- ./${BIN}

${INPUT}:
	@mkdir -p ${INPUT_DIR}
	@${ROOT_DIR}/test/gentest \
	    -a 'John Smith' \
	    -d 2021-11-18T12:38:41 \
	    -o ${INPUT} \
	    1000 4 4 Hungary flag,stripes huflag \
	    1000 4 4 Italy flag,stripes itflag

${BIN}:
	@make -C ${ROOT_DIR} clean
	@make -C ${ROOT_DIR} CC=${AFLCC} ${BIN}
	@cp ${ROOT_DIR}/${BIN} ${BIN}

clean:
	rm -rf ${BIN} ${INPUT_DIR} ${OUTPUT_DIR}


.PHONY: fuzz clean
