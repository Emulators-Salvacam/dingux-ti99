# TI-99/sim src/sdl makefile

include ../../rules.mak

TARGETS  := ti99sim-sdl
OBJS      = main.o bitmap.o tms9919-sdl.o tms9918a-sdl.o ti994a-sdl.o

ifdef WIN32
#LIBS     += -lSDLmain
OBJS     += ../../../SDL/lib/SDL_main.o
else
LIBS     += -Wl,-rpath,/usr/lib -lpthread -lrt
endif

all: $(TARGETS)

clean:
	-rm -f *~ *.o *.exe std*.txt $(TARGETS) .depend

dep:
	gcc -c -MM $(INCLUDES) $(OBJS:%.o=%.cpp) 2>/dev/null > .depend

ti99sim-sdl: $(OBJS) \
	../core/ti-core.a
	$(CC) -o $@ $^ $(LIBPATH) $(LIBS) `sdl-config --libs`

../core/ti-core.a:
	$(MAKE) -C ../core

main.o: \
	main.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/cartridge.hpp	\
	../../include/tms9900.hpp	\
	../../include/ti994a.hpp	\
	../../include/ti994a-sdl.hpp	\
	../../include/tms9918a.hpp	\
	../../include/tms9918a-sdl.hpp	\
	../../include/tms9919.hpp	\
	../../include/tms9919-sdl.hpp	\
	../../include/tms5220.hpp	\
	../../include/device.hpp	\
	../../include/diskio.hpp	\
	../../include/ti-disk.hpp	\
	../../include/support.hpp	\
	../../include/option.hpp

bitmap.o: \
	bitmap.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/tms9900.hpp	\
	../../include/tms9918a.hpp	\
	../../include/ti994a.hpp	\
	../../include/bitmap.hpp	\
	../../include/tms9918a-sdl.hpp

tms9919-sdl.o: \
	tms9919-sdl.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/tms9919.hpp	\
	../../include/tms9919-sdl.hpp	\
	../../include/tms5220.hpp

tms9918a-sdl.o: \
	tms9918a-sdl.cpp		\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/bitmap.hpp	\
	../../include/tms9900.hpp	\
	../../include/tms9918a.hpp	\
	../../include/ti994a.hpp	\
	../../include/tms9918a-sdl.hpp

ti994a-sdl.o: \
	ti994a-sdl.cpp			\
	../../include/common.hpp	\
	../../include/logger.hpp	\
	../../include/bitmap.hpp	\
	../../include/compress.hpp	\
	../../include/cartridge.hpp	\
	../../include/tms9900.hpp	\
	../../include/tms9918a.hpp	\
	../../include/tms9919.hpp	\
	../../include/ti994a.hpp	\
	../../include/ti994a-sdl.hpp	\
	../../include/support.hpp	\
	../../include/device.hpp	\
	../../include/tms9901.hpp
