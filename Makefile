CC=mpicc
CFLAGS=-O3 -g
LDFLAGS=-ljansson


all: app plugin/plugin_echo.so


app : main.c libashell.so
	$(CC) $(CFLAGS) ./main.c -L. -lashell -Wl,-rpath=$(PWD) -o $@

plugin/plugin_echo.so : libashell.so
	$(CC) $(CFLAGS) -fpic -shared ./plugin/plugin_echo.c -I$(PWD) -L$(PWD) -Wl,-rpath=$(PWD)  -o $@

libashell.so : ashell.c
	$(CC) $(CFLAGS) -fpic -shared $^ -o $@ $(LDFLAGS)
	
clean:
	rm -f *.o ./a.out ./app libashell.so ./plugin/*.so
