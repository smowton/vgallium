# progs/egl/Makefile

TOP = ../..
include $(TOP)/configs/current


INCLUDE_DIRS = -I$(TOP)/include

HEADERS = $(TOP)/include/GLES/egl.h

PROGRAMS = \
	demo1 \
	demo2 \
	demo3 \
	egltri \
	eglinfo \
	eglgears \
	peglgears \
	xeglgears \
	xegl_tri \
	xegl_rawgal_tri \
	xegl_rawgal_init \
	xegl_rawgal_texquad \
	xegl_rawgal_comp \
	xegl_rawgal_asynccomp \
	xegl_rawgal_2d3dcomp


.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(CFLAGS) $< -o $@



default: $(PROGRAMS)


demo1: demo1.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) demo1.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) -o $@

demo1.o: demo1.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include demo1.c


demo2: demo2.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) demo2.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) $(APP_LIB_DEPS) -o $@

demo2.o: demo2.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include demo2.c


demo3: demo3.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) demo3.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) $(APP_LIB_DEPS) -o $@

demo3.o: demo3.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include demo3.c


egltri: egltri.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) egltri.o -L$(TOP)/$(LIB_DIR) -lGL -lEGL $(LIBDRM_LIB) -o $@

egltri.o: egltri.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include egltri.c


eglinfo: eglinfo.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) eglinfo.o -L$(TOP)/$(LIB_DIR) -lGL -lEGL $(LIBDRM_LIB) -o $@

eglinfo.o: eglinfo.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include eglinfo.c


eglgears: eglgears.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) eglgears.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) $(APP_LIB_DEPS) -o $@

eglgears.o: eglgears.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include eglgears.c


peglgears: peglgears.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) peglgears.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) $(APP_LIB_DEPS) -o $@

peglgears.o: peglgears.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include peglgears.c


xeglgears: xeglgears.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xeglgears.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) $(APP_LIB_DEPS) -o $@

xeglgears.o: xeglgears.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include xeglgears.c


xegl_tri: xegl_tri.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xegl_tri.o -L$(TOP)/$(LIB_DIR) -lEGL $(LIBDRM_LIB) $(APP_LIB_DEPS) -o $@

xegl_tri.o: xegl_tri.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include xegl_tri.c

xegl_rawgal_tri: xegl_rawgal_tri.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xegl_rawgal_tri.o -L$(TOP)/$(LIB_DIR) -lEGL -L $(TOP)/build/linux-x86/lib -lrawgal -L $(TOP)/build/linux-x86/gallium/auxiliary/util -lutil -L $(TOP)/build/linux-x86/gallium/auxiliary/tgsi -ltgsi -o $@

xegl_rawgal_tri.o: xegl_rawgal_tri.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include -I$(TOP)/src/gallium/include -I$(TOP)/src/gallium/auxiliary -I$(TOP)/src/gallium/state_trackers/raw xegl_rawgal_tri.c

xegl_rawgal_init: xegl_rawgal_init.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xegl_rawgal_init.o -L$(TOP)/$(LIB_DIR) -lEGL -o $@

xegl_rawgal_init.o: xegl_rawgal_init.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include xegl_rawgal_init.c
	
xegl_rawgal_texquad: xegl_rawgal_texquad.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xegl_rawgal_texquad.o -L$(TOP)/$(LIB_DIR) -lEGL -L $(TOP)/build/linux-x86/lib -lrawgal -L $(TOP)/build/linux-x86/gallium/auxiliary/util -lutil -L $(TOP)/build/linux-x86/gallium/auxiliary/tgsi -ltgsi -o $@

xegl_rawgal_texquad.o: xegl_rawgal_texquad.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include -I$(TOP)/src/gallium/include -I$(TOP)/src/gallium/auxiliary -I$(TOP)/src/gallium/state_trackers/raw xegl_rawgal_texquad.c

xegl_rawgal_comp: xegl_rawgal_comp.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xegl_rawgal_comp.o -L$(TOP)/$(LIB_DIR) -lEGL -L $(TOP)/build/linux-x86/lib -lrawgal -L $(TOP)/build/linux-x86/gallium/auxiliary/util -lutil -L $(TOP)/build/linux-x86/gallium/auxiliary/tgsi -ltgsi -o $@

xegl_rawgal_comp.o: xegl_rawgal_comp.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include -I$(TOP)/src/gallium/include -I$(TOP)/src/gallium/auxiliary -I$(TOP)/src/gallium/state_trackers/raw xegl_rawgal_comp.c

xegl_rawgal_asynccomp: xegl_rawgal_asynccomp.o $(TOP)/$(LIB_DIR)/libEGL.so
	$(CC) $(CFLAGS) xegl_rawgal_asynccomp.o -L$(TOP)/$(LIB_DIR) -lEGL -L $(TOP)/build/linux-x86/lib -lrawgal -L $(TOP)/build/linux-x86/gallium/auxiliary/util -lutil -L $(TOP)/build/linux-x86/gallium/auxiliary/tgsi -ltgsi -o $@

xegl_rawgal_asynccomp.o: xegl_rawgal_asynccomp.c $(HEADERS)
	$(CC) -c $(CFLAGS) -I$(TOP)/include -I$(TOP)/src/gallium/include -I$(TOP)/src/gallium/auxiliary -I$(TOP)/src/gallium/state_trackers/raw xegl_rawgal_asynccomp.c

clean:
	rm -f *.o *~
	rm -f *.so
	rm -f $(PROGRAMS)

run:
	LD_LIBRARY_PATH=$(TOP)/lib ./egltri

debug:
	LD_LIBRARY_PATH=$(TOP)/lib gdb ./eglgears
