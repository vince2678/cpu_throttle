#define _XOPEN_SOURCE 600
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <getopt.h>

#define MAX_BUF 255
#define MINI_BUF 32
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

/* SOME GLOBAL VARIABLES */

/* Array of pthread_t. Array is defined in main() */
pthread_t * threads;

FILE * log_file;
char log_path[MAX_BUF];

int LOGGING;
/* the numbers of the hwmon nodes.*/
int CT_HWMON_NODE;
int FAN_CTRL_HWMON_NODE;
int FAN_CTRL_HWMON_SUBNODE;

/* cpu target temperature settings. C*1000 */ 
int CPU_TARGET_TEMP;

/* temperature hysteresis variables */
int HYSTERESIS_TEMP_DEVIATION;
int HYSTERESIS_TEMP_UPPER_LIMIT;
int HYSTERESIS_TEMP_LOWER_LIMIT;
int HYSTERESIS_COUNT_RESET_THRESHOLD;
float HYSTERESIS_TEMP_PERCENT;

/* CPU scaling frequency information.
 * These values are in KHz. */
int CPUINFO_MIN_FREQ;
int CPUINFO_MAX_FREQ;

/* fan speed information */
int FAN_MIN_SPEED;
int FAN_MAX_SPEED;

/* USER DEFINED VARIABLES */

/* maximum we are allowed to scale to */
int SCALING_TARGET_FREQ;
/* Scaling step in Khz. Amount by which we scale after every interval */
int CPU_SCALING_STEP;
/* Scaling step for the fan. Amount by which we scale after every interval */
int FAN_STEP;
int FAN_REST_SPEED;
/* how long we wait before reading sysfs files again in uS */
int POLLING_INTERVAL_US;

int HT_AVAILABLE;
int NUM_CORES;

/* Returns the nonnegative integer value in the file */ 
int read_integer(const char* filename) {

	int retval;
	FILE * file;

	/* open the file */
	if (!(file = fopen(filename, "rw"))) {
		perror("open");
		LOGE("%s\n", getpid(), strerror(errno));
		return -1;
	}
	/* read the value from the file */
	fscanf(file, "%d", &retval);
	fclose(file);

	return retval;
}
	
/* Writes the nonnegative integer value in the file.
 * The integer is written *as a string* . */ 
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

/* Writes a string to the file provided */
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

/* Sets the scaling governor on cpu core to governor */
int set_governor(int core, const char* governor)
{
	char filename[MAX_BUF];

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_governor");

	/* write the string to the file */
	return write_string(filename, governor);
}

/* Resets the maximum frequency on cpu core to the maximum permissible*/
int reset_max_freq(int core)
{
	char filename[MAX_BUF];

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	LOGI("\tResetting cpu %d speed to %d.\n", getpid(), core, CPUINFO_MAX_FREQ);

	/* write the string to the file and return */
	return write_integer(filename, CPUINFO_MAX_FREQ);
}

