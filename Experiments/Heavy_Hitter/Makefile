SRCCOM += $(wildcard ../../lib/*.c) \
	          $(wildcard ../../lib/*.cpp)

CFLAGS = -Wall -std=c++11 -O3 
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lm 
INCLUDES= -I/home/mina/Documents/github/SeiveSketch/lib -I/usr/local/include/pcapplusplus/

# heavy hitter
hh_our: hh_our.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hh_cm: hh_cm.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hh_es: hh_elastic.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hh_ws: hh_waving.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hh_dhs: hh_dhs.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hh_ss: hh_ss.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)