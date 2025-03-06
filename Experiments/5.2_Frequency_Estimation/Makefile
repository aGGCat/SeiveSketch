SRCCOM += $(wildcard ../../lib/*.c) \
	          $(wildcard ../../lib/*.cpp)

CFLAGS = -Wall -std=c++11 -O3 
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lm 
INCLUDES= -I/home/mina/Documents/github/SeiveSketch/lib -I/usr/local/include/pcapplusplus/

# flow size estimation
FILES = our cm cu elastic cf as laf
all: $(FILES)

our: main_our.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

cm: main_cm.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

cu: main_cu.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

elastic: main_elastic.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

cf: main_cf.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

as: main_as.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

laf: main_laf.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

clean:
	rm $(FILES)