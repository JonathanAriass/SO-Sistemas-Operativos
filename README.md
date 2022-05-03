# SO-Sistemas-Operativos
Repo for SO subject (2 year uni course). 

# Info about V1
For using V1 you have to do a make to compile Objects and executables into the dir. Then you can run some example programms like programVerySimple. Then you will see the trace of the OS.

# Info about V2
In this version we take care of clock interruptions, where we block some processes and wake them up when some condition are satisfied. We take care of the output style too.

# Info about V3
For this version we change how to long term scheduler works. Now we can add more args to the execution program, like the clock tick for which the process will be created.

To execute this version we need to compile the program:

```sh
make
```

Then we will be able to run the program for different user programs:

```sh
./Simulator [--debugSections] [programFileName] [instant] | head -n 1000 > [outputFile]
./Simulator --debugSections=a programV2-2a 12 programV2-2b 8 programV2-2c 0 | head -n 1000 > V3-output.log
cat V3-output.log
```

# Info about V4
In this version we work with the space on memory creating different partitions on the memory space. This memory space is divided into different blocks where the programs will be stored (done in the OperatingSystem_ObtainMainMemory()).

To execute this version we need to compile the program:

```sh
make
```
Then we will be able to run the program for different user programs:

```sh
./Simulator [--debugSections] [programFileName] [instant] | head -n 1000 > [outputFile]
./Simulator --debugSections=a programV4-1 0 programV4-2 22 programV4-3 44 programV4-4 66 | head -n 550 > V4-2-output.log
cat V4-2-output.log
```


# How to run the simulator
First of all you should do a make of the code you want to use.

```sh
make
```

Then you should be able to run this command:

```sh
./Simulator [--debugSections] [programFileName] | head -n 1000 > [outputFile]
./Simulator --debugSections=a programV2-2a programV2-2b programV2-2c| head -n 1000 > V2-2-output.log
cat V2-2-output.log
```

This file will contain the information about the execution of the programs given as parameters.