/* Decreases the maximum frequency on cpu core by the step */
int decrease_max_freq(int core, int step)
{
	char filename[MAX_BUF];
	int freq;

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	/* read the current max frequency */
	freq = read_integer(filename);

	/* determine the new frequency */
	freq -= step;
	if (freq < CPUINFO_MIN_FREQ) {
		freq = CPUINFO_MIN_FREQ;
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

/* Increases the maximum frequency on cpu core by the step */
int increase_max_freq(int core, int step)
{
	char filename[MAX_BUF];
	int freq;

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	/* read the current max frequency */
	freq = read_integer(filename);

	/* determine the new frequency */
	freq += step;
	if (freq > SCALING_TARGET_FREQ) {
		freq = SCALING_TARGET_FREQ;
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

/* Reset the fan speed */
int reset_fan_speed(void)
{
	char fanctrl_filename[MAX_BUF];
	char filename_buf[MINI_BUF];

	/* format the full file name */
	sprintf(filename_buf, "pwm%d", FAN_CTRL_HWMON_SUBNODE);
	sprintf(fanctrl_filename, FAN_CTRL_DIR,
			FAN_CTRL_HWMON_NODE, filename_buf);

	LOGI("\tResetting fan speed to %d.\n", getpid(), FAN_REST_SPEED);

	/* set the fan speed */
	return write_integer(fanctrl_filename, FAN_REST_SPEED);
}

/* Increase the fan speed */
int increase_fan_speed(int step)
{
	char fanctrl_filename[MAX_BUF];
	char filename_buf[MINI_BUF];
	int fan_speed;

	/* format the full file name */
	sprintf(filename_buf, "pwm%d", FAN_CTRL_HWMON_SUBNODE);
	sprintf(fanctrl_filename, FAN_CTRL_DIR,
			FAN_CTRL_HWMON_NODE, filename_buf);

	/* read the current fan speed */
	fan_speed = read_integer(fanctrl_filename);

	/* determine the new fan speed */
	fan_speed += step;
	if (fan_speed > FAN_MAX_SPEED) {
		fan_speed = FAN_MAX_SPEED;
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

/* Decrease the fan speed */
int decrease_fan_speed(int step)
{
	char filename_buf[MINI_BUF];
	char fanctrl_filename[MAX_BUF];
	int fan_speed;

	/* format the full file name */
	sprintf(filename_buf, "pwm%d", FAN_CTRL_HWMON_SUBNODE);
	sprintf(fanctrl_filename, FAN_CTRL_DIR,
			FAN_CTRL_HWMON_NODE, filename_buf);

	/* read the current fan speed */
	fan_speed = read_integer(fanctrl_filename);

	/* determine the new fan speed */
	fan_speed -= step;
	if (fan_speed < FAN_MIN_SPEED) {
		fan_speed = FAN_MIN_SPEED;
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

void * worker(void* worker_num) {

	/* buffers for storing file names */
	char fanctrl_filename[MAX_BUF];
	char temperature_filename[MAX_BUF];

	/* general filename buffer. */
	char filename_buf[MINI_BUF];

	int core = *(int*)worker_num;
	int curr_temp = 0, prev_temp = 0;

	/* count the number of intervals spent in hysteresis */
	int intervals_in_hysteresis = 0;

	/* Format the temperature reading file. We add two because the
	 * hwmon files are 1-indexed and the first one is that of the whole die. */
	sprintf(filename_buf, "temp%d_input", core+2);
	sprintf(temperature_filename, CT_HWMON_DIR, CT_HWMON_NODE, filename_buf);

	if (FAN_CTRL_HWMON_SUBNODE != -1) {
		/* format the full file name */
		sprintf(filename_buf, "pwm%d_enable", FAN_CTRL_HWMON_SUBNODE);
		sprintf(fanctrl_filename, FAN_CTRL_DIR,
				FAN_CTRL_HWMON_NODE, filename_buf);
		/* enable manual fan control */
		write_integer(fanctrl_filename, 1);
	}

	/* let's loop forever */
	while (1) {
		/* sleep, then run the commands */
		usleep(POLLING_INTERVAL_US);

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
		if ((curr_temp >= HYSTERESIS_TEMP_LOWER_LIMIT)  && (curr_temp <= HYSTERESIS_TEMP_UPPER_LIMIT)) {

			/* update the hysteresis counter */
			intervals_in_hysteresis += 1;

			if (intervals_in_hysteresis == HYSTERESIS_COUNT_RESET_THRESHOLD) {

				/* reset the hysteresis counter */
				intervals_in_hysteresis = 0;

				/* reset the fan speed */
				if (FAN_CTRL_HWMON_SUBNODE != -1) {
					reset_fan_speed();
				}
				/* reset the speed to the CPU_TARGET_TEMP */
				increase_max_freq(core, (CPUINFO_MAX_FREQ));
			}
		}
		/*case 2: temp is below the (lower) hysteresis range of target */
		else if (curr_temp < HYSTERESIS_TEMP_LOWER_LIMIT) {

			/* reset hysteresis counter */
			intervals_in_hysteresis = 0;

			/* decrease the fan speed */
			if (FAN_CTRL_HWMON_SUBNODE != -1) {
				decrease_fan_speed((FAN_STEP/2));
			}
			/* adjust the processor speed by half a step */
			increase_max_freq(core, (CPU_SCALING_STEP/2));
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
						(float)HYSTERESIS_TEMP_DEVIATION));

			/* make sure to get the absolute value*/
			deviations = abs(deviations);

			/* if our temperature didn't change, move it a step */
			if (temp_difference == 0) {
				if (FAN_CTRL_HWMON_SUBNODE != -1) {
					/* adjust the fan speed by a step */
					increase_fan_speed((FAN_STEP));
				}
				/* adjust the processor speed by a step */
				decrease_max_freq(core, (CPU_SCALING_STEP));
			}
			/* if our current temp is lower than the previous one */
			else if (temp_difference > 0) {
				/* decrease the fan speed by half a step */
				if (FAN_CTRL_HWMON_SUBNODE != -1) {
					increase_fan_speed((FAN_STEP/2));
				}
				/* increase the processor speed by half a step */
				decrease_max_freq(core, (CPU_SCALING_STEP/2));
			}
			/* if our current temp is worse than the previous one */
			else {
				/* increase the fan speed by deviations steps */
				if (FAN_CTRL_HWMON_SUBNODE != -1) {
					increase_fan_speed(FAN_STEP * deviations);
				}
				/* decrease the processor speed by deviations steps */
				decrease_max_freq(core, CPU_SCALING_STEP * deviations);
			}
		}
		prev_temp = curr_temp;
	}
	return NULL;
}

/* Helper function that parses command line arguments to main
 */
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
		{"threading",	no_argument,       0, 'v' },
		{"cores",	no_argument,       0, 'c' },
		{"help",	no_argument,       0, 'h' },
		{0,         0,                 0,  0 }
	};

	/* set sane defaults for everything */
	POLLING_INTERVAL_US = MS_TO_US(500);
	SCALING_TARGET_FREQ = MHZ_TO_KHZ(2400);
	CPU_SCALING_STEP = MHZ_TO_KHZ(100);
	CPU_TARGET_TEMP = C_TO_MC(55);
	HYSTERESIS_TEMP_PERCENT = 0.1;
	HYSTERESIS_COUNT_RESET_THRESHOLD = 100;
	FAN_REST_SPEED = 50;
	FAN_STEP = 10;
	HT_AVAILABLE = 0;
	NUM_CORES = 1;

	/* disable logging by default */
	LOGGING = 0;
	log_file = stderr;

	/* read in the command line args if anything was passed */
	while ( (opt = getopt_long (argc, argv, "i:f:s:c:t:l:r:e:u:hv", long_options, &optind)) != -1 ) {
		switch (opt) {
		    case 'u':
			HYSTERESIS_COUNT_RESET_THRESHOLD=atoi(optarg);
			break;
		    case 'e':
			FAN_REST_SPEED=atoi(optarg);
			break;
		    case 'r':
			HYSTERESIS_TEMP_PERCENT=(float) (strtol(optarg, NULL, 10) / 100.0);
			break;
		    case 'i':
			POLLING_INTERVAL_US=MS_TO_US(atoi(optarg));
			break;
		    case 'f':
			SCALING_TARGET_FREQ=MHZ_TO_KHZ(atoi(optarg));
			break;
		    case 's':
			CPU_SCALING_STEP=MHZ_TO_KHZ(atoi(optarg));
			break;
		    case 't':
			CPU_TARGET_TEMP=C_TO_MC(atoi(optarg));
			break;
		    case 'l':
			strncpy(log_path, optarg, MAX_BUF);
			LOGGING = 1;
			break;
		    case 'v':
			HT_AVAILABLE = 1;
			break;
		    case 'c':
			NUM_CORES = atoi(optarg);
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
			fprintf (stderr, "  -c, --cores\tnumber of (physical) cores on the system.\n" );
			fprintf (stderr, "  -l, --log\tPath to log file.\n" );
			fprintf (stderr, "  -h, --help\tprint this message.\n");
			exit (EXIT_FAILURE);
		}
	}
}

void handler(int signal) {

	char filename_buf[MINI_BUF];
	char fanctrl_filename[MAX_BUF];
	int i;

	LOGI("Termination signal sent. Winding up...\n", getpid());

	if (FAN_CTRL_HWMON_SUBNODE != -1) {
		/* format the full file name */
		sprintf(filename_buf, "pwm%d_enable", FAN_CTRL_HWMON_SUBNODE);
		sprintf(fanctrl_filename, FAN_CTRL_DIR,
				FAN_CTRL_HWMON_NODE, filename_buf);
		/* disable manual fan control */
		LOGI("Enabling automatic fan control...\n", getpid());
		write_integer(fanctrl_filename, 0);
	}

	/* reset the cpu maximum frequency */
	for (i = 0; i < NUM_CORES; i++) {
		/* don't scale the virtual cores */
		if ((i % 2) && (HT_AVAILABLE)) continue;

		LOGI("Resetting cpu%d maximum frequency...\n", getpid(), i);
		reset_max_freq(i);
	}
	/* exit */
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	/* parse the command line */
	parse_commmand_line(argc, argv);

	/*define the pthreads array */
	pthread_t pthreads_arr[NUM_CORES];

	/* set the global pointer */
	threads = pthreads_arr;

	/* initialise the sigaction struct */
	struct sigaction sa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/* set up signal handlers */
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("fopen");
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("fopen");
	}

	/* open the log file */
	if (LOGGING == 1) {
		if (!(log_file = fopen(log_path, "a+"))) {
			perror("fopen");
			fprintf(stderr, "Could not open log file\n");
			exit(EXIT_FAILURE);
		}
	}

	LOGI("Firing up...\n", getpid());
	LOGI("Setting throttling parameters...\n", getpid());
	// initialise the hwmon global variables
	CT_HWMON_NODE = -1;
	FAN_CTRL_HWMON_NODE = -1;
	FAN_CTRL_HWMON_SUBNODE = -1;

	/* loop variables */
	// buffer to store stats on the dir
	struct stat stat_buf;
	char filename[MAX_BUF];

	int i = 0, max_tries = 10;

	/* find the hwmon node for the core control */
	for (i = 0; i < max_tries; i++) {
		// format the filename
		sprintf(filename, CT_HWMON_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			CT_HWMON_NODE = i;
			LOGI("\tFound cpufreq hwmon node at %d.\n", getpid(), i);
		}

		// format the filename
		sprintf(filename, FAN_CTRL_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			FAN_CTRL_HWMON_NODE = i;
			LOGI("\tFound Asus fan-control hwmon node at %d.\n", getpid(), i);
		}
	}
	if (CT_HWMON_NODE == -1) {
		LOGE("\tCould not find core control hwmon directory.\n", getpid());
		exit(EXIT_FAILURE);
	}
	if (FAN_CTRL_HWMON_NODE == -1) {
		LOGW("\tCould not find fan control hwmon directory. Working without it.\n", getpid());
	}
	else {
		for (i = 0; i < max_tries; i++) {
			char fanctrl_file[MINI_BUF];

			// format the filename
			sprintf(fanctrl_file, "pwm%d_enable", i);
			sprintf(filename, FAN_CTRL_DIR, i, fanctrl_file);

			// stat the file
			if (stat(filename, &stat_buf) != -1) {
				// we found the node
				LOGI("\tFound asus fan-control sysfs interface at node %d.\n", getpid(), i);
				// set the fan control global variable.
				FAN_CTRL_HWMON_SUBNODE = i;
			}
		}
	}
	/* calculate the hysteresis range */
	HYSTERESIS_TEMP_DEVIATION = (((float) CPU_TARGET_TEMP) * HYSTERESIS_TEMP_PERCENT);
	HYSTERESIS_TEMP_UPPER_LIMIT = CPU_TARGET_TEMP + HYSTERESIS_TEMP_DEVIATION;
	HYSTERESIS_TEMP_LOWER_LIMIT = CPU_TARGET_TEMP - HYSTERESIS_TEMP_DEVIATION;

	/* get the cpuinfo scaling limits */
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_min_freq");
	CPUINFO_MIN_FREQ = read_integer(filename);
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_max_freq");
	CPUINFO_MAX_FREQ = read_integer(filename);

	/* set the fan speeds */
	if (FAN_CTRL_HWMON_SUBNODE != -1) {
		char general_buf[MINI_BUF];
		sprintf(general_buf, "fan%d_speed_max", FAN_CTRL_HWMON_SUBNODE);
		sprintf(filename, FAN_CTRL_DIR, FAN_CTRL_HWMON_NODE, general_buf);
		FAN_MAX_SPEED = read_integer(filename);
		sprintf(general_buf, "fan%d_min", FAN_CTRL_HWMON_SUBNODE);
		sprintf(filename, FAN_CTRL_DIR, FAN_CTRL_HWMON_NODE, general_buf);
		FAN_MIN_SPEED = read_integer(filename);
	}

	if ((CPUINFO_MIN_FREQ == -1) || (CPUINFO_MIN_FREQ == -1)) {
		LOGE("\tCould not read cpu scaling limits.\n", getpid());
		exit(EXIT_FAILURE);
	}
	else {
		LOGI("\tSuccessfully read cpu scaling limits.\n"
			"\t\t\tmax_freq:%dMHz min_freq:%dMHz\n", getpid(),
				KHZ_TO_MHZ(CPUINFO_MAX_FREQ), KHZ_TO_MHZ(CPUINFO_MIN_FREQ));
	}

	// make sure an illegal target frequency wasn't specified.
	if (SCALING_TARGET_FREQ > CPUINFO_MAX_FREQ ) SCALING_TARGET_FREQ = CPUINFO_MAX_FREQ;

	/* print some information about the values we set */
	LOGI("\tSet polling interval to %dms.\n", getpid(), US_TO_MS(POLLING_INTERVAL_US));
	LOGI("\tSet scaling target freq to %dMHz.\n", getpid(), KHZ_TO_MHZ(SCALING_TARGET_FREQ));
	LOGI("\tSet cpu scaling step to %dMHz.\n", getpid(), KHZ_TO_MHZ(CPU_SCALING_STEP));
	LOGI("\tSet cpu target temperature to %dmC.\n", getpid(), CPU_TARGET_TEMP);
	LOGI("\tSet cpu hysteresis range to %d percent.\n", getpid(), (int)(HYSTERESIS_TEMP_PERCENT*100));
	LOGI("\tSet hysteresis threshold to %d intervals.\n", getpid(), HYSTERESIS_COUNT_RESET_THRESHOLD);
	LOGI("\tCalculated temperature hysteresis deviation of %dmC.\n", getpid(), HYSTERESIS_TEMP_DEVIATION);
	LOGI("\tCalculated temperature hysteresis lower limit of %dmC.\n", getpid(), HYSTERESIS_TEMP_LOWER_LIMIT);
	LOGI("\tCalculated temperature hysteresis upper limit of %dmC.\n", getpid(), HYSTERESIS_TEMP_UPPER_LIMIT);
	LOGI("\tSet cpus to throttle to %d.\n", getpid(), NUM_CORES);

	if (FAN_CTRL_HWMON_SUBNODE != -1) {
		// make sure an illegal target frequency wasn't specified.
		if (FAN_REST_SPEED > FAN_MAX_SPEED ) FAN_REST_SPEED = FAN_MAX_SPEED;
		else if (FAN_REST_SPEED < FAN_MIN_SPEED ) FAN_REST_SPEED = FAN_MIN_SPEED;

		LOGI("\tSet fan scaling step to %d.\n", getpid(), FAN_STEP);
		LOGI("\tSet fan rest speed to %d.\n", getpid(), FAN_REST_SPEED);
		LOGI("\tSuccessfully read fan speed limits.\n"
			"\t\tspeed_max: %d\t speed_min: %d\n", getpid(),
				FAN_MAX_SPEED, FAN_MIN_SPEED);
	}
	LOGI("Done reading/setting throttling parameters.\n", getpid());

	/* start the scaling/throttling threads */
	LOGI("Starting throttling threads...\n", getpid());
	for (i = 0; i < NUM_CORES; i++) {
		/* don't scale the virtual cores */
		if ((i % 2) && (HT_AVAILABLE)) continue;

		LOGI("Starting thread for cpu%d...\n", getpid(), i);

		int rc = pthread_create(&(threads[i]), NULL, worker, &i);
		if (rc) {
			LOGE("Failed to start thread for cpu%d...\n", getpid(), i);
			exit(EXIT_FAILURE);
		}
	}
	/* reset the cpu maximum frequency */
	for (i = 0; i < NUM_CORES; i++) {
		/* don't scale the virtual cores */
		if ((i % 2) && (HT_AVAILABLE)) continue;

		LOGI("Waiting for cpu%d thread to finish...\n", getpid(), i);
		int rc = pthread_join(threads[i], NULL);
		if (rc) {
			LOGE("Failed to join thread for cpu%d...\n", getpid(), i);
		}

		LOGI("Resetting cpu%d maximum frequency...\n", getpid(), i);
		reset_max_freq(i);
	}
}
