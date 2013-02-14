CARG = -std=c99 -Wall

trimnl: trimnl.o
	gcc $(CARG) -o trimnl trimnl.o

trimnl.o: trimnl.c
	gcc $(CARG) -c trimnl.c

clean:
	rm trimnl trimnl.o

.phony: clean
