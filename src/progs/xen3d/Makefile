
include Makefile.config

X11_INCLUDE = /usr/include
XEXT_INCLUDE = /usr/include
QEMU_INCLUDE = ./qemu/
INCLUDES = -I./include -I../../src/gallium/include -I../../src/gallium/auxiliary -I../../include -I../../src/gallium/drivers/remote -I../../src/gallium/state_trackers/raw -I$(X11_INCLUDE) -I$(X11_INCLUDE)/pixman-1 -I$(QEMU_INCLUDE) -I$(XEXT_INCLUDE)

CFLAGS = -std=gnu99 -ggdb -D_GNU_SOURCE

default: xen3d

obj/main.o: src/main.c
	gcc src/main.c -c -o obj/main.o $(INCLUDES) $(CFLAGS)

obj/master_context.o: src/master_context.c
	gcc src/master_context.c -c -o obj/master_context.o $(INCLUDES) $(CFLAGS)

obj/dispatch.o: src/dispatch.c
	gcc src/dispatch.c -c -o obj/dispatch.o $(INCLUDES) $(CFLAGS)

obj/context.o: src/context.c
	gcc src/context.c -c -o obj/context.o $(INCLUDES) $(CFLAGS)

obj/screen.o: src/screen.c
	gcc src/screen.c -c -o obj/screen.o $(INCLUDES) $(CFLAGS)

obj/winsys.o: src/winsys.c
	gcc src/winsys.c -c -o obj/winsys.o $(INCLUDES) $(CFLAGS)

obj/map_list.o: src/map_list.c
	gcc src/map_list.c -c -o obj/map_list.o $(INCLUDES) $(CFLAGS)

obj/debug.o: src/debug.c
	gcc src/debug.c -c -o obj/debug.o $(INCLUDES) $(CFLAGS)

xen3d: obj/main.o obj/master_context.o obj/dispatch.o obj/context.o obj/screen.o obj/winsys.o obj/map_list.o obj/debug.o
	gcc obj/*.o -o xen3d -L ../../$(MESA_LIB_DIR) -lEGL -L../../$(GALLIUM_BUILD_DIR)/lib -lrawgal -L ../../$(GALLIUM_BUILD_DIR)/gallium/auxiliary/util -lutil -L ../../$(GALLIUM_BUILD_DIR)/gallium/auxiliary/tgsi -ltgsi -L/usr/X11R6/lib -lX11 -lXext -lXxf86vm -lXdamage -lXfixes -lxenctrl -lxenstore -lHermes

clean: 
	rm obj/*.o
	rm xen3d
