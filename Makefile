DESTDIR=/
PREFIX=$(DESTDIR)/usr/local
DEPDIR=.deps
LIBS = $(shell pkg-config libcap_utils-0.7 --libs --atleast-version=0.7.14) -lqd
bin_PROGRAMS = bitrate pktrate timescale

all: $(bin_PROGRAMS)

bitrate: bitrate.o extract.o
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

pktrate: pktrate.o extract.o
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

timescale: timescale.o extract.o
	$(CXX) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm -rf *.o $(bin_PROGRAMS) $(DEPDIR)

$(DEPDIR):
	mkdir -p $@

%.o: %.cpp Makefile $(DEPDIR)
	$(CXX) -Wall -std=c++0x -DHAVE_CONFIG_H $(CFLAGS) $(shell pkg-config libcap_utils-0.7 --cflags --atleast-version=0.7.14) -c $< -MD -MF $(DEPDIR)/$(@:.o=.d) -o $@

install: all
	install -m 0755 bitrate $(PREFIX)/bin
	install -m 0755 pktrate $(PREFIX)/bin
	install -m 0755 timescale $(PREFIX)/bin

-include $(wildcard $(DEPDIR)/*.d)
