/**
* throttle_functions.c
*
* Copyright (C) 2017 Vincent Zvikaramba <vincent.zvikaramba@mail.utoronto.ca>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This source is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with the source code.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <getopt.h>
#include "cpu_throttle.h"

/* Read the file at filename and returns the integer
 * value in the file.
 *
 * @prereq: Assumes that integer is non-negative.
 *
 * @return: integer value read if succesful, -1 otherwise. */
int read_integer(const char* filename) {

	int retval;
	FILE * file;

	/* open the file */
	if (!(file = fopen(filename, "r"))) {
		perror("open");
		LOGE("%s\n", getpid(), strerror(errno));
		return -1;
	}
	/* read the value from the file */
	fscanf(file, "%d", &retval);
	fclose(file);

	return retval;
}
	
/* Write value to the file at filename.
 *
 * value is written to file as a string, not an integer.
 *
 * @return: 0 if succesful, -1 otherwise. */
int write_integer(const char* filename, int value)
{
	int fd;

	/* open the file */
	if ((fd = open(filename, O_RDWR)) == -1) {
		perror("open");
		LOGE("%s\n", getpid(), strerror(errno));
		return -1;
	}
	/* write the string to the file */
	dprintf(fd, "%d", value);
	close(fd);
	return 0;
}

/* Write out_str to the file at filename.
 *
 * @return: 0 if succesful, -1 otherwise. */
int write_string(const char* filename, const char* out_str)
{
	int fd;

	/* open the file */
	if ((fd = open(filename, O_RDWR)) == -1) {
		perror("open");
		LOGE("%s\n", getpid(), strerror(errno));
		return -1;
	}
	/* write the string to the file */
	dprintf(fd, "%s", out_str);
	close(fd);
	return 0;
}

/* Set the scaling governor on cpu core to governor
 *
 * @return: 0 if succesful, -1 otherwise. */
int set_governor(int core, const char* governor)
{
	char filename[MAX_BUF_SIZE];

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_governor");

	/* write the string to the file */
	return write_string(filename, governor);
}

/* Reset the maximum frequency on cpu core to the
 * maximum defined in sysfs
 *
 * @return: 0 if succesful, -1 otherwise. */
int reset_max_freq(int core)
{
	char filename[MAX_BUF_SIZE];

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	LOGI("\tResetting cpu %d speed to %d.\n", getpid(), core, settings.cpuinfo_max_freq);

	/* write the string to the file and return */
	return write_integer(filename, settings.cpuinfo_max_freq);
}

/* Decrease the maximum frequency on cpu core by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int decrease_max_freq(int core, int step)
{
	char filename[MAX_BUF_SIZE];
	int freq;

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	/* read the current max frequency */
	freq = read_integer(filename);

	/* determine the new frequency */
	freq -= step;
	if (freq < settings.cpuinfo_min_freq) {
		freq = settings.cpuinfo_min_freq;
		LOGI("\tDecreasing cpu%d speed to %d KHz.\n",
			getpid(), core, freq);
	}
	else {
		LOGI("\tDecreasing cpu%d speed by %d KHz.\n",
			getpid(), core, step);
	}

	/* write the string to the file and return */
	return write_integer(filename, freq);
}

/* Increase the maximum frequency on cpu core by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int increase_max_freq(int core, int step)
{
	char filename[MAX_BUF_SIZE];
	int freq;

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	/* read the current max frequency */
	freq = read_integer(filename);

	/* determine the new frequency */
	freq += step;
	if (freq > settings.cpu_target_freq) {
		freq = settings.cpu_target_freq;
		LOGI("\tIncreasing cpu%d speed to %d KHz.\n",
			getpid(), core, freq);
	}
	else {
		LOGI("\tIncreasing cpu%d speed by %d KHz.\n",
			getpid(), core, step);
	}

	/* write the string to the file and return */
	return write_integer(filename, freq);
}

/* Reset the fan speed to the target fan speed.
 *
 * @return: 0 if succesful, -1 otherwise. */
