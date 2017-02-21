
FAN_CTRL_DIR = '"/sys/devices/platform/asus_fan/hwmon/hwmon%d/%s"'
CT_HWMON_DIR = '"/sys/devices/platform/coretemp.0/hwmon/hwmon%d/%s"'
SCALING_DIR = '"/sys/devices/system/cpu/cpu%d/cpufreq/%s"'

CFLAGS = -Wall -Werror -g \
	 -DFAN_CTRL_DIR=$(FAN_CTRL_DIR) \
	 -DCT_HWMON_DIR=$(CT_HWMON_DIR) -DSCALING_DIR=$(SCALING_DIR)

all: cpu_throttle

install: cpu_throttle
	cp cpu_throttle /usr/local/bin/cpu_throttle
	chmod +x /usr/local/bin/cpu_throttle

cpu_throttle: throttle_functions.o cpu_throttle.o cpu_throttle.h
	$(CC) $(CFLAGS) -g -lm -pthread $^ -o $@

.c.o: $@.c cpu_throttle.h
	$(CC) $(CFLAGS) -Wall -Werror -g $^ -c

uninstall:
	rm /usr/local/bin/cpu_throttle

clean:
	rm -f *.o *vgcore* cpu_throttle
