# TI-99/sim src/console makefile

include ../../rules.mak

TARGETS  := ti99sim-console
OBJS     := gpl.o screenio.o ti-main.o ti994a-console.o tms9918a-console.o

ifdef DEBUG
LIBS     += -Wl,-rpath,/usr/lib -lpthread -lrt
endif

all: $(TARGETS)

depend: clean

clean:
	-rm -f *~ *.o *.exe $(TARGETS) .depend

dep:
	gcc -c -MM $(INCLUDES) $(OBJS:%.o=%.cpp) 2>/dev/null > .depend

ti99sim-console: $(OBJS) \
	../core/ti-core.a
	$(CC) $(LIBS) -o $@ $^

../core/ti-core.a:
	$(MAKE) -C ../core

compiler.o: \
	compiler.cpp				\
	../../include/common.hpp

gpl.o: \
	gpl.cpp					\
	../../include/common.hpp

screenio.o: \
	screenio.cpp				\
	../../include/common.hpp		\
	../../include/screenio.hpp

ti-main.o: \
	ti-main.cpp				\
	../../include/common.hpp		\
	../../include/logger.hpp		\
	../../include/tms9900.hpp		\
	../../include/cartridge.hpp		\
	../../include/ti994a.hpp		\
	../../include/ti994a-console.hpp	\
	../../include/tms9918a.hpp		\
	../../include/tms9918a-console.hpp	\
	../../include/device.hpp		\
	../../include/diskio.hpp		\
	../../include/ti-disk.hpp		\
	../../include/screenio.hpp		\
	../../include/option.hpp		\
	../../include/support.hpp

ti994a-console.o: \
	ti994a-console.cpp			\
	../../include/common.hpp		\
	../../include/compress.hpp		\
	../../include/tms9900.hpp		\
	../../include/tms9918a.hpp		\
	../../include/cartridge.hpp		\
	../../include/ti994a.hpp		\
	../../include/ti994a-console.hpp	\
	../../include/tms9918a-console.hpp	\
	../../include/screenio.hpp		\
	../../include/support.hpp

tms9918a-console.o: \
	tms9918a-console.cpp			\
	../../include/common.hpp		\
	../../include/compress.hpp		\
	../../include/tms9900.hpp		\
	../../include/tms9918a.hpp		\
	../../include/tms9918a-console.hpp	\
	../../include/screenio.hpp
