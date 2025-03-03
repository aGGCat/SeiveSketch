SRCCOM += $(wildcard ../../lib/*.c) \
	          $(wildcard ../../lib/*.cpp)

CFLAGS = -Wall -std=c++11 -O3 
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lm 
INCLUDES= -I/home/mina/Documents/github/SeiveSketch/lib -I/usr/local/include/pcapplusplus/

# heavy change
hc_our: hc_our.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hc_es: hc_elastic.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hc_ws: hc_waving.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hc_dhs: hc_dhs.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

hc_ss: hc_ss.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)