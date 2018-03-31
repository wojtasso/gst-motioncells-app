CPP = g++
CPPINCLUDE_DIRS = -I. -I/usr/local/include/ `pkg-config --cflags gstreamer-video-1.0 cairo`
CPPFLAGS = -std=c++11 -Wall -Wextra $(CPPINCLUDE_DIRS)

CPPOUTFILE =motion-detect
CPPOBJS =$(CPPSOURCE:.cpp=.o)
CPPSOURCE =$(wildcard main.cpp)

all: $(CPPOUTFILE)

$(CPPOUTFILE): $(CPPOBJS)
	$(CPP) $(CPPFLAGS) $(CPPOBJS) -o $(CPPOUTFILE) `pkg-config --libs gstreamer-video-1.0 cairo`
clean:
	rm -f *.o $(CPPOUTFILE)
