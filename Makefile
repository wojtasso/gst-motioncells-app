CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
CPPFLAGS = -I. -I/usr/local/include/ `pkg-config --cflags gstreamer-video-1.0 cairo`
LDFLAGS = `pkg-config --libs gstreamer-video-1.0 cairo`

CPPOUTFILE =motion-detect
CPPOBJS =$(CPPSOURCE:.cpp=.o)
CPPSOURCE =main.cpp Motion.cpp

all: $(CPPOUTFILE)

$(CPPOUTFILE): $(CPPSOURCE)
	$(CXX) $(CXXFLAGS) $(CPPSOURCE) -o $(CPPOUTFILE) $(CPPFLAGS) $(LDFLAGS)

udpsrc: $(CPPSOURCE)
	$(CXX) $(CXXFLAGS) -DUDPSRC $(CPPSOURCE) -o $(CPPOUTFILE) $(CPPFLAGS) $(LDFLAGS)
clean:
	rm -f *.o $(CPPOUTFILE)
