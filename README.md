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
The tool provides a systemd service which is intalled when `make install` is run. This enables the service by default.
The program accepts the following command line switches:

```
  -i, --interval	 Time to wait before scaling again, in ms.
  -f, --max-freq		 Maximum frequency cpus can attain, in MHz.
  -s, --cpu-step	 Scaling step, in MHz
  -a, --fan-step	 Fan scaling step.
  -t, --temp		 Target temperature, in degrees.
  -e, --minimum-fan-speed	 Minimum speed fan can reach.
  -r, --hysteresis	 Hysteresis deviation range in degrees.
  -u, --reset-threshold	 Number of intervals spent consecutively
			 in hysteresis before fan speed and cpu clock are reset.
  -o, --config		 Path to read/write binary config.
  -w, --write-config		 Just save the new configuration and exit.
  -c, --cores		 Number of (physical) cores on the system.
  -l, --log		 Path to log file.
  -v, --verbose		 Print detailed throttling information.
  -h, --help		 Print this message.
```

The program saves the configurations in a binary format if a path to a config file is provided.
By default the systemd script is set to read the binary configuration from `/etc/cpu_throttle/cpu_throttle.dat` . A binary configuration can be generated like this for example:
`sudo cpu_throttle --fan-step 20 --temp 57 --hysteresis 6 --log /var/log/cpu_throttle.log -o /etc/cpu_throttle/cpu_throttle.dat --verbose --write-config`

The service can then be started (or reloaded) and the settings will take effect.
