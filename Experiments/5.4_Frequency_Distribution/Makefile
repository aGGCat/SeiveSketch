SRCCOM += $(wildcard ../../lib/*.c) \
	          $(wildcard ../../lib/*.cpp)

CFLAGS = -Wall -std=c++11 -O3 
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lm 
INCLUDES= -I/home/mina/Documents/github/SeiveSketch/lib -I/usr/local/include/pcapplusplus/

# frequency distribution
FILES = fd_our fd_cm fd_cu fd_as fd_cf fd_es fd_laf
all: $(FILES)

fd_our: fredist_our.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

fd_cm: fredist_cm.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

fd_cu: fredist_cu.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

fd_as: fredist_as.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

fd_cf: fredist_cf.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

fd_es: fredist_es.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

fd_laf: fredist_laf.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

clean:
	rm $(FILES)