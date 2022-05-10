#!/bin/bash

echo "12
32
16
16
32" > MemConfigTest

echo "32
5
ADD 50 0
TRAP 7
TRAP 3
" > test2-1

echo "32
5
BADINSTRUCTION 1
" > test2-2


make clean; make

if [ -x Simulator ]; then
	echo
	echo "Running: ./Simulator --debugSections=a --memConfigFile=MemConfigTest test2-1 10 test2-2 30"
	echo "output saved in \"OutputForV4-test2.log\""
	./Simulator --debugSections=a --memConfigFile=MemConfigTest test2-1 10 test2-2 30 iDontExist 350 2>&1 | head -n 3000 > OutputForV4-test2.log
	echo
	echo "Calculating diferences and sending to \"differences-test2\" file..."
	diff TeacherOutputForV4-test2.log OutputForV4-test2.log | tee differences-test2
	echo
	echo "Diferences are in \"differences-test2\" file..."
	# rm programVerySimpleTest daemonTest[123] teachersDaemonsTest memConfigTest 

	

else 
	echo "Don't compile !!!"
fi

make clean &>/dev/null