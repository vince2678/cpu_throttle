
FAN_CTRL_DIR = '"/sys/devices/platform/asus_fan/hwmon/hwmon%d/%s"'
CT_HWMON_DIR = '"/sys/devices/platform/coretemp.0/hwmon/hwmon%d/%s"'
SCALING_DIR = '"/sys/devices/system/cpu/cpu%d/cpufreq/%s"'

NAME = cpu_throttle
BINARY_DIR = /usr/bin
SYSTEMD_UNIT_DIR = /etc/systemd/system/multi-user.target.wants

CFLAGS = -Wall -Werror -g
EXTRA_LINKS = -lm -pthread

EXTRA_CFLAGS = -DFAN_CTRL_DIR=$(FAN_CTRL_DIR) \
	       -DCT_HWMON_DIR=$(CT_HWMON_DIR) -DSCALING_DIR=$(SCALING_DIR)

all: $(NAME)

install: $(NAME)
	cp $(NAME) $(BINARY_DIR)/$(NAME)
	cp $(NAME).service $(SYSTEMD_UNIT_DIR)/$(NAME).service
	mkdir -p /etc/$(NAME)
	chmod +x $(BINARY_DIR)/$(NAME)

$(NAME): throttle_functions.o $(NAME).o $(NAME).h
	$(CC) $(CFLAGS) $(EXTRA_LINKS) $(EXTRA_CFLAGS) $^ -o $@

.c.o: $@.c $(NAME).h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $^ -c

uninstall:
	rm -f $(BINARY_DIR)/$(NAME) $(SYSTEMD_UNIT_DIR)/$(NAME).service

clean:
	rm -f *.o $(NAME)
