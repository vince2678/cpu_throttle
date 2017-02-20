#define _XOPEN_SOURCE 600
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <getopt.h>

#define MAX_BUF 255
#define FAN_CTRL_DIR "/sys/devices/platform/asus_fan/hwmon/hwmon%d/%s"
#define CT_HWMON_DIR "/sys/devices/platform/coretemp.0/hwmon/hwmon%d/%s"
#define SCALING_DIR "/sys/devices/system/cpu/cpu%d/cpufreq/%s"

#define C_TO_MS(x) (x*1000)
#define MS_TO_US(x) (x*1000)
#define MHZ_TO_KHZ(x) (x*1000)

#define LOGE(...) fprintf(log_file, "[%d] E: " __VA_ARGS__)
#define LOGI(...) fprintf(log_file, "[%d] I: " __VA_ARGS__)
#define LOGW(...) fprintf(log_file, "[%d] W: " __VA_ARGS__)

/* SOME GLOBAL VARIABLES */

FILE * log_file;
char log_path[MAX_BUF];

int LOGGING;
/* the numbers of the hwmon nodes.*/
int CT_HWMON_NODE;
int FAN_CTRL_HWMON_NODE;
int FAN_CTRL_HWMON_SUBNODE;

/* cpu target temperature settings. C*1000 */ 
int CPU_TARGET_TEMP;

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
int SCALING_STEP;
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

	/* format the full file name */
	//char filename[MAX_BUF];
	//sprintf(filename, CT_HWMON_DIR, CT_HWMON_NODE, temp_filename);
	/* format the full file name */
	//sprintf(file, SCALING_DIR, core, "scaling_max_freq");
	
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

/* Sets the maximum frequency on cpu core to freq */
int set_max_freq(int core, int freq)
{
	char filename[MAX_BUF];

	/* format the full file name */
	sprintf(filename, SCALING_DIR, core, "scaling_max_freq");

	/* write the string to the file and return */
	return write_integer(filename, freq);
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

void freq_max_worker(int core) {

	/* let's loop forever */
	while (1) {
				/* enable manual fan control */
				//write_integer(fanctrl_file, 1);

		/* read the current temps */

		usleep(POLLING_INTERVAL_US);
	}

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
		{"threading",	no_argument,       0, 'v' },
		{"cores",	no_argument,       0, 'c' },
		{"help",	no_argument,       0, 'h' },
		{0,         0,                 0,  0 }
	};

	/* set sane defaults for everything */
	POLLING_INTERVAL_US = MS_TO_US(75);
	SCALING_TARGET_FREQ = MHZ_TO_KHZ(2400);
	SCALING_STEP = MHZ_TO_KHZ(100);
	CPU_TARGET_TEMP = C_TO_MS(67);
	HT_AVAILABLE = 0;
	NUM_CORES = 1;

	/* disable logging by default */
	LOGGING = 0;
	log_file = stderr;

	/* read in the command line args if anything was passed */
	while ( (opt = getopt_long (argc, argv, "i:f:s:c:t:l:hv", long_options, &optind)) != -1 ) {
		switch (opt) {
		    case 'i':
			POLLING_INTERVAL_US=MS_TO_US(atoi(optarg));
			break;
		    case 'f':
			SCALING_TARGET_FREQ=MHZ_TO_KHZ(atoi(optarg));
			break;
		    case 's':
			SCALING_STEP=MHZ_TO_KHZ(atoi(optarg));
			break;
		    case 't':
			CPU_TARGET_TEMP=C_TO_MS(atoi(optarg));
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
			fprintf (stderr, "  -i, --interval\ttime to wait before scaling again, in ms\n" );
			fprintf (stderr, "  -f, --freq\tfrequency to limit to, in MHz\n" );
			fprintf (stderr, "  -s, --step\tscaling step, in MHz\n" );
			fprintf (stderr, "  -t, --temp\tTarget temperature, in degrees. \n" );
			fprintf (stderr, "  -v, --threading\tWhether threading is available or not. \n" );
			fprintf (stderr, "  -c, --cores\tnumber of (physical) cores on the system\n" );
			fprintf (stderr, "  -l, --log\tPath to log file\n" );
			fprintf (stderr, "  -h, --help\tprint this message\n");
			exit (EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[])
{
	/* parse the command line */
	parse_commmand_line(argc, argv);

	/* open the log file */
	if (LOGGING == 1) {
		if (!(log_file = fopen(log_path, "a+"))) {
			perror("fopen");
			fprintf(stderr, "Could not open log file\n");
			exit(EXIT_FAILURE);
		}
	}

	LOGI("Firing up...\n", getpid());
	LOGI("Setting default throttling parameters...\n", getpid());
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
			LOGI("Found cpufreq hwmon node at %d.\n", getpid(), i);
		}

		// format the filename
		sprintf(filename, FAN_CTRL_DIR, i, "");

		// stat the dir
		if (stat(filename, &stat_buf) != -1) {
			// we found the node
			FAN_CTRL_HWMON_NODE = i;
			LOGI("Found Asus fan-control hwmon node at %d.\n", getpid(), i);

			/* set the fan speeds */
			FAN_MAX_SPEED = 255;
			FAN_MIN_SPEED = 10;
		}
	}
	if (CT_HWMON_NODE == -1) {
		LOGE("Could not find core control hwmon directory.\n", getpid());
		exit(EXIT_FAILURE);
	}
	if (FAN_CTRL_HWMON_NODE == -1) {
		LOGW("Could not find fan control hwmon directory. Working without it.\n", getpid());
	}
	else {
		for (i = 0; i < max_tries; i++) {
			char fanctrl_file[MAX_BUF];

			// format the filename
			sprintf(fanctrl_file, "pwm%d_enable", i);
			sprintf(filename, FAN_CTRL_DIR, i, fanctrl_file);

			// stat the file
			if (stat(filename, &stat_buf) != -1) {
				// we found the node
				FAN_CTRL_HWMON_NODE = i;
				LOGI("Found asus fan-control sysfs interface.\n", getpid());
				// set the fan control global variable.
				FAN_CTRL_HWMON_SUBNODE = i;
			}
		}
	}

	/* get the cpuinfo scaling limits */
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_min_freq");
	CPUINFO_MIN_FREQ = read_integer(filename);
	sprintf(filename, SCALING_DIR, 0, "cpuinfo_max_freq");
	CPUINFO_MAX_FREQ = read_integer(filename);

	if ((CPUINFO_MIN_FREQ == -1) || (CPUINFO_MIN_FREQ == -1)) {
		LOGE("Could not read cpu scaling limits.\n", getpid());
		exit(EXIT_FAILURE);
	}
	else {
		LOGI("Successfully read cpu scaling limits. "
			       	" max_freq: %d\t min_freq: %d\n", getpid(),
				CPUINFO_MAX_FREQ, CPUINFO_MIN_FREQ);
	}

	// make sure an illegal target frequency wasn't specified.
	if (SCALING_TARGET_FREQ > CPUINFO_MAX_FREQ ) SCALING_TARGET_FREQ = CPUINFO_MAX_FREQ;
}
