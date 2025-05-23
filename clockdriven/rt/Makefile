CC = g++
CFLAGS = -O3 -Wall -pthread -std=c++11
LFLAGS = -pthread

OUT = librt_pthread.a

all: $(OUT)

librt_pthread.a: rt_pthread.o
	ar -rv $@ $^
	
rt_pthread.o: rt_pthread.cpp affinity.h priority.h
	$(CC) $(CFLAGS) -c rt_pthread.cpp

clean:
	rm -f *.o *~ $(OUT)


