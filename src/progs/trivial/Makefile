# progs/tests/Makefile


# These programs aren't intended to be included with the normal distro.
# They're not too interesting but they're good for testing.

TOP = ../..
include $(TOP)/configs/current


LIBS = $(APP_LIB_DEPS)

SOURCES = \
	clear-fbo-tex.c \
	clear-fbo.c \
	clear-scissor.c \
	clear-undefined.c \
	clear.c \
	dlist-dangling.c \
	dlist-edgeflag-dangling.c \
	dlist-edgeflag.c \
	drawarrays.c \
	drawelements.c \
	drawrange.c \
	flat-clip.c \
	fs-tri.c \
	line-clip.c \
	line-cull.c \
	line-smooth.c \
	line-stipple-wide.c \
	line-userclip-clip.c \
	line-userclip-nop-clip.c \
	line-userclip-nop.c \
	line-userclip.c \
	line-wide.c \
	line.c \
	lineloop-clip.c \
	lineloop-elts.c \
	lineloop.c \
	linestrip-flat-stipple.c \
	linestrip-stipple-wide.c \
	linestrip-stipple.c \
	linestrip.c \
	long-fixed-func.c \
	pgon-mode.c \
	point-clip.c \
	point-param.c \
	point-sprite.c \
	point-wide.c \
	point-wide-smooth.c \
	point.c \
	poly-flat.c \
	poly-flat-clip.c \
	poly-flat-unfilled-clip.c \
	poly-unfilled.c \
	poly.c \
	quad-clip-all-vertices.c \
	quad-clip-nearplane.c \
	quad-clip.c \
	quad-degenerate.c \
	quad-flat.c \
	quad-offset-factor.c \
	quad-offset-unfilled.c \
	quad-offset-units.c \
	quad-tex-2d.c \
	quad-tex-3d.c \
	quad-tex-alpha.c \
	quad-tex-pbo.c \
	quad-unfilled-clip.c \
	quad-unfilled-stipple.c \
	quad-unfilled.c \
	quad.c \
	quads.c \
	quadstrip-cont.c \
	quadstrip-flat.c \
	quadstrip.c \
	tri-orig.c \
	tri-alpha.c \
	tri-blend-color.c \
	tri-blend-max.c \
	tri-blend-min.c \
	tri-blend-revsub.c \
	tri-blend-sub.c \
	tri-blend.c \
	tri-clip.c \
	tri-cull-both.c \
	tri-cull.c \
	tri-dlist.c \
	tri-edgeflag.c \
	tri-fbo-tex.c \
	tri-fbo.c \
	tri-flat-clip.c \
	tri-flat.c \
	tri-fog.c \
	tri-mask-tri.c \
	tri-query.c \
	tri-scissor-tri.c \
	tri-stencil.c \
	tri-tex-3d.c \
	tri-tri.c \
	tri-unfilled-edgeflag.c \
	tri-unfilled-clip.c \
	tri-unfilled-smooth.c \
	tri-unfilled-userclip-stip.c \
	tri-unfilled-userclip.c \
	tri-unfilled.c \
	tri-userclip.c \
	tri-z-eq.c \
	tri-z.c \
	tri.c \
	trifan-flat.c \
	trifan-flat-clip.c \
	trifan-flat-unfilled-clip.c \
	trifan-unfilled.c \
	trifan.c \
	tristrip-clip.c \
	tristrip-flat.c \
	tristrip.c \
	vbo-drawarrays.c \
	vbo-drawelements.c \
	vbo-drawrange.c \
	vp-array.c \
	vp-array-int.c \
	vp-clip.c \
	vp-line-clip.c \
	vp-tri.c \
	vp-unfilled.c 

PROGS = $(SOURCES:%.c=%)

INCLUDES = -I. -I$(TOP)/include -I../samples

UTIL_FILES = readtex.h readtex.c


##### RULES #####

.SUFFIXES:
.SUFFIXES: .c

.c:
	$(APP_CC) $(INCLUDES) $(CFLAGS) $< $(LIBS) -o $@

.c.o:
	$(APP_CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $< -o $@

.S.o:
	$(APP_CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES)  $< -o $@


##### TARGETS #####

default: $(UTIL_FILES) $(PROGS)

clean:
	rm -f $(PROGS)
	rm -f *.o
	rm -f getproclist.h


# auto code generation
getprocaddress: getprocaddress.c getproclist.h

getproclist.h: $(TOP)/src/mesa/glapi/gl_API.xml getprocaddress.c getprocaddress.py
	python getprocaddress.py > getproclist.h


texrect: texrect.o readtex.o
	$(APP_CC) texrect.o readtex.o $(LIBS) -o $@

texrect.o: texrect.c readtex.h
	$(APP_CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $< -o $@

invert: invert.o readtex.o
	$(APP_CC) invert.o readtex.o $(LIBS) -o $@

invert.o: invert.c readtex.h
	$(APP_CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $< -o $@

readtex.o: readtex.c
	$(APP_CC) -c $(INCLUDES) $(CFLAGS) $(DEFINES) $< -o $@


readtex.h: $(TOP)/progs/util/readtex.h
	ln -s $(TOP)/progs/util/readtex.h .

readtex.c: $(TOP)/progs/util/readtex.c
	ln -s $(TOP)/progs/util/readtex.c .


fs-tri: fs-tri.c extfuncs.h


extfuncs.h:  $(TOP)/progs/util/extfuncs.h
	cp $< .


# Emacs tags
tags:
	etags `find . -name \*.[ch]` `find ../include`
