# progs/egl/Makefile

TOP = ../..
include $(TOP)/configs/current


INCLUDE_DIRS = -I$(TOP)/include

PROGRAMS = \
	dumb_responder

.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(CFLAGS) $< -o $@

default: $(PROGRAMS)

dumb_responder: dumb_responder.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) dumb_responder.o -L$(TOP)/$(LIB_DIR) -lEGL -L $(TOP)/build/linux-x86_64/lib -lrawgal -L $(TOP)/build/linux-x86_64/gallium/auxiliary/util -lutil -L $(TOP)/build/linux-x86_64/gallium/auxiliary/tgsi -ltgsi -o $@

dumb_responder.o: dumb_responder.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include -I$(TOP)/src/gallium/include -I$(TOP)/src/gallium/auxiliary -I$(TOP)/src/gallium/state_trackers/raw -I$(TOP)/src/gallium/drivers/remote dumb_responder.c

clean:
	rm -f *.o *~
	rm -f *.so
	rm -f $(PROGRAMS)
