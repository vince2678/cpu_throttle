/**
* cpu_throttle.c
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

#include "cpu_throttle.h"

int main(int argc, char *argv[])
{
	/* parse the command line */
	parse_commmand_line(argc, argv);

	/*define the pthreads array */
	pthread_t pthreads_arr[settings.num_cores];

	/* set the global pointer */
	threads = pthreads_arr;

	/* initialise the sigaction struct */
	struct sigaction sa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/* set up signal handlers */
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("sigaction");
	}

	/* open the log file */
	if (settings.logging_enabled == 1) {
		if (!(log_file = fopen(settings.log_path, "a+"))) {
			perror("fopen");
			fprintf(stderr, "Could not open log file\n");
			exit(EXIT_FAILURE);
		}
	}

	LOGI("Firing up...\n", getpid());
	LOGI("Setting throttling parameters...\n", getpid());
	// initialise the hwmon global variables
	settings.sysfs_coretemp_hwmon_node = -1;
	settings.sysfs_fanctrl_hwmon_node = -1;
	settings.sysfs_fanctrl_hwmon_subnode = -1;

	/* loop variables */
	// buffer to store stats on the dir
	struct stat stat_buf;
	char filename[MAX_BUF_SIZE];

	int i = 0, max_tries = 10;

	/* find the hwmon node for the core control */
	for (i = 0; i < max_tries; i++) {
		// format the filename
		sprintf(filename, CT_HWMON_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			settings.sysfs_coretemp_hwmon_node = i;
			LOGI("\tFound cpufreq hwmon node at %d.\n", getpid(), i);
		}

		// format the filename
		sprintf(filename, FAN_CTRL_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			settings.sysfs_fanctrl_hwmon_node = i;
			LOGI("\tFound fan-control hwmon node at %d.\n", getpid(), i);
		}
	}
	if (settings.sysfs_coretemp_hwmon_node == -1) {
		LOGE("\tCould not find core temp hwmon directory.\n", getpid());
		exit(EXIT_FAILURE);
	}
	if (settings.sysfs_fanctrl_hwmon_node == -1) {
		LOGW("\tCould not find fan control hwmon directory. Working without it.\n", getpid());
	}
	else {
		for (i = 0; i < max_tries; i++) {
			char fanctrl_file[MIN_BUF_SIZE];

			// format the filename
			sprintf(fanctrl_file, "pwm%d_enable", i);
			sprintf(filename, FAN_CTRL_DIR, i, fanctrl_file);

			// stat the file
			if (stat(filename, &stat_buf) != -1) {
				// we found the node
				LOGI("\tFound fan-control sysfs interface at node %d.\n", getpid(), i);
				// set the fan control global variable.
				settings.sysfs_fanctrl_hwmon_subnode = i;
			}
		}
	}
	/* calculate the hysteresis range */
	settings.hysteresis_deviation = (((float) settings.cpu_target_temperature) * settings.hysteresis_range);
	settings.hysteresis_upper_limit = settings.cpu_target_temperature + settings.hysteresis_deviation;
	settings.hysteresis_lower_limit = settings.cpu_target_temperature - settings.hysteresis_deviation;

	/* get the cpuinfo scaling limits */
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_min_freq");
	settings.cpuinfo_min_freq = read_integer(filename);
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_max_freq");
	settings.cpuinfo_max_freq = read_integer(filename);

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

	if ((settings.cpuinfo_min_freq == -1) || (settings.cpuinfo_min_freq == -1)) {
		LOGE("\tCould not read cpu scaling limits.\n", getpid());
		exit(EXIT_FAILURE);
	}
	else {
		LOGI("\tSuccessfully read cpu scaling limits.\n"
			"\t\t\tmax_freq:%dMHz min_freq:%dMHz\n", getpid(),
				KHZ_TO_MHZ(settings.cpuinfo_max_freq), KHZ_TO_MHZ(settings.cpuinfo_min_freq));
	}

	// make sure an illegal target frequency wasn't specified.
	if (settings.cpu_target_freq > settings.cpuinfo_max_freq ) settings.cpu_target_freq = settings.cpuinfo_max_freq;

	/* print some information about the values we set */
	LOGI("\tSet polling interval to %dms.\n", getpid(), US_TO_MS(settings.polling_interval));
	LOGI("\tSet scaling target freq to %dMHz.\n", getpid(), KHZ_TO_MHZ(settings.cpu_target_freq));
	LOGI("\tSet cpu scaling step to %dMHz.\n", getpid(), KHZ_TO_MHZ(settings.cpu_scaling_step));
	LOGI("\tSet cpu target temperature to %dmC.\n", getpid(), settings.cpu_target_temperature);
	LOGI("\tSet cpu hysteresis range to %d percent.\n", getpid(), (int)(settings.hysteresis_range*100));
	LOGI("\tSet hysteresis threshold to %d intervals.\n", getpid(), settings.hysteresis_reset_threshold);
	LOGI("\tCalculated temperature hysteresis deviation of %dmC.\n", getpid(), settings.hysteresis_deviation);
	LOGI("\tCalculated temperature hysteresis lower limit of %dmC.\n", getpid(), settings.hysteresis_lower_limit);
	LOGI("\tCalculated temperature hysteresis upper limit of %dmC.\n", getpid(), settings.hysteresis_upper_limit);
	LOGI("\tSet cpus to throttle to %d.\n", getpid(), settings.num_cores);

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		// make sure an illegal target frequency wasn't specified.
		if (settings.fan_target_speed > settings.fan_max_speed ) settings.fan_target_speed = settings.fan_max_speed;
		else if (settings.fan_target_speed < settings.fan_min_speed ) settings.fan_target_speed = settings.fan_min_speed;

		LOGI("\tSet fan scaling step to %d.\n", getpid(), settings.fan_scaling_step);
		LOGI("\tSet fan rest speed to %d.\n", getpid(), settings.fan_target_speed);
		LOGI("\tSuccessfully read fan speed limits.\n"
			"\t\tspeed_max: %d\t speed_min: %d\n", getpid(),
				settings.fan_max_speed, settings.fan_min_speed);
	}
	LOGI("Done reading/setting throttling parameters.\n", getpid());

	/* start the scaling/throttling threads */
	LOGI("Starting throttling threads...\n", getpid());
	for (i = 0; i < settings.num_cores; i++) {
		/* don't scale the virtual cores */
		if ((i % 2) && (settings.cpu_ht_available)) continue;

		LOGI("Starting thread for cpu%d...\n", getpid(), i);

		int rc = pthread_create(&(threads[i]), NULL, worker, &i);
		if (rc) {
			LOGE("Failed to start thread for cpu%d...\n", getpid(), i);
			exit(EXIT_FAILURE);
		}
	}
	/* reset the cpu maximum frequency */
	for (i = 0; i < settings.num_cores; i++) {
		/* don't scale the virtual cores */
		if ((i % 2) && (settings.cpu_ht_available)) continue;

		LOGI("Waiting for cpu%d thread to finish...\n", getpid(), i);
		int rc = pthread_join(threads[i], NULL);
		if (rc) {
			LOGE("Failed to join thread for cpu%d...\n", getpid(), i);
		}

		LOGI("Resetting cpu%d maximum frequency...\n", getpid(), i);
		reset_max_freq(i);
	}
}
