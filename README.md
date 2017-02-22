# cpu_throttle
A simple cpu temperature control daemon/program for cpufreq-compatible systems.

## How to use
1. Clone the repository:
	`git clone https://github.com/vince2678/cpu_throttle`

2. Compile the program:
	`cd cpu_throttle && make`

3. Install the program:
	`make install`

4. Run the program. The default settings are ok, but they can be tweaked for better performance.

## Why did you write this?
The tools available already for such a task were either too difficult to configure and get going, or simply didn't cut it when it came to maintaining balance between cpu speed and overall system temperature.
This program is simple enough to set up in a minute and can lower system temperature without cutting too much into cpu speed when the system is under load.

## How do I configure the program?
The tool does not provide a systemd service or an init script, but can be easily set up to start at boot. The program accepts the following command line switches:

```
  -i, --interval	 Time to wait before scaling again, in ms.
  -f, --freq		 Maximum frequency cpus can attain, in MHz.
  -s, --cpu-step	 Scaling step, in MHz
  -a, --fan-step	 Fan scaling step.
  -t, --temp		 Target temperature, in degrees.
  -e, --minimum-fan-speed	 Minimum speed fan can reach.
  -r, --hysteresis	 Hysteresis range (< 51, in percent) as an integer.
  -u, --reset-threshold	 Number of intervals spent consecutively
			 in hysteresis before fan speed and cpu clock are reset.
  -o, --config		 Path to read/write binary config.
  -c, --cores		 Number of (physical) cores on the system.
  -l, --log		 Path to log file.
  -v, --verbose		 Print detailed throttling information.
  -h, --help		 Print this message.
```

The program saves the configurations in a binary format if a path to a config file is provided.
