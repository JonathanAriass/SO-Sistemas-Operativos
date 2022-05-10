#!/bin/bash

echo "60
50
40
30
5" > MemConfigTest

echo "55  
133
ADD 2 0
INC -1
TRAP 4
ZJUMP 2
JUMP -3
HALT
" > test3-1

echo "45
133
ADD 0 -2
INC 1
TRAP 4
ZJUMP 2
JUMP -3
IRET
" > test3-2

echo "35
133
ADD 0 -4
INC 2
TRAP 4
ZJUMP 2
JUMP -3
OS
" > test3-3

make clean; make

if [ -x Simulator ]; then
	echo
	echo "Running: ./Simulator --debugSections=a --memConfigFile=MemConfigTest test3-1 10 test3-2 11 test3-3 12"
	echo "output saved in \"OutputForV4-test3.log\""
	./Simulator --debugSections=a --memConfigFile=MemConfigTest test3-1 10 test3-2 11 test3-3 12 2>&1 | head -n 3000 > OutputForV4-test3.log
	echo
	echo "Calculating diferences and sending to \"differences-test3\" file..."
	echo "Calculating diferences and sending to \"differences-test3\" file..."
	diff TeacherOutputForV4-test3.log OutputForV4-test3.log | tee differences-test3
	echo
	echo "Diferences are in \"differences-test3\" file..."
	# rm programVerySimpleTest daemonTest[123] teachersDaemonsTest memConfigTest 

	

else 
	echo "Don't compile !!!"
fi

make clean &>/dev/null