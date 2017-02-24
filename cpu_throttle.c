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

	/* loop variables */
	int i, rc;

	/* Read sysfs interfaces and populate the throttle_settings
	 * buffer with basic defaults. */
	initialise_settings();

	/* parse the command line */
	parse_commmand_line(argc, argv);

	/* initialise the sigaction struct */
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/*define the pthreads array */
	pthread_t pthreads_arr[settings.num_cores+1];

	/* set up signal handlers */
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("sigaction");
	}
	if (sigaction(SIGHUP, &sa, NULL) == -1) {
		perror("sigaction");
	}

	/* print statements to stderr for now */
	log_file = stderr;

	if (write_config) {
		/* write to configuration file if one was passed */
		LOGI("Saving configuration...\n", getpid());
		write_configuration_file();
		return 0;
	}
	else {
		/* read a configuration file if one was passed */
		read_configuration_file();
	}

	/* validate the settings read */
	validate_settings();

	/* open the log file */
	if (settings.logging_enabled) {
		if (!(log_file = fopen(settings.log_path, "a+"))) {

			perror("fopen");
			fprintf(stderr, "Could not open log file\n");

			/* print statements to stderr */
			log_file = stderr;
		}
	}

	LOGI("Firing up...\n", getpid());
	LOGI("\n",getpid());

	LOGI("\tFound cpu core temp hwmon node at %d.\n",
			getpid(), settings.sysfs_coretemp_hwmon_node);

	/* check if the sysfs fan control node exist */
	if (settings.sysfs_fanctrl_hwmon_subnode == -1) {
		LOGW("\tCould not find fan control hwmon directory."
				" Working without it.\n", getpid());
	}
	else {
		LOGI("\tFound fan-control sysfs interface at node %d.\n",
				getpid(), settings.sysfs_fanctrl_hwmon_subnode);
	}

	LOGI("\tSuccessfully read cpu scaling limits.\n"
		"\t\t\tmax_freq:%dMHz min_freq:%dMHz\n", getpid(),
			KHZ_TO_MHZ(settings.cpuinfo_max_freq),
			KHZ_TO_MHZ(settings.cpuinfo_min_freq));

	/* print some information about the values we set */
	LOGI("\n",getpid());

	LOGI("\tSet polling interval to %dms.\n",
			getpid(), US_TO_MS(settings.polling_interval));

	LOGI("\tSet maximum scaling freq to %dMHz.\n",
			getpid(), KHZ_TO_MHZ(settings.cpu_max_freq));

	LOGI("\tSet cpu scaling step to %dMHz.\n",
			getpid(), KHZ_TO_MHZ(settings.cpu_scaling_step));

	LOGI("\tSet cpu target temperature to %dC.\n",
			getpid(), MC_TO_C(settings.cpu_target_temperature));

	LOGI("\tSet cpus to throttle to %d.\n", getpid(), settings.num_cores);

	LOGI("\n",getpid());

	LOGI("\tSet hysteresis to %dC."
			" Temp range: %dC <= %dC <= %dC \n",
			getpid(), MC_TO_C(settings.hysteresis),
			MC_TO_C(settings.hysteresis_lower_limit),
			MC_TO_C(settings.cpu_target_temperature),
			MC_TO_C(settings.hysteresis_upper_limit));

	LOGI("\tSet hysteresis threshold to %d intervals.\n",
			getpid(), settings.hysteresis_reset_threshold);

	LOGI("\n",getpid());

	if (settings.sysfs_fanctrl_hwmon_subnode != -1) {

		LOGI("\n",getpid());

		LOGI("\tSet fan scaling step to %d.\n",
				getpid(), settings.fan_scaling_step);

		LOGI("\tSet fan minimum speed to %d.\n",
				getpid(), settings.fan_min_speed);

		LOGI("\tSuccessfully read fan speed limits.\n"
			"\t\tspeed_max: %d\t speed_min: %d\n", getpid(),
				settings.fan_hw_max_speed, settings.fan_hw_min_speed);
		LOGI("\n",getpid());
	}

	/* start the scaling/throttling threads */
	LOGI("Done reading/setting throttling parameters. "
			"Starting throttling threads...\n", getpid());

	/* use a fixed memory area for the values
	 * needed by pthread */
	int core[settings.num_cores];

	/* start the worker threads */
	for (i = 0; i < settings.num_cores; i++) {

		LOGI("Starting thread for cpu%d...\n", getpid(), i);

		core[i] = i;

		rc = pthread_create(&(pthreads_arr[i]),
				NULL, cpu_worker, &(core[i]));
		if (rc) {
			LOGE("Failed to start thread for cpu%d...\n",
				getpid(), core[i]);
			exit(EXIT_FAILURE);
		}
	}

	/* start the fan worker */
	rc = pthread_create(&(pthreads_arr[i]), NULL, fan_worker, NULL);
	if (rc) {
		LOGE("Failed to start fan worker thread.\n", getpid());
	}

	/* wait for the worker threads to finish */
	for (i = 0; i <= settings.num_cores; i++) {
		LOGI("[worker%d] Waiting for thread...\n", getpid(), i);
		int rc = pthread_join(pthreads_arr[i], NULL);
		if (rc) {
			LOGE("[worker%d] Failed to join thread.\n", getpid(), i);
		}
	}
	return 0;
}
