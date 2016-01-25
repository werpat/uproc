# HOWTO
It is best practice to isolate one core from the scheduler and bind the benchmark to this core. Additionally turboboost may be disable to get reproducible results.
First can be done during the boot process by adding the kernelvariable `isolcpus=<core_id0>,<core_id1>,..` to the kernelparameters.
To disable turboboost, just execute the script `disable_turboboost.sh` with superuser rights.

To acctually start the benchmark and bind it to a specific hardware thread and core execute `taskset <core_id> ./cachebench`.
If this is not done, the performance stats gathered with the assembly instruction `rdtscp` can produce garbage, as start and end values are evantually read on different cores,which may initialise the performance registers differently.
