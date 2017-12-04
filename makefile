program:bid.o	
	cc -o bid bid.o	-lpthread
bid.o:bid.c
	cc -c bid.c
clean:
	rm bid *.o
