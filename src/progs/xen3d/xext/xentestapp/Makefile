
hello: hello.o ext.o
	gcc hello.o ext.o -o hello -lX11 -lXext

hello.o: HelloX.c
	gcc -c -ggdb HelloX.c -o hello.o

ext.o: ext.c
	gcc -c -ggdb ext.c -o ext.o -I/usr/include/X11/extensions

rxapp: rxapp.c
	gcc -ggdb rxapp.c -o rxapp -I/root/sources/Xbuild/include -L/root/sources/Xbuild/lib

