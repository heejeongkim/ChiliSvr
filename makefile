all: ChiliSvr

ChiliSvr: ChiliSvr.cpp ChiliSvr.h
	g++ -L/usr/local/lib ChiliSvr.cpp -lzmq -lczmq -o ChiliSvr

clean:
	rm -rf *.o ChiliSvr
