# Time-stamp: <02/11/09 10:40:24 INGA>
# File: Makefile
# LinkAggregator
#clients
#g++ -c bitrate_modified_feb_2012.cpp 
#g++ -o bitrate bitrate_modified_feb_2012.o

CARG=-c -O4 -Wall 
OBJECTd= main.o

targetd= bitrate




all: $(OBJECTd)	
	$(CXX) -o $(targetd) $(OBJECTd) -lqd -L/usr/local/lib -lcap_stream-07

clean:
	rm -f *.o *.exe
	rm -r $(OBJECTd) 

main.o: main.cpp
	$(CXX) $(CARG) main.cpp -lqd -L/usr/local/lib -lcap_stream-07
