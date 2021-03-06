# TI-99/sim src/util makefile

include ../../rules.mak

ifdef WIN32
#LIBS     += -lSDLmain
OBJS     += ../../../SDL/lib/SDL_main.o
else
ifdef DEBUG
LIBS     += -Wl,-rpath,/usr/lib -lpthread -lrt
endif
endif

TARGETS  := convert-ctg decode disk dumpcpu dumpgrom dumpspch list mkspch say
OBJS     := convert.o decode.o disk.o dumpcpu.o dumpgrom.o dumpspch.o list.o mkspch.o say.o

all: $(TARGETS)

clean:
	-rm -f *~ *.o *.exe $(TARGETS) .depend

dep:
	gcc -c -MM $(INCLUDES) $(OBJS:%.o=%.cpp) > .depend

../core/ti-core.a:
	$(MAKE) -C ../core

convert-ctg: \
	convert.o				\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

decode: \
	decode.o				\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

disk: \
	disk.o					\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

dumpcpu: \
	dumpcpu.o				\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

dumpgrom: \
	../console/gpl.o			\
	dumpgrom.o				\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

dumpspch: \
	dumpspch.o				\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

list: \
	list.o					\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

mkspch: \
	mkspch.o				\
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

say: \
	say.o					\
	../sdl/tms9919-sdl.o			\
	../core/ti-core.a
	$(CC) -o $@ $^ $(LIBPATH) $(LIBS) `sdl-config --libs`

convert.o: \
	convert.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/cartridge.hpp		\
	../../include/option.hpp		\
	../../include/diskio.hpp		\
	../../include/diskfs.hpp		\
	../../include/support.hpp

decode.o: \
	decode.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/option.hpp

disk.o: \
	disk.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/cartridge.hpp		\
	../../include/diskio.hpp		\
	../../include/diskfs.hpp		\
	../../include/fileio.hpp		\
	../../include/option.hpp		\
	../../include/support.hpp

dumpcpu.o: \
	dumpcpu.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/cartridge.hpp		\
	../../include/diskio.hpp		\
	../../include/diskfs.hpp		\
	../../include/fileio.hpp		\
	../../include/tms9900.hpp		\
	../../include/ti994a.hpp		\
	../../include/compress.hpp		\
	../../include/option.hpp		\
	../../include/support.hpp

dumpgrom.o: \
	dumpgrom.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/cartridge.hpp		\
	../../include/support.hpp		\
	../../include/option.hpp

dumpspch.o: \
	dumpspch.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/option.hpp

list.o: \
	list.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/support.hpp		\
	../../include/diskio.hpp		\
	../../include/iBaseObject.hpp		\
	../../include/cBaseObject.hpp		\
	../../include/diskfs.hpp		\
	../../include/fileio.hpp		\
	../../include/option.hpp

mkspch.o: \
	mkspch.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/option.hpp

say.o: \
	say.cpp					\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/tms5220.hpp		\
	../../include/tms9919.hpp		\
	../../include/tms9919-sdl.hpp		\
	../../include/option.hpp
