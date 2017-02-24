 /**
* cpu_throttle.h
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

#ifndef CPU_THROTTLE_H
#define CPU_THROTTLE_H

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>

#define MAX_BUF_SIZE 255
#define MIN_BUF_SIZE 32

#define C_TO_MC(x) (x*1000)
#define MC_TO_C(x) (x/1000)
#define MS_TO_US(x) (x*1000)
#define US_TO_MS(x) (x/1000)
#define MHZ_TO_KHZ(x) (x*1000)
#define KHZ_TO_MHZ(x) (x/1000)

#define LOGE(...) fprintf(log_file, "[%d] E: " __VA_ARGS__)
#define LOGI(...) fprintf(log_file, "[%d] I: " __VA_ARGS__)
#define LOGW(...) fprintf(log_file, "[%d] W: " __VA_ARGS__)

/* forward declaration of struct */
struct throttle_settings;

/*==== GLOBALS ===== */
FILE * log_file;

/* termination signal */
int termination_signaled;

/* struct to store runtime settings */
struct throttle_settings settings;

/* config file */
char * config_file_path;
int write_config;

/* hwmon sysfs interface info */
int sysfs_coretemp_hwmon_node;
int sysfs_fanctrl_hwmon_node;
int sysfs_fanctrl_hwmon_subnode;

/* values calculated from hysteresis range */
int hysteresis_upper_limit;
int hysteresis_lower_limit;

/* fan speed information read from sysfs */
int fan_hw_min_speed;
int fan_hw_max_speed;

/* CPU scaling frequency information read from sysfs.
 * These values are in KHz. */
int cpuinfo_min_freq;
int cpuinfo_max_freq;

struct throttle_settings {
	/* logging */
	char log_path[MAX_BUF_SIZE];
	int logging_enabled;
	int verbose;

	/* temperature hysteresis variables 
	 * hysteresis: input by user to determine range
	 * in which to stop throttling cpu frequency.*/
	int hysteresis;

	/* this represents the number of consecutive intervals
	 * in hysteresis that can pass before we reset frequency
	 * and fan speed to hardware defaults. */
	int hysteresis_reset_threshold;

	/* actual minimum speed to be used when adjusting
	 * temperature. This can be user defined. */
	int fan_min_speed;

	/* Scaling step for the fan. Amount by which we
	 * scale after every interval */
	int fan_scaling_step;

	/* cpu target temperature in mC */ 
	int cpu_target_temperature;

	/* The target frequency to scale to, in KHz */
	int cpu_max_freq;

	/* Scaling step in Khz. Amount by which we scale
	 * after every interval */
	int cpu_scaling_step;

	/* how long we wait before reading sysfs files
	 *  again in uS */
	int polling_interval;

	/* the number of cores to scale. In most cases 1 is
	 * sufficent since all cores are usually share
	 *  frequency limits. */
	int num_cores;

};

/* Read the file at filename and returns the integer
 * value in the file.
 *
 * @prereq: Assumes that integer is non-negative.
 *
 * @return: integer value read if succesful, -1 otherwise. */
int read_integer(const char* filename); 

/* Write value to the file at filename.
 *
 * value is written to file as a string, not an integer.
 *
 * @return: 0 if succesful, -1 otherwise. */
int write_integer(const char* filename, int value);

/* Reset the maximum frequency on cpu core to the
 * maximum defined in sysfs
 *
 * @return: 0 if succesful, -1 otherwise. */
int reset_max_freq(int core);

/* Decrease the maximum frequency on cpu core by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int decrease_max_freq(int core, int step);

/* Increase the maximum frequency on cpu core by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int increase_max_freq(int core, int step);


/* Reset the fan speed to the
 * (user defined) minimum fan speed.
 *
 * @return: 0 if succesful, -1 otherwise. */
int reset_fan_speed(void);

/* Increase the fan speed by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int increase_fan_speed(int step);

/* Decrease the fan speed by step
 *
 * @return: 0 if succesful, -1 otherwise. */
int decrease_fan_speed(int step);

/* Worker function which does the actual throttling.
 * Is intended to be run as a pthread. */
void * cpu_worker(void* worker_num);

/* Worker function which does the actual throttling.
 * Is intended to be run as a pthread. */
void * fan_worker(void* worker_num);

/* Helper function to parse command line arguments from main */
void parse_commmand_line(int argc, char *argv[]);

/* Signal handler function to handle termination
 * signals and reset hardware settings to original. */
void handler(int signal);

/* Read the configuration specified by the user.
 *
 * @return: 0 if succesful, -1 otherwise. */
int read_configuration_file();

/* Write to the configuration specified by the user.
 *
 * @return: 0 if succesful, -1 otherwise. */
int write_configuration_file();

/* Verifies taht settings input are valid */
void validate_settings();

/* Read sysfs interfaces and populate the
 * throttle_settings buffer with basic defaults. */
void initialise_settings();

#endif /* CPU_THROTTLE_H */
