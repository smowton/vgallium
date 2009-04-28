
xen3dd: main.c
	gcc main.c -o xen3dd -std=gnu99 -lxenctrl -lxenstore -ggdb

install: xen3dd
	cp $^ $(DESTDIR)/usr/bin/xen3dd