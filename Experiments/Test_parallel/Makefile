SRCCOM += $(wildcard ../../lib/*.c) \
	          $(wildcard ../../lib/*.cpp)

CFLAGS = -Wall -std=c++11 -O3 
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lm 
INCLUDES= -I/home/mina/Documents/github/SeiveSketch/lib -I/usr/local/include/pcapplusplus/

# flow size estimation
FILES = our our_parallel
all: $(FILES)

our: main_our.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

our_parallel: main_parallel.cpp
	g++ -O3 -mavx2 -mbmi -fopenmp $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS) -pthread

clean:
	rm $(FILES)