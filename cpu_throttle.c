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
	/* initialise the sigaction struct */
	struct sigaction sa;

	/* Read sysfs interfaces and populate the throttle_settings
	 * buffer with basic defaults. */
	initialise_settings();

	/* parse the command line */
	parse_commmand_line(argc, argv);

	/* read a configuration file if one was passed */
	read_configuration_file();

	/* initialise the sigaction struct */
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/*define the pthreads array */
	pthread_t pthreads_arr[settings.num_cores];

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
	else {
		// print statements to stderr
		log_file = stderr;
	}

	LOGI("Firing up...\n", getpid());

	/* check if the sysfs core temperature node exist */
	if (settings.sysfs_coretemp_hwmon_node == -1) {
		LOGE("\tCould not find core temp hwmon directory.\n", getpid());
		exit(EXIT_FAILURE);
	}
	else {
		LOGI("\tFound cpu core temp hwmon node at %d.\n",
				getpid(), settings.sysfs_coretemp_hwmon_node);
	}

	/* check if the sysfs fan control node exist */
	if (settings.sysfs_fanctrl_hwmon_subnode == -1) {
		LOGW("\tCould not find fan control hwmon directory."
				" Working without it.\n", getpid());
	}
	else {
		LOGI("\tFound fan-control sysfs interface at node %d.\n",
				getpid(), settings.sysfs_fanctrl_hwmon_subnode);
	}

	/* set the target speed to the hardware minimum if
	 * not already set. */
	if (settings.fan_min_speed == -1) {
		settings.fan_min_speed = settings.fan_hw_min_speed;
	}

	/* calculate the hysteresis range */
	settings.hysteresis_deviation =
		(((float) settings.cpu_target_temperature) * settings.hysteresis_range);
	settings.hysteresis_upper_limit =
		settings.cpu_target_temperature + settings.hysteresis_deviation;
	settings.hysteresis_lower_limit =
		settings.cpu_target_temperature - settings.hysteresis_deviation;

	/* check if we read the cpu scaling limits properly */
	if ((settings.cpuinfo_min_freq == -1) || (settings.cpuinfo_min_freq == -1)) {
		LOGE("\tCould not read cpu scaling limits.\n", getpid());
		exit(EXIT_FAILURE);
	}
	else {
		LOGI("\tSuccessfully read cpu scaling limits.\n"
			"\t\t\tmax_freq:%dMHz min_freq:%dMHz\n", getpid(),
				KHZ_TO_MHZ(settings.cpuinfo_max_freq),
				KHZ_TO_MHZ(settings.cpuinfo_min_freq));
	}

	// make sure an illegal target frequency wasn't specified.
	if (settings.cpu_max_freq > settings.cpuinfo_max_freq ) {
		settings.cpu_max_freq = settings.cpuinfo_max_freq;
	}

	/* print some information about the values we set */
	LOGI("\tSet polling interval to %dms.\n",
			getpid(), US_TO_MS(settings.polling_interval));

	LOGI("\tSet maximum scaling freq to %dMHz.\n",
			getpid(), KHZ_TO_MHZ(settings.cpu_max_freq));

	LOGI("\tSet cpu scaling step to %dMHz.\n",
			getpid(), KHZ_TO_MHZ(settings.cpu_scaling_step));

	LOGI("\tSet cpu target temperature to %dmC.\n",
			getpid(), settings.cpu_target_temperature);

	LOGI("\tSet cpu hysteresis range to %d percent.\n",
			getpid(), (int)(settings.hysteresis_range*100));

	LOGI("\tSet hysteresis threshold to %d intervals.\n",
			getpid(), settings.hysteresis_reset_threshold);

	LOGI("\tCalculated temperature hysteresis deviation of %dmC.\n",
			getpid(), settings.hysteresis_deviation);

	LOGI("\tCalculated temperature hysteresis lower limit of %dmC.\n",
			getpid(), settings.hysteresis_lower_limit);

	LOGI("\tCalculated temperature hysteresis upper limit of %dmC.\n",
			getpid(), settings.hysteresis_upper_limit);

	LOGI("\tSet cpus to throttle to %d.\n", getpid(), settings.num_cores);

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {
		// make sure an illegal target fan speed wasn't specified.
		if (settings.fan_min_speed > settings.fan_hw_max_speed ) {
			settings.fan_min_speed = settings.fan_hw_max_speed;
		}
		else if (settings.fan_min_speed < settings.fan_hw_min_speed ) {
			settings.fan_min_speed = settings.fan_hw_min_speed;
		}

		LOGI("\tSet fan scaling step to %d.\n",
				getpid(), settings.fan_scaling_step);

		LOGI("\tSet fan minimum speed to %d.\n",
				getpid(), settings.fan_min_speed);

		LOGI("\tSuccessfully read fan speed limits.\n"
			"\t\tspeed_max: %d\t speed_min: %d\n", getpid(),
				settings.fan_hw_max_speed, settings.fan_hw_min_speed);
	}

	LOGI("Done reading/setting throttling parameters.\n", getpid());

	/* start the scaling/throttling threads */
	LOGI("Starting throttling threads...\n", getpid());
	int i;
	for (i = 0; i < settings.num_cores; i++) {
		/* don't scale the virtual cores */
		if ((i % 2) && (settings.cpu_ht_available)) continue;

		LOGI("Starting thread for cpu%d...\n", getpid(), i);

		int rc = pthread_create(&(pthreads_arr[i]), NULL, worker, &i);
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
		int rc = pthread_join(pthreads_arr[i], NULL);
		if (rc) {
			LOGE("Failed to join thread for cpu%d...\n", getpid(), i);
		}

		LOGI("Resetting cpu%d maximum frequency...\n", getpid(), i);
		reset_max_freq(i);
	}
}
