SRCCOM += $(wildcard ../../lib/*.c) \
	          $(wildcard ../../lib/*.cpp)

CFLAGS = -Wall -std=c++11 -O3 
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lm 
INCLUDES= -I/home/mina/Documents/github/SeiveSketch/lib -I/usr/local/include/pcapplusplus/

# frequency estimation under different skewness
FILES = skew_cm skew_cu skew_elastic skew_cf skew_our skew_as skew_laf
all: $(FILES)

skew_our: skew_our.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

skew_cm: skew_cm.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

skew_cu: skew_cu.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

skew_elastic: skew_elastic.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

skew_cf: skew_cf.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

skew_as: skew_as.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

skew_laf: skew_laf.cpp
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRCCOM) $(LIBS)

clean:
	rm $(FILES)