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

/* Reset the maximum frequency on cpu core to the
 * maximum defined in sysfs
 *
 * @return: 0 if succesful, -1 otherwise. */
int reset_max_freq(int core)
{
	char filename[MAX_BUF_SIZE];

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	if (settings.verbose) {
		LOGI("\t[cpu%d] Resetting speed ceiling to %d.\n",
				getpid(), core, settings.cpuinfo_max_freq);
	}

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

		/* log a message */
		if (settings.verbose) {
			LOGI("\t[cpu%d] Decreasing speed ceiling to %dMHz.\n",
				getpid(), core, KHZ_TO_MHZ(freq));
		}
	}
	else if (settings.verbose) {
		LOGI("\t[cpu%d] Decreasing speed ceiling by %dMHz.\n",
			getpid(), core, KHZ_TO_MHZ(step));
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
	if (freq > settings.cpu_max_freq) {
		freq = settings.cpu_max_freq;

		/* log a message */
		if (settings.verbose) {
			LOGI("\t[cpu%d] Increasing speed ceiling to %dMHz.\n",
				getpid(), core, KHZ_TO_MHZ(freq));
		}
	}
	else if (settings.verbose) {
		LOGI("\t[cpu%d] Increasing speed ceiling by %dMHz.\n",
			getpid(), core, KHZ_TO_MHZ(step));
	}

	/* write the string to the file and return */
	return write_integer(filename, freq);
}

/* Reset the fan speed to the
 * (user defined) minimum fan speed.
 *
 * @return: 0 if succesful, -1 otherwise. */
int reset_fan_speed(void)
{
	char file_path[MAX_BUF_SIZE];
	char filename[MIN_BUF_SIZE];

	/* format the full file name */
	sprintf(filename, "pwm%d",
			settings.sysfs_fanctrl_hwmon_subnode);

	sprintf(file_path, FAN_CTRL_DIR,
			settings.sysfs_fanctrl_hwmon_node, filename);

	if (settings.verbose) {
		LOGI("\t[fan] Resetting fan speed to %d.\n", getpid(),
				settings.fan_min_speed);
	}

	/* set the fan speed */
	return write_integer(file_path, settings.fan_min_speed);
}

/* Increase the fan speed by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int increase_fan_speed(int step)
{
	char file_path[MAX_BUF_SIZE];
	char filename[MIN_BUF_SIZE];
	int fan_speed;

	/* format the full file name */
	sprintf(filename, "pwm%d",
			settings.sysfs_fanctrl_hwmon_subnode);

	sprintf(file_path, FAN_CTRL_DIR,
			settings.sysfs_fanctrl_hwmon_node, filename);

	/* read the current fan speed */
	fan_speed = read_integer(file_path);

	/* determine the new fan speed */
	fan_speed += step;
	if (fan_speed > settings.fan_hw_max_speed) {
		fan_speed = settings.fan_hw_max_speed;

		if (settings.verbose) {
			LOGI("\t[fan] Increasing fan speed to %d.\n",
				getpid(), fan_speed);
		}
	}
	else if (settings.verbose) {
		LOGI("\t[fan] Increasing fan speed by %d.\n",
			getpid(), step);
	}
	/* set the fan speed */
	return write_integer(file_path, fan_speed);
}

/* Decrease the fan speed by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int decrease_fan_speed(int step)
{
	char file_path[MAX_BUF_SIZE];
	char filename[MIN_BUF_SIZE];
	int fan_speed;

	/* format the full file name */
	sprintf(filename, "pwm%d", settings.sysfs_fanctrl_hwmon_subnode);
	sprintf(file_path, FAN_CTRL_DIR,
			settings.sysfs_fanctrl_hwmon_node, filename);

	/* read the current fan speed */
	fan_speed = read_integer(file_path);

	/* determine the new fan speed */
	fan_speed -= step;
	if (fan_speed < settings.fan_min_speed) {

		fan_speed = settings.fan_min_speed;

		if (settings.verbose) {
			LOGI("\t[fan] Decreasing fan speed to %d.\n",
				getpid(), fan_speed);
		}
	}
	else if (settings.verbose) {
		LOGI("\t[fan] Decreasing fan speed by %d.\n",
			getpid(), step);
	}
	/* set the fan speed */
	return write_integer(file_path, fan_speed);
}

/* Worker function which does the actual throttling.
 * Is intended to be run as a pthread. */
void * cpu_worker(void* worker_num) {

	/* buffers for storing file names/paths */
	char temperature_file_path[MAX_BUF_SIZE];
	char filename[MIN_BUF_SIZE];

	int core = *(int*)worker_num;
	int curr_temp = 0, prev_temp = 0;

	/* count the number of intervals spent in hysteresis */
	int intervals_in_hysteresis = 0;

	/* Format the temperature reading file. We add two because the
	 * hwmon files are 1-indexed and the first one is that of the whole die. */
	sprintf(filename, "temp%d_input", core+2);
	sprintf(temperature_file_path, CT_HWMON_DIR,
			settings.sysfs_coretemp_hwmon_node, filename);

	/* let's loop forever */
	while (1) {
		/* sleep, then run the commands */
		usleep(settings.polling_interval);

		/* read the current temp of the core */
		curr_temp = read_integer(temperature_file_path);

		if (settings.verbose) {
			LOGI("\t[cpu%d] Current temperature is %dC.\n",
					getpid(), core, MC_TO_C(curr_temp));
		}

		if (curr_temp == -1) {
			if (settings.verbose) {
					LOGE("\t[cpu%d] Could not read "
						"cpu temperature.\n",
							getpid(), core);
			}
			continue;
		}

		/*case 1: temp is in hysteresis range of target */
		if ((curr_temp >= settings.hysteresis_lower_limit)
				&& (curr_temp <= settings.hysteresis_upper_limit)) {

			/*subcase 1: If temp is between lower and target temp */
			if (curr_temp <= settings.cpu_target_temperature) {
				/* adjust the processor speed by a quarter step */
				increase_max_freq(core, ceil((float)settings.cpu_scaling_step/4.0));
			}
			/*subcase 2: If temp is between target temp and upper range */
			else {
				/* update the hysteresis counter */
				intervals_in_hysteresis += 1;

				/* check if we've reached the reset threshold */
				if (intervals_in_hysteresis == settings.hysteresis_reset_threshold) {

					/* reset the hysteresis counter */
					intervals_in_hysteresis = 0;

					/* reset the speed to the settings.cpu_max_freq */
					increase_max_freq(core, settings.cpuinfo_max_freq);
				}
			}
		}
		/*case 2: temp is below the (lower) hysteresis range of target */
		else if (curr_temp < settings.hysteresis_lower_limit) {

			/* reset hysteresis counter */
			intervals_in_hysteresis = 0;

			/* increase the processor speed by half a step */
			increase_max_freq(core, ceil((float)settings.cpu_scaling_step/2.0));
		}
		/*case 3: temp is beyond the (upper) hysteresis range of target */
		else {
			/* reset hysteresis counter */
			intervals_in_hysteresis = 0;

			/* check if the temperature has dropped significantly
			 * since the last time we read the temps .*/
			int temp_difference = prev_temp - curr_temp;

			/* if our temperature didn't change, decrease the core max frequency */
			if (temp_difference == 0) {
				/* adjust the processor speed by a quarter step */
				decrease_max_freq(core, ceil((float)settings.cpu_scaling_step/4.0));
			}
			/* if our current temp is lower than the previous one */
			else if (temp_difference > 0) {
				/* decrease the processor speed by half a step */
				decrease_max_freq(core, ceil((float)settings.cpu_scaling_step/2.0));
			}
			/* if our current temp is worse than the previous one */
			else {
				/* decrease the processor speed by a step */
				decrease_max_freq(core, settings.cpu_scaling_step);
			}
			prev_temp = curr_temp;
		}
	}
	return NULL;
}

/* Worker function which does the actual throttling.
 * Is intended to be run as a pthread. */
void * fan_worker(void* worker_num) {

	/* buffers for storing file names */
	char fanctrl_file_path[MAX_BUF_SIZE];
	char temperature_file_path[MAX_BUF_SIZE];

	/* general filename buffer. */
	char filename_buf[MIN_BUF_SIZE];

	int curr_temp = 0, prev_temp = 0;

	/* Format the temperature reading file. We add two because the
	 * hwmon files are 1-indexed and the first one is that of the whole die. */
	sprintf(temperature_file_path, CT_HWMON_DIR,
			settings.sysfs_coretemp_hwmon_node, "temp1_input");

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		/* format the full file name */
		sprintf(filename_buf, "pwm%d_enable",
				settings.sysfs_fanctrl_hwmon_subnode);

		sprintf(fanctrl_file_path, FAN_CTRL_DIR,
				settings.sysfs_fanctrl_hwmon_node, filename_buf);
		/* enable manual fan control */
		write_integer(fanctrl_file_path, 1);
	}
	else { /* return NULL if theres no fan control. */
		LOGW("\tNo fan control interface detetected. "
			"Disabling fan control worker.\n", getpid());
		return NULL;
	}

	/* let's loop forever */
	while (1) {
		/* sleep, then run the commands */
		usleep(settings.polling_interval);

		/* read the current temp of the core */
		curr_temp = read_integer(temperature_file_path);

		if (curr_temp == -1) {
			if (settings.verbose) {
				LOGE("\tCould not read cpu die temperature.\n",
						getpid());
			}
			continue;
		}

		/*case 1: temp is in hysteresis range of target */
		if ((curr_temp >= settings.hysteresis_lower_limit)
				&& (curr_temp <= settings.hysteresis_upper_limit)) {

			/*subcase 1: If temp is between lower and target temp */
			if (curr_temp <= settings.cpu_target_temperature) {
				/* decrease the fan speed by a quarter step */
				decrease_fan_speed(ceil((float)settings.fan_scaling_step/4.0));
			}
			/*subcase 2: If temp is between target temp and upper range */
			else {
				/* increase the fan speed by a quarter step */
				increase_fan_speed(ceil((float)settings.fan_scaling_step/4.0));
			}
		}
		/*case 2: temp is below the (lower) hysteresis range of target */
		else if (curr_temp < settings.hysteresis_lower_limit) {
			/* decrease the fan speed by half a step */
			decrease_fan_speed(ceil((float)settings.fan_scaling_step/2.0));
		}
		/*case 3: temp is beyond the (upper) hysteresis range of target */
		else {
			/* check if the temperature has dropped significantly
			 * since the last time we read the temps .*/
			int temp_difference = prev_temp - curr_temp;

			/* if our temperature didn't change, move it a step */
			if (temp_difference == 0) {
				/* inrease the fan speed by a quarter step */
				increase_fan_speed(ceil((float)settings.fan_scaling_step/4.0));
			}
			/* if our current temp is lower than the previous one */
			else if (temp_difference > 0) {
				/* increase the fan speed by half a step */
				increase_fan_speed(ceil((float)settings.fan_scaling_step/2.0));
			}
			/* if our current temp is worse than the previous one */
			else {
				/* increase the fan speed by a step */
				increase_fan_speed(settings.fan_scaling_step);
			}
			prev_temp = curr_temp;
		}
	}
	return NULL;
}

/* Read the configuration specified by the user.
 *
 * @return: 0 if succesful, -1 otherwise. */
