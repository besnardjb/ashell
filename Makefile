CC=mpicc
CFLAGS=-O3 -g -I. -I$(PWD)/bdeps/include/
LDFLAGS=-ljansson -Wl,-rpath=$(PWD) -Wl,-rpath=$(PWD)/bdeps/lib/ -L$(PWD)/bdeps/lib/


all: app plugin/plugin_mem.so plugin/plugin_echo.so


app : main.c libashell.so
	$(CC) $(CFLAGS) ./main.c -L. -lashell -Wl,-rpath=$(PWD) -o $@

plugin/plugin_mem.so : libashell.so
	$(CC) $(CFLAGS) -fpic -shared ./plugin/plugin_mem.c -I$(PWD) -L$(PWD) -Wl,-rpath=$(PWD)  -o $@

plugin/plugin_echo.so : libashell.so
	$(CC) $(CFLAGS) -fpic -shared ./plugin/plugin_echo.c -I$(PWD) -L$(PWD) -Wl,-rpath=$(PWD)  -o $@

libashell.so : ashell.c ./bdeps/lib/libjansson.so
	$(CC) $(CFLAGS) -fpic -shared $^ -o $@ $(LDFLAGS)

bdeps/lib/libjansson.so :
	./deps/build_deps.sh


clean:
	rm -fr *.o ./a.out ./app libashell.so ./plugin/*.so ./bdeps/