int reset_fan_speed(void)
{
	char fanctrl_filename[MAX_BUF_SIZE];
	char filename_buf[MIN_BUF_SIZE];

	/* format the full file name */
	sprintf(filename_buf, "pwm%d", settings.sysfs_fanctrl_hwmon_subnode);
	sprintf(fanctrl_filename, FAN_CTRL_DIR,
			settings.sysfs_fanctrl_hwmon_node, filename_buf);

	LOGI("\tResetting fan speed to %d.\n", getpid(), settings.fan_target_speed);

	/* set the fan speed */
	return write_integer(fanctrl_filename, settings.fan_target_speed);
}

/* Increase the fan speed by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int increase_fan_speed(int step)
{
	char fanctrl_filename[MAX_BUF_SIZE];
	char filename_buf[MIN_BUF_SIZE];
	int fan_speed;

	/* format the full file name */
	sprintf(filename_buf, "pwm%d", settings.sysfs_fanctrl_hwmon_subnode);
	sprintf(fanctrl_filename, FAN_CTRL_DIR,
			settings.sysfs_fanctrl_hwmon_node, filename_buf);

	/* read the current fan speed */
	fan_speed = read_integer(fanctrl_filename);

	/* determine the new fan speed */
	fan_speed += step;
	if (fan_speed > settings.fan_max_speed) {
		fan_speed = settings.fan_max_speed;
		LOGI("\tIncreasing fan speed to %d.\n",
			getpid(), fan_speed);
	}
	else {
		LOGI("\tIncreasing fan speed by %d.\n",
			getpid(), step);
	}
	/* set the fan speed */
	return write_integer(fanctrl_filename, fan_speed);
}

/* Decrease the fan speed by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int decrease_fan_speed(int step)
{
	char filename_buf[MIN_BUF_SIZE];
	char fanctrl_filename[MAX_BUF_SIZE];
	int fan_speed;

	/* format the full file name */
	sprintf(filename_buf, "pwm%d", settings.sysfs_fanctrl_hwmon_subnode);
	sprintf(fanctrl_filename, FAN_CTRL_DIR,
			settings.sysfs_fanctrl_hwmon_node, filename_buf);

	/* read the current fan speed */
	fan_speed = read_integer(fanctrl_filename);

	/* determine the new fan speed */
	fan_speed -= step;
	if (fan_speed < settings.fan_min_speed) {
		fan_speed = settings.fan_min_speed;
		LOGI("\tDecreasing fan speed to %d.\n",
			getpid(), fan_speed);
	}
	else {
		LOGI("\tDecreasing fan speed by %d.\n",
			getpid(), step);
	}
	/* set the fan speed */
	return write_integer(fanctrl_filename, fan_speed);
}

/* Worker function which does the actual throttling.
 * Is intended to be run as a pthread. */
void * worker(void* worker_num) {

	/* buffers for storing file names */
	char fanctrl_filename[MAX_BUF_SIZE];
	char temperature_filename[MAX_BUF_SIZE];

	/* general filename buffer. */
	char filename_buf[MIN_BUF_SIZE];

	int core = *(int*)worker_num;
	int curr_temp = 0, prev_temp = 0;

	/* count the number of intervals spent in hysteresis */
	int intervals_in_hysteresis = 0;

	/* Format the temperature reading file. We add two because the
	 * hwmon files are 1-indexed and the first one is that of the whole die. */
	sprintf(filename_buf, "temp%d_input", core+2);
	sprintf(temperature_filename, CT_HWMON_DIR, settings.sysfs_coretemp_hwmon_node, filename_buf);

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		/* format the full file name */
		sprintf(filename_buf, "pwm%d_enable", settings.sysfs_fanctrl_hwmon_subnode);
		sprintf(fanctrl_filename, FAN_CTRL_DIR,
				settings.sysfs_fanctrl_hwmon_node, filename_buf);
		/* enable manual fan control */
		write_integer(fanctrl_filename, 1);
	}

	/* let's loop forever */
	while (1) {
		/* sleep, then run the commands */
		usleep(settings.polling_interval);

		/* read the current temp of the core */
		curr_temp = read_integer(temperature_filename);
		LOGI("\tCurrent temperature for cpu core %d is %dmC.\n", getpid(), core, curr_temp);
		if (curr_temp == -1) {
			LOGE("\tCould not read cpu core %d temperature.\n", getpid(), core);
			continue;
		}
		else if (!curr_temp) { // initialise prev_temp
			prev_temp = curr_temp;
		}

		/*case 1: temp is in hysteresis range of target */
		if ((curr_temp >= settings.hysteresis_lower_limit)  && (curr_temp <= settings.hysteresis_upper_limit)) {

			/* update the hysteresis counter */
			intervals_in_hysteresis += 1;

			if (intervals_in_hysteresis == settings.hysteresis_reset_threshold) {

				/* reset the hysteresis counter */
				intervals_in_hysteresis = 0;

				/* reset the fan speed */
				if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
					reset_fan_speed();
				}
				/* reset the speed to the settings.cpu_target_temperature */
				increase_max_freq(core, (settings.cpuinfo_max_freq));
			}
		}
		/*case 2: temp is below the (lower) hysteresis range of target */
		else if (curr_temp < settings.hysteresis_lower_limit) {

			/* reset hysteresis counter */
			intervals_in_hysteresis = 0;

			/* decrease the fan speed */
			if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
				decrease_fan_speed((settings.fan_scaling_step/2));
			}
			/* adjust the processor speed by half a step */
			increase_max_freq(core, (settings.cpu_scaling_step/2));
		}
		/*case 3: temp is beyond the (upper) hysteresis range of target */
		else {
			/* reset hysteresis counter */
			intervals_in_hysteresis = 0;

			/* check if the temperature has dropped significantly
			 * since the last time we read the temps .*/
			int temp_difference = prev_temp - curr_temp;

			/* we essentially want to multiply the steps by how many
			 * multiples of the hysteresis deviation we moved since the
			 * last loop iteration */
			int deviations = ceil(((float)temp_difference /
						(float)settings.hysteresis_deviation));

			/* make sure to get the absolute value*/
			deviations = abs(deviations);

			/* if our temperature didn't change, move it a step */
			if (temp_difference == 0) {
				if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
					/* adjust the fan speed by a step */
					increase_fan_speed((settings.fan_scaling_step));
				}
				/* adjust the processor speed by a step */
				decrease_max_freq(core, (settings.cpu_scaling_step));
			}
			/* if our current temp is lower than the previous one */
			else if (temp_difference > 0) {
				/* decrease the fan speed by half a step */
				if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
					increase_fan_speed((settings.fan_scaling_step/2));
				}
				/* increase the processor speed by half a step */
				decrease_max_freq(core, (settings.cpu_scaling_step/2));
			}
			/* if our current temp is worse than the previous one */
			else {
				/* increase the fan speed by deviations steps */
				if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
					increase_fan_speed(settings.fan_scaling_step * deviations);
				}
				/* decrease the processor speed by deviations steps */
				decrease_max_freq(core, settings.cpu_scaling_step * deviations);
			}
		}
		prev_temp = curr_temp;
	}
	return NULL;
}

/* Read the configuration specified by the user.
 *
 * @return: 0 if succesful, -1 otherwise. */
int read_configuration_file() {

	struct stat stat_buf;
	int fd;
	int rc;

	// Do nothing if there's no config file specified.
	if (config_file_path == NULL) {
		return 0;
	}
	/* check if the file exists at that path */
	if (stat(config_file_path, &stat_buf) != -1) {
		// we found the node
		LOGI("\tFound config file at %s.\n",
				getpid(), config_file_path);

		/* open the file */
		if ((fd = open(config_file_path, O_RDWR)) == -1) {
			perror("open");
			LOGE("Failed to open config file %s for writing.\n",
					getpid(), config_file_path);
			return fd;
		}

		/* write the settings to the file */
		rc = read(fd, &settings,
				sizeof(struct throttle_settings));

		if (rc == -1) {
			perror("open");
			LOGE("Failed to open to config file at %s.\n",
					getpid(), config_file_path);
			return rc;
		}
		close(fd);
		return 0;
	}

	return 0;
}

/* Read sysfs interfaces and populate the
 * throttle_settings buffer with basic defaults. */
void initialise_settings(void) {

	struct stat stat_buf;
	char filename[MAX_BUF_SIZE];

	int i = 0, max_tries = 10;

	/* initialise global pointers to NULL */
	log_file = NULL;
	config_file_path = NULL;

	// initialise the hwmon global variables
	settings.sysfs_coretemp_hwmon_node = -1;
	settings.sysfs_fanctrl_hwmon_node = -1;
	settings.sysfs_fanctrl_hwmon_subnode = -1;

	/* find the hwmon nodes for the core control
	 *  and fan control */
	for (i = 0; i < max_tries; i++) {
		// format the filename
		sprintf(filename, CT_HWMON_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			settings.sysfs_coretemp_hwmon_node = i;
		}

		// format the filename
		sprintf(filename, FAN_CTRL_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			settings.sysfs_fanctrl_hwmon_node = i;
		}
	}
	/* find out the naming convention for the pwm files */
	if (settings.sysfs_fanctrl_hwmon_node != -1) {
		for (i = 0; i < max_tries; i++) {
			char fanctrl_file[MIN_BUF_SIZE];

			// format the filename
			sprintf(fanctrl_file, "pwm%d_enable", i);
			sprintf(filename, FAN_CTRL_DIR, i, fanctrl_file);

			// stat the file
			if (stat(filename, &stat_buf) != -1) {
				// set the fan control global variable.
				settings.sysfs_fanctrl_hwmon_subnode = i;
			}
		}
	}

	/* set the fan speeds */
	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		char general_buf[MIN_BUF_SIZE];
		sprintf(general_buf, "fan%d_speed_max", settings.sysfs_fanctrl_hwmon_subnode);
		sprintf(filename, FAN_CTRL_DIR, settings.sysfs_fanctrl_hwmon_node, general_buf);
		settings.fan_max_speed = read_integer(filename);
		sprintf(general_buf, "fan%d_min", settings.sysfs_fanctrl_hwmon_subnode);
		sprintf(filename, FAN_CTRL_DIR, settings.sysfs_fanctrl_hwmon_node, general_buf);
		settings.fan_min_speed = read_integer(filename);
	}

	/* get the cpuinfo scaling limits */
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_min_freq");
	settings.cpuinfo_min_freq = read_integer(filename);
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_max_freq");
	settings.cpuinfo_max_freq = read_integer(filename);

	// initialise to -1. We will set this later.
	settings.fan_target_speed = -1;

	/* set the default target freqency to the maximum */
	settings.cpu_target_freq = settings.cpuinfo_max_freq;

	/* set sane defaults for everything */
	settings.polling_interval = MS_TO_US(500);
	settings.cpu_scaling_step = MHZ_TO_KHZ(100);
	settings.cpu_target_temperature = C_TO_MC(55);
	settings.hysteresis_range = 0.1;
	settings.hysteresis_reset_threshold = 100;
	settings.fan_scaling_step = 10;
	settings.cpu_ht_available = 0;
	settings.num_cores = 1;

	/* disable logging by default */
	settings.logging_enabled = 0;
}

/* Write to the configuration specified by the user.
 *
 * @return: 0 if succesful, -1 otherwise. */
int write_configuration_file()
{
	int fd;
	int rc;

	// Do nothing if no config was specified.
	if (config_file_path == NULL) {
		return 0;
	}

	/* open the file */
	if ((fd = open(config_file_path, O_RDWR)) == -1) {
		perror("open");
		LOGE("Failed to open config file %s for writing.\n",
				getpid(), config_file_path);
		return fd;
	}

	/* write the settings to the file */
	rc = write(fd, &settings,
			sizeof(struct throttle_settings));

	if (rc == -1) {
		perror("write");
		LOGE("Failed to write to config file at %s.\n",
				getpid(), config_file_path);
		return rc;
	}

	close(fd);
	return 0;
}

/* Helper function to parse command line arguments from main */
void parse_commmand_line(int argc, char *argv[]) {

	//variable to store return status of getopt().
	int opt;
	int optind = 0;

	static struct option long_options[] = {
		/* *name ,  has_arg,           *flag,  val */
		{"interval",	required_argument,       0, 'i' },
		{"freq",	required_argument,       0, 'f' },
		{"step",	required_argument,       0, 's' },
		{"temp",	required_argument,       0, 't' },
		{"log",	required_argument,       0, 'l' },
		{"hysteresis",	required_argument,       0, 'r' },
		{"reset-threshold",	required_argument,       0, 'u' },
		{"fan-rest-speed",	required_argument,       0, 'e' },
		{"config",	required_argument,       0, 'o' },
		{"threading",	no_argument,       0, 'v' },
		{"cores",	no_argument,       0, 'c' },
		{"help",	no_argument,       0, 'h' },
		{0,         0,                 0,  0 }
	};

	/* read in the command line args if anything was passed */
	while ( (opt = getopt_long (argc, argv, "i:f:s:c:t:l:r:e:u:hv", long_options, &optind)) != -1 ) {
		switch (opt) {
		    case 'o':
			config_file_path = malloc(MAX_BUF_SIZE);
			strncpy(config_file_path, optarg, MAX_BUF_SIZE);
			break;
		    case 'u':
			settings.hysteresis_reset_threshold=atoi(optarg);
			break;
		    case 'e':
			settings.fan_target_speed=atoi(optarg);
			break;
		    case 'r':
			settings.hysteresis_range=(float) (strtol(optarg, NULL, 10) / 100.0);
			break;
		    case 'i':
			settings.polling_interval=MS_TO_US(atoi(optarg));
			break;
		    case 'f':
			settings.cpu_target_freq=MHZ_TO_KHZ(atoi(optarg));
			break;
		    case 's':
			settings.cpu_scaling_step=MHZ_TO_KHZ(atoi(optarg));
			break;
		    case 't':
			settings.cpu_target_temperature=C_TO_MC(atoi(optarg));
			break;
		    case 'l':
			strncpy(settings.log_path, optarg, MAX_BUF_SIZE);
			settings.logging_enabled = 1;
			break;
		    case 'v':
			settings.cpu_ht_available = 1;
			break;
		    case 'c':
			settings.num_cores = atoi(optarg);
			break;
		    case 'h':
		    case '?': // case in which the argument is not recognised.
		    default: 
			fprintf (stderr, "Usage: %s [OPTION]\n",argv[0]);
			fprintf (stderr, "\nOptional commands:\n");
			fprintf (stderr, "  -i, --interval\ttime to wait before scaling again, in ms.\n" );
			fprintf (stderr, "  -f, --freq\tfrequency to limit to, in MHz.\n" );
			fprintf (stderr, "  -s, --step\tscaling step, in MHz\n" );
			fprintf (stderr, "  -t, --temp\tTarget temperature, in degrees.\n" );
			fprintf (stderr, "  -v, --threading\tWhether threading is available or not. \n" );
			fprintf (stderr, "  -e, --fan-rest-speed\t Speed to set fan to in hysteresis.\n");
			fprintf (stderr, "  -r, --hysteresis\tHysteresis range (< 51, in percent) as an integer.\n");
			fprintf (stderr, "  -u, --reset-threshold\tNumber of intervals spent consecutively in \n"
					"hysteresis before fan speed and cpu clock are reset.\n");
			fprintf (stderr, "  -o, --config\tPath to read/write binary config.\n" );
			fprintf (stderr, "  -c, --cores\tnumber of (physical) cores on the system.\n" );
			fprintf (stderr, "  -l, --log\tPath to log file.\n" );
			fprintf (stderr, "  -h, --help\tprint this message.\n");
			exit (EXIT_FAILURE);
		}
	}
}

/* Signal handler function to handle termination
 * signals and reset hardware settings to original. */
void handler(int signal) {

	char filename_buf[MIN_BUF_SIZE];
	char fanctrl_filename[MAX_BUF_SIZE];
	int i;

	LOGI("Termination signal sent. Winding up...\n", getpid());

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		/* format the full file name */
		sprintf(filename_buf, "pwm%d_enable", settings.sysfs_fanctrl_hwmon_subnode);
		sprintf(fanctrl_filename, FAN_CTRL_DIR,
				settings.sysfs_fanctrl_hwmon_node, filename_buf);
		/* disable manual fan control */
		LOGI("Enabling automatic fan control...\n", getpid());
		write_integer(fanctrl_filename, 0);
	}

	/* write to configuration file if one was passed */
	LOGI("Saving configuration...\n", getpid());
	write_configuration_file();

	/* reset the cpu maximum frequency */
	for (i = 0; i < settings.num_cores; i++) {

		/* don't scale the virtual cores */
		if ((i % 2) && (settings.cpu_ht_available)) continue;

		LOGI("Resetting cpu%d maximum frequency...\n", getpid(), i);
		reset_max_freq(i);
	}
	/* exit */
	exit(EXIT_SUCCESS);
}