int read_configuration_file(void) {

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
		if ((fd = open(config_file_path, O_RDONLY)) == -1) {
			perror("open");
			LOGE("Failed to open config file %s for reading.\n",
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

	/* initialise global variables */
	log_file = NULL;
	config_file_path = NULL;
	write_config = 0;

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
			sprintf(filename, FAN_CTRL_DIR,
				settings.sysfs_fanctrl_hwmon_node,
				fanctrl_file);

			// stat the file
			if (stat(filename, &stat_buf) != -1) {
				// set the fan control global variable.
				settings.sysfs_fanctrl_hwmon_subnode = i;
			}
		}
	}

	/* read and set the fan speeds */
	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		char general_buf[MIN_BUF_SIZE];

		sprintf(general_buf, "fan%d_speed_max",
				settings.sysfs_fanctrl_hwmon_subnode);
		sprintf(filename, FAN_CTRL_DIR,
				settings.sysfs_fanctrl_hwmon_node, general_buf);
		settings.fan_hw_max_speed = read_integer(filename);

		sprintf(general_buf, "fan%d_min",
				settings.sysfs_fanctrl_hwmon_subnode);
		sprintf(filename, FAN_CTRL_DIR,
				settings.sysfs_fanctrl_hwmon_node, general_buf);
		settings.fan_hw_min_speed = read_integer(filename);
	}

	/* get the cpuinfo scaling limits */
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_min_freq");
	settings.cpuinfo_min_freq = read_integer(filename);
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_max_freq");
	settings.cpuinfo_max_freq = read_integer(filename);

	// initialise to -1. We will set this later.
	settings.fan_min_speed = -1;

	/* set the default target freqency to the maximum */
	settings.cpu_max_freq = settings.cpuinfo_max_freq;

	/* set sane defaults for everything */
	settings.polling_interval = MS_TO_US(500);
	settings.cpu_scaling_step = MHZ_TO_KHZ(100);
	settings.cpu_target_temperature = C_TO_MC(55);
	settings.hysteresis = C_TO_MC(6);
	settings.hysteresis_reset_threshold = 100;
	settings.fan_scaling_step = 2;
	settings.num_cores = 1;

	/* disable logging by default */
	settings.verbose = 0;
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
	if ((fd = open(config_file_path, O_CREAT|O_TRUNC|O_WRONLY,
				S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1) {
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
		/* *name ,  has_arg,		   *flag,  val */
		{"interval",	required_argument,	   0, 'i' },
		{"max-freq",	required_argument,	   0, 'f' },
		{"cpu-step",	required_argument,	   0, 's' },
		{"fan-step",	required_argument,	   0, 'a' },
		{"temp",	required_argument,	   0, 't' },
		{"log",	required_argument,	   0, 'l' },
		{"hysteresis",	required_argument,	   0, 'r' },
		{"reset-threshold",	required_argument,	   0, 'u' },
		{"minimum-fan-speed",	required_argument,	   0, 'e' },
		{"config",	required_argument,	   0, 'o' },
		{"cores",	required_argument,	   0, 'c' },
		{"write-config",	no_argument,	   0, 'w' },
		{"help",	no_argument,	   0, 'h' },
		{"verbose",	no_argument,	   0, 'v' },
		{0,		 0,				 0,  0 }
	};

	/* read in the command line args if anything was passed */
	while ( (opt = getopt_long(argc, argv, "i:f:s:a:c:t:l:o:r:e:u:hvw",
					long_options, &optind)) != -1 ) {
		switch (opt) {
			case 'o':
				config_file_path = malloc(MAX_BUF_SIZE);
				strncpy(config_file_path, optarg, MAX_BUF_SIZE);
				break;
			case 'w':
				write_config=1;
				break;
			case 'u':
				settings.hysteresis_reset_threshold=atoi(optarg);
				break;
			case 'a':
				settings.fan_scaling_step=atoi(optarg);
				break;
			case 'e':
				settings.fan_min_speed=atoi(optarg);
				break;
			case 'r':
				settings.hysteresis=C_TO_MC(atoi(optarg));
				break;
			case 'i':
				settings.polling_interval=MS_TO_US(atoi(optarg));
				break;
			case 'f':
				settings.cpu_max_freq=MHZ_TO_KHZ(atoi(optarg));
				break;
			case 's':
				settings.cpu_scaling_step=MHZ_TO_KHZ(atoi(optarg));
				break;
			case 't':
				settings.cpu_target_temperature=C_TO_MC(atoi(optarg));
				break;
			case 'v':
				settings.verbose = 1;
				break;
			case 'l':
				strncpy(settings.log_path, optarg, MAX_BUF_SIZE);
				settings.logging_enabled = 1;
				break;
			case 'c':
				settings.num_cores = atoi(optarg);
				break;
			case 'h':
			case '?': // case in which the argument is not recognised.
			default:
				fprintf (stderr, "Usage: %s [OPTION]\n",argv[0]);
				fprintf (stderr, "\nOptional commands:\n");
				fprintf (stderr, "  -i, --interval\t Time to wait before scaling again, in ms.\n" );
				fprintf (stderr, "  -f, --max-freq\t\t Maximum frequency cpus can attain, in MHz.\n" );
				fprintf (stderr, "  -s, --cpu-step\t Scaling step, in MHz\n" );
				fprintf (stderr, "  -a, --fan-step\t Fan scaling step.\n");
				fprintf (stderr, "  -t, --temp\t\t Target temperature, in degrees.\n" );
				fprintf (stderr, "  -e, --minimum-fan-speed\t Minimum speed fan can reach.\n");
				fprintf (stderr, "  -r, --hysteresis\t Hysteresis deviation range in degrees.\n");
				fprintf (stderr, "  -u, --reset-threshold\t Number of intervals spent consecutively\n"
						"\t\t\t in hysteresis before fan speed and cpu clock are reset.\n");
				fprintf (stderr, "  -o, --config\t\t Path to read/write binary config.\n" );
				fprintf (stderr, "  -w, --write-config\t\t Just save the new configuration and exit.\n" );
				fprintf (stderr, "  -c, --cores\t\t Number of (physical) cores on the system.\n" );
				fprintf (stderr, "  -l, --log\t\t Path to log file.\n" );
				fprintf (stderr, "  -v, --verbose\t\t Print detailed throttling information.\n");
				fprintf (stderr, "  -h, --help\t\t Print this message.\n");
				exit (EXIT_FAILURE);
		}
	}
}

/* Verifies taht settings input are valid */
void validate_settings(void) {

	/* calculate the hysteresis range */
	settings.hysteresis_upper_limit =
		settings.cpu_target_temperature + settings.hysteresis;
	settings.hysteresis_lower_limit =
		settings.cpu_target_temperature - settings.hysteresis;

	// make sure an illegal target frequency wasn't specified.
	if (settings.cpu_max_freq > settings.cpuinfo_max_freq ) {
		settings.cpu_max_freq = settings.cpuinfo_max_freq;
	}

	/* set the target speed to the hardware minimum if
	 * not already set. */
	if (settings.fan_min_speed == -1) {
		settings.fan_min_speed = settings.fan_hw_min_speed;
	}

	/* check if the sysfs core temperature node exist */
	if (settings.sysfs_coretemp_hwmon_node == -1) {
		LOGE("\tCould not find core temp hwmon directory.\n", getpid());
		exit(EXIT_FAILURE);
	}

	/* check if we read the cpu scaling limits properly */
	if ((settings.cpuinfo_min_freq == -1) || (settings.cpuinfo_min_freq == -1)) {
		LOGE("\tCould not read cpu scaling limits.\n", getpid());
		exit(EXIT_FAILURE);
	}

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		// make sure an illegal target fan speed wasn't specified.
		if (settings.fan_min_speed > settings.fan_hw_max_speed) {
			settings.fan_min_speed = settings.fan_hw_max_speed;
		}
		else if (settings.fan_min_speed < settings.fan_hw_min_speed) {
			settings.fan_min_speed = settings.fan_hw_min_speed;
		}
	}
}

