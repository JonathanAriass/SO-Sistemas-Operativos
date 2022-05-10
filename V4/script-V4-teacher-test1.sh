#!/bin/bash

echo "12
4
16
96
64
16
32" > MemConfigTest

echo "32
5
ADD 10 0
TRAP 7
INC -1
ZJUMP 2
JUMP -3
INC 1
ZJUMP 2
JUMP 85
TRAP 3
" > daemonTest1

echo "32
5
ADD -10 0
TRAP 7
INC 1
ZJUMP 2
JUMP -3
TRAP 23
" > daemonTest2

echo "32
2
TRAP 4
READ 32
WRITE 209
WRITE 129
ADD 10 1
TRAP 7
INC -1
ZJUMP 2
JUMP -3
INC 1
ZJUMP 2
JUMP -44
TRAP 3
" > daemonTest3

echo "daemonTest1,10
daemonTest2,11
daemonTest3,12
daemonTest1,700" > teachersDaemonsTest

echo "30
5
NOP
ADD 10 -13
WRITE 15
HALT
" > programVerySimpleTest

make clean; make

if [ -x Simulator ]; then
	echo
	echo "Running: ./Simulator --debugSections=a --daemonsProgramsFile=teachersDaemonsTest --memConfigFile=MemConfigTest programVerySimpleTest 100"
	echo "output saved in \"OutputForV4-test1.log\""
	./Simulator --debugSections=a --daemonsProgramsFile=teachersDaemonsTest --memConfigFile=MemConfigTest programVerySimpleTest 100 2>&1 | head -n 3000 > OutputForV4-test1.log
	echo
	echo "Calculating diferences and sending to \"differences-test1\" file..."
	diff TeacherOutputForV4-test1.log OutputForV4-test1.log | tee differences-test1
	echo
	echo "Diferences are in \"differences-test1\" file..."
	# rm programVerySimpleTest daemonTest[123] teachersDaemonsTest memConfigTest 

	

else 
	echo "Don't compile !!!"
fi

make clean &>/dev/null