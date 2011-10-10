SRC_DIR = src/
INCLUDE_DIR = include/
CFLAGS = -Wall -g

annotations.o : ${SRC_DIR}annotations.cc ${INCLUDE_DIR}annotations.h
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}annotations.cc

hashfile.o : ${SRC_DIR}hashfile.cc ${INCLUDE_DIR}hashfile.h
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}hashfile.cc

btreefile.o : ${SRC_DIR}btreefile.cc ${INCLUDE_DIR}btreefile.h
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}btreefile.cc

logfile.o : ${SRC_DIR}logfile.cc ${INCLUDE_DIR}logfile.h
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}logfile.cc

utils.o : ${SRC_DIR}utils.cc ${INCLUDE_DIR}utils.h
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}utils.cc

testsuite.o : ${SRC_DIR}testsuite.cc
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}testsuite.cc

profiler.o : ${SRC_DIR}profiler.cc 
	g++ ${CFLAGS} -I${INCLUDE_DIR} -c ${SRC_DIR}profiler.cc

testsuite : annotations.o hashfile.o btreefile.o logfile.o utils.o testsuite.o
	g++ -g annotations.o hashfile.o btreefile.o logfile.o utils.o testsuite.o -o testsuite	

profiler : annotations.o hashfile.o btreefile.o logfile.o utils.o profiler.o
	g++ -g annotations.o hashfile.o btreefile.o logfile.o utils.o profiler.o -o profiler