all: cpu_throttle

cpu_throttle: cpu_throttle.o
	$(CC) -g -lm -pthread $^ -o $@

.c.o: $@.c
	$(CC) -Wall -Werror -g $^ -c

clean:
	rm -f *.o *vgcore* cpu_throttle
