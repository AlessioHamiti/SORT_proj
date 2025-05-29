CC = g++
CFLAGS = -O3 -Wall -pthread -std=c++11
LFLAGS = -Lrt -pthread -lrt_pthread

OUT = rt/librt_pthread.a application_1 application_2 application_3

all : $(OUT)
	
application_%: application_%.o executive.o busy_wait.o
	$(CC) -o $@ $^ $(LFLAGS)

application_%.o: application_%.cpp executive.h busy_wait.h
	$(CC) $(CFLAGS) -c -o $@ $<

executive.o: executive.cpp executive.h
	$(CC) $(CFLAGS) -c executive.cpp

busy_wait.o: busy_wait.cpp busy_wait.h
	$(CC) $(CFLAGS) -c busy_wait.cpp

rt/librt_pthread.a:
	cd rt; make

clean:
	rm -f *.o *~ $(OUT)
	cd rt; make clean



