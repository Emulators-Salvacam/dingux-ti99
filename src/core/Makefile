# TI-99/sim src/core makefile

include ../../rules.mak

OBJS     := arcfs.o cartridge.o cBaseObject.o compress.o decodelzw.o device.o disassemble.o diskfs.o diskio.o fileio.o fs.o opcodes.o option.o pseudofs.o support.o ti-disk.o ti994a.o tms5220.o tms9900.o tms9901.o tms9918a.o tms9919.o
TARGETS  := ti-core.a

ifdef DEBUG
CFLAGS   += -fexceptions
OBJS     += logger.o string.o
ifdef WIN32
OBJS     += win32-logger.o
else
OBJS     += linux-logger.o
endif
endif

all: $(TARGETS)

clean:
	-rm -f *~ *.o *.a $(TARGETS) .depend

dep:
	gcc -c -MM $(INCLUDES) $(OBJS:%.o=%.cpp) 2>/dev/null > .depend

ti-core.a: $(OBJS)
	$(AR) rc $@ $?
	ranlib $@

arcfs.o: \
	arcfs.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/fs.hpp		\
	../../include/pseudofs.hpp	\
	../../include/arcfs.hpp	\
	../../include/fileio.hpp	\
	../../include/support.hpp

cartridge.o: \
	cartridge.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/compress.hpp	\
	../../include/cartridge.hpp	\
	../../include/support.hpp

cBaseObject.o: \
	cBaseObject.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/cBaseObject.hpp

compress.o: \
	compress.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp

decodelzw.o: \
	decodelzw.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/decodelzw.hpp

device.o: \
	device.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/cartridge.hpp	\
	../../include/tms9900.hpp	\
	../../include/device.hpp

disassemble.o: \
	disassemble.cpp			\
	../../include/common.hpp	\
	../../include/tms9900.hpp

diskfs.o: \
	diskfs.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/fs.hpp		\
	../../include/diskfs.hpp	\
	../../include/fileio.hpp	\
	../../include/support.hpp

diskio.o: \
	diskio.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/support.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/fs.hpp		\
	../../include/diskfs.hpp	\
	../../include/fileio.hpp

fileio.o: \
	fileio.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/fs.hpp		\
	../../include/diskfs.hpp	\
	../../include/pseudofs.hpp	\
	../../include/fileio.hpp	\
	../../include/support.hpp

fs.o: \
	fs.cpp				\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/fs.hpp		\
	../../include/fileio.hpp

opcodes.o: \
	opcodes.cpp			\
	../../include/common.hpp	\
	../../include/tms9900.hpp	\
	../../include/opcodes.hpp	\
	../../include/device.hpp	\
	../../include/tms9901.hpp
	$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -fomit-frame-pointer -fschedule-insns -o $@ $<

option.o: \
	option.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/option.hpp

pseudofs.o: \
	pseudofs.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/fs.hpp		\
	../../include/pseudofs.hpp	\
	../../include/fileio.hpp	\
	../../include/support.hpp

support.o: \
	support.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/support.hpp

ti-disk.o: \
	ti-disk.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/cartridge.hpp	\
	../../include/tms9900.hpp	\
	../../include/device.hpp	\
	../../include/diskio.hpp	\
	../../include/cBaseObject.hpp	\
	../../include/iBaseObject.hpp	\
	../../include/ti-disk.hpp

ti994a.o: \
	ti994a.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/compress.hpp	\
	../../include/tms9900.hpp	\
	../../include/tms9918a.hpp	\
	../../include/tms9919.hpp	\
	../../include/tms5220.hpp	\
	../../include/cartridge.hpp	\
	../../include/ti994a.hpp	\
	../../include/device.hpp	\
	../../include/tms9901.hpp

tms5220.o: \
	tms5220.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/support.hpp	\
	../../include/tms5220.hpp	\
	../../include/tms9919.hpp	\
	../../include/tms9900.hpp	\
	../../include/ti994a.hpp

tms9900.o: \
	tms9900.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/tms9900.hpp	\
	../../include/opcodes.hpp

tms9901.o: \
	tms9901.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/tms9900.hpp	\
	../../include/device.hpp	\
	../../include/tms9901.hpp	\
	../../include/ti994a.hpp

tms9918a.o: \
	tms9918a.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/compress.hpp	\
	../../include/tms9900.hpp	\
	../../include/tms9918a.hpp	\
	../../include/ti994a.hpp	\
	../../include/device.hpp	\
	../../include/tms9901.hpp

tms9919.o: \
	tms9919.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/tms9919.hpp

ifdef DEBUG
logger.o: ../../../common/logger/logger.cpp
	$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -o $@ $<

string.o: ../../../common/logger/string.cpp

	$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -o $@ $<

ifdef WIN32
win32-logger.o: ../../../common/logger/win32-logger.cpp
	$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -o $@ $<
else
linux-logger.o: ../../../common/logger/linux-logger.cpp
	$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -o $@ $<
endif
endif
