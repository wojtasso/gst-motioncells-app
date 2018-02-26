CPP = g++
CPPINCLUDE_DIRS = -I. -I/usr/local/include/ `pkg-config --cflags gstreamer-1.0`
CPPFLAGS = -std=c++11 -Wall -Wextra $(CPPINCLUDE_DIRS)

CPPOUTFILE =motion-detect
CPPOBJS =$(CPPSOURCE:.cpp=.o)
CPPSOURCE =$(wildcard main.cpp)
CPPLIBS = -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0

all: $(CPPOUTFILE)

$(CPPOUTFILE): $(CPPOBJS)
	$(CPP) $(CPPFLAGS) $(CPPOBJS) -o $(CPPOUTFILE) `pkg-config --libs gstreamer-1.0` $(CPPLIBS)
clean:
	rm -f *.o $(CPPOUTFILE)