/* Signal handler function to handle termination
 * signals and reset hardware settings to original. */
void handler(int signal) {

	char filename[MIN_BUF_SIZE];
	int i;

	if ((signal == SIGTERM) || (signal == SIGINT)) {
		LOGI("Termination signal sent. Winding up...\n", getpid());

		if (settings.sysfs_fanctrl_hwmon_subnode != -1) {

			char fanctrl_file_path[MAX_BUF_SIZE];

			/* format the full file name */
			sprintf(filename, "pwm%d_enable",
					settings.sysfs_fanctrl_hwmon_subnode);

			sprintf(fanctrl_file_path, FAN_CTRL_DIR,
					settings.sysfs_fanctrl_hwmon_node, filename);

			/* disable manual fan control */
			LOGI("[fan] Enabling automatic fan control...\n", getpid());
			write_integer(fanctrl_file_path, 0);
		}

		/* reset the cpu maximum frequency */
		for (i = 0; i < settings.num_cores; i++) {

			LOGI("[cpu%d] Resetting maximum frequency...\n",
					getpid(), i);

			reset_max_freq(i);
		}
		exit(EXIT_SUCCESS);
	}
	else if (signal == SIGHUP) {
		LOGI("Reloading configuration...\n", getpid());

		/* read a configuration file if one was passed */
		read_configuration_file();

		/* validate the settings read */
		validate_settings();
	}
}

