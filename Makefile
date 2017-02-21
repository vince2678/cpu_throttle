all: cpu_throttle

cpu_throttle: throttle_functions.o cpu_throttle.o
	$(CC) -g -lm -pthread $^ -o $@

.c.o: $@.c cpu_throttle.h
	$(CC) -Wall -Werror -g $^ -c

clean:
	rm -f *.o *vgcore* cpu_throttle
