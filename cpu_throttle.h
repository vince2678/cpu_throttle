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
#define FAN_CTRL_DIR "/sys/devices/platform/asus_fan/hwmon/hwmon%d/%s"
#define CT_HWMON_DIR "/sys/devices/platform/coretemp.0/hwmon/hwmon%d/%s"
#define SCALING_DIR "/sys/devices/system/cpu/cpu%d/cpufreq/%s"

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
pthread_t * threads;
FILE * log_file;
struct throttle_settings settings;

struct throttle_settings {
	/* logging */
	char log_path[MAX_BUF_SIZE];
	int logging_enabled;

	/* hwmon sysfs interface info */
	int sysfs_coretemp_hwmon_node;
	int sysfs_fanctrl_hwmon_node;
	int sysfs_fanctrl_hwmon_subnode;

	/* temperature hysteresis variables 
	 * hysteresis_range: input by user to determine
	 * range in which to stop throttling cpu frequency.*/
	float hysteresis_range;

	/* values calculated from hysteresis range */
	int hysteresis_deviation;
	int hysteresis_upper_limit;
	int hysteresis_lower_limit;

	/* this represents the number of consecutive intervals
	 * in hysteresis that can pass before we reset frequency
	 * and fan speed to hardware defaults. */
	int hysteresis_reset_threshold;

	/* CPU scaling frequency information.
	 * These values are in KHz. */
	int cpuinfo_min_freq;
	int cpuinfo_max_freq;

	/* fan speed information read from sysfs */
	int fan_min_speed;
	int fan_max_speed;

	/* cpu target temperature in mC */ 
	int cpu_target_temperature;

	/* The target frequency to scale to, in KHz */
	int cpu_target_freq;

	/* Scaling step in Khz. Amount by which we scale
	 * after every interval */
	int cpu_scaling_step;

	/* Scaling step for the fan. Amount by which we
	 * scale after every interval */
	int fan_scaling_step;
	int fan_target_speed;

	/* how long we wait before reading sysfs files
	 *  again in uS */
	int polling_interval;

	/* Boolean value input by user to determine
	 * whether to treat some cores as vcoresand
	 * scale them or not */
	int cpu_ht_available;

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

/* Write out_str to the file at filename.
 *
 * @return: 0 if succesful, -1 otherwise. */
int write_string(const char* filename, const char* out_str);

/* Set the scaling governor on cpu core to governor
 *
 * @return: 0 if succesful, -1 otherwise. */
int set_governor(int core, const char* governor);

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

/* Reset the fan speed to the target fan speed.
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
void * worker(void* worker_num);

/* Helper function to parse command line arguments from main */
void parse_commmand_line(int argc, char *argv[]);

/* Signal handler function to handle termination
 * signals and reset hardware settings to original. */
void handler(int signal);

#endif /* CPU_THROTTLE_H */
