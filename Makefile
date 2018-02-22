CC = g++
CFLAGS = -c -Wall
LDFLAGS =

CFLAGS +=`pkg-config --cflags gstreamer-1.0 pkg-config --cflags glib-2.0`
LDFLAGS += `pkg-config --libs gstreamer-1.0 pkg-config --libs glib-2.0`
 
main: main.o
	g++ $(LDFLAGS) $< -o $@

main.o: main.cpp
	g++ $(CFLAGS) $< -o $@
