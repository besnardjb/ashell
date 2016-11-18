CC=mpicc
CFLAGS=-O3
LDFLAGS=


all: app


app : main.c libashell.so
	$(CC) $(CFLAGS) ./main.c -L. -lashell -Wl,-rpath=$(PWD) -o $@


libashell.so : ashell.c
	$(CC) $(CFLAGS) -fpic -shared $^ -o $@
	
clean:
	rm -f *.o ./a.out ./app libashell.so
