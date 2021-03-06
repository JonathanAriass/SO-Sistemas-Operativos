#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_HandleClockInterrupt();
int OperatingSystem_GetExecutingProcessID();

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID=PROCESSTABLEMAXSIZE-1;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0, 0};

char * queueNames[NUMBEROFQUEUES]={"USER","DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

char DAEMONS_PROGRAMS_FILE[MAXIMUMLENGTH]="teachersDaemons";

char * statesNames [5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"};

int numberOfClockInterrupts = 0;

heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses=0;

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile=fopen("OperatingSystemCode", "r");
	if (programFile==NULL){
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing Operating System!\n");
		exit(1);		
	}

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++){
		processTable[i].busy=0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	// We fill the arrival time queue
	ComputerSystem_FillInArrivalTimeQueue();
	OperatingSystem_PrintStatus();

	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	if (numberOfNotTerminatedUserProcesses <= 0 && numberOfProgramsInArrivalTimeQueue <= 0) {
		if (executingProcessID == sipID) {
			OperatingSystem_TerminatingSIP();
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
			return;
		}
		//OperatingSystem_TerminatingSIP();
		OperatingSystem_ReadyToShutdown();
	} else {
		OperatingSystem_PrintStatus();
	}

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing SIP program!\n");
		exit(1);		
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, i, numberOfSuccessfullyCreatedProcesses=0;
	while (OperatingSystem_IsThereANewProgram() == YES) {
	//for (i=0; programList[i]!=NULL && i<PROGRAMSMAXNUMBER ; i++) {
		//PID=OperatingSystem_CreateProcess(i);
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		PID = OperatingSystem_CreateProcess(i);
		if (PID >= 0) {
		
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type==USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
			//OperatingSystem_PrintStatus();
		} else {
			switch(PID) {
				case PROGRAMNOTVALID:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(104,ERROR,programList[i] -> executableName,"invalid priority or size");
					break;
				case PROGRAMDOESNOTEXIST:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(104,ERROR,programList[i]->executableName,"it does not exist");
					break;
				case NOFREEENTRY:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(103,ERROR,programList[i]->executableName);
					break;
				case TOOBIGPROCESS:
					OperatingSystem_ShowTime(ERROR);
					ComputerSystem_DebugMessage(105,ERROR,programList[i]->executableName);
					break;
				default:
					break;
			}
		}
	//}
	}
	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}


// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID=OperatingSystem_ObtainAnEntryInTheProcessTable();

	if (PID == -3) {
		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile=fopen(executableProgram->executableName, "r");
	// We check if the program exists
	if (programFile == NULL) {
		return PROGRAMDOESNOTEXIST;
	}


	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	

	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);
	
	// We check that the size and priority is valid
	if (processSize == -2 || priority == -2) {
		return PROGRAMNOTVALID;
	}
			
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);
	// We check if the programm size is small enough to fit on the memmory space
	if (loadingPhysicalAddress == TOOBIGPROCESS) {
		return TOOBIGPROCESS;
	}

	// Load program in the allocated memory
	if (OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize) == TOOBIGPROCESS) {
		return TOOBIGPROCESS;
	}
	
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	
	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(70,INIT,PID,executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	
 	return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(111,SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,statesNames[0]);
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID = DAEMONSQUEUE;
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
		processTable[PID].queueID = USERPROCESSQUEUE;
	}

}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	int previousState = processTable[PID].state;
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[processTable[PID].queueID] ,PROCESSTABLEMAXSIZE)>=0) {
		processTable[PID].state=READY;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,statesNames[previousState],statesNames[1]);
	} 
	//OperatingSystem_PrintReadyToRunQueue();
}

void OperatingSystem_MoveToTheBLOCKEDState() {
	int aux = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
	processTable[executingProcessID].whenToWakeUp = aux;
	if (Heap_add(executingProcessID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses,PROCESSTABLEMAXSIZE)>=0) {
		processTable[executingProcessID].state=BLOCKED;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[2],statesNames[3]);
		OperatingSystem_SaveContext(executingProcessID);
	} 
	//OperatingSystem_PrintStatus();
}
// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
// Devuelve el pid del proceso con mas prioridad de las colas.
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;

	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0) {
		selectedProcess=OperatingSystem_ExtractFromReadyToRun(USERPROCESSQUEUE);
	}
	else {
		selectedProcess=OperatingSystem_ExtractFromReadyToRun(DAEMONSQUEUE);
	}
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun(int queueID) {
  
	int selectedProcess=NOPROCESS;
	
	selectedProcess=Heap_poll(readyToRunQueue[queueID],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[queueID]);
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}

// Return PID of more priority process in the SLEEPING queue
int OperatingSystem_ExtractFromBlockedToReady() {
	int selectedProcess = NOPROCESS;

	selectedProcess = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);

	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	int previousState = processTable[PID].state;

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	processTable[PID].state=EXECUTING;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[previousState],statesNames[EXECUTING]);
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	Processor_SetAccumulator(processTable[PID].copyOfAcumulator);
	
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);
	
	// Save Accumulator to PCB
	processTable[PID].copyOfAcumulator = Processor_GetAccumulator();

}


// Exception management routine
void OperatingSystem_HandleException() {
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(71,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
	
	OperatingSystem_TerminateProcess();
	OperatingSystem_PrintStatus();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	int selectedProcess;
  	int previousState = processTable[executingProcessID].state;
	processTable[executingProcessID].state=EXIT;
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,statesNames[previousState],statesNames[EXIT]);

	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	if (numberOfNotTerminatedUserProcesses==0 && numberOfProgramsInArrivalTimeQueue <= 0) {
		if (executingProcessID==sipID) {
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99,SHUTDOWN,"The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;

	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	// int previousPID = executingProcessID; 
	// int queue = processTable[executingProcessID].queueID;

	int programID;

	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			OperatingSystem_PrintStatus();
			break;
		case SYSCALL_YIELD:
			// int previousPID = executingProcessID; 
			// int queue = processTable[executingProcessID].queueID;
			// if (processTable[executingProcessID].priority == readyToRunQueue[queueID][0].)
			
			programID = Heap_getFirst(readyToRunQueue[processTable[executingProcessID].queueID],numberOfReadyToRunProcesses[processTable[executingProcessID].queueID]);

			if (processTable[executingProcessID].priority == processTable[programID].priority) {
				// executingProcessID = readyToRunQueue[queue][0].info;
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				ComputerSystem_DebugMessage(115,SHORTTERMSCHEDULE,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,programID,programList[processTable[programID].programListIndex]->executableName);
				OperatingSystem_PreemptRunningProcess();
				OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
				OperatingSystem_PrintStatus();
			}
			break;
		case SYSCALL_SLEEP:
			OperatingSystem_MoveToTheBLOCKEDState(executingProcessID);
			OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
			OperatingSystem_PrintStatus();
			break;
	}
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();

			break;
		case CLOCKINT_BIT: // CLOCKINT_BIT=9
			numberOfClockInterrupts++;
			OperatingSystem_HandleClockInterrupt();
			break;
	}

}


void OperatingSystem_PrintReadyToRunQueue() {
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106,SHORTTERMSCHEDULE,"Ready-to-run processes queues:");

	// if (numberOfReadyToRunProcesses == 1) {
	// 	ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE,readyToRunQueue[0].info,processTable[readyToRunQueue[0].info].priority);
	// 	return;
	// }
	
	ComputerSystem_DebugMessage(112,SHORTTERMSCHEDULE);
	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] == 0) {
		ComputerSystem_DebugMessage(109,SHORTTERMSCHEDULE);
	}
	else if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] <= 1) {
		ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE,readyToRunQueue[USERPROCESSQUEUE][0].info,processTable[readyToRunQueue[USERPROCESSQUEUE][0].info].priority);
	} else {
		for (int i=0;i<numberOfReadyToRunProcesses[USERPROCESSQUEUE];i++) {
			if (i == numberOfReadyToRunProcesses[USERPROCESSQUEUE]-1) { 
				ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE,readyToRunQueue[USERPROCESSQUEUE][i].info,processTable[readyToRunQueue[USERPROCESSQUEUE][i].info].priority);
			} 
			else {
				ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE,readyToRunQueue[USERPROCESSQUEUE][i].info,processTable[readyToRunQueue[USERPROCESSQUEUE][i].info].priority);
			}
		}
	}

	ComputerSystem_DebugMessage(113,SHORTTERMSCHEDULE);
	if (numberOfReadyToRunProcesses[DAEMONSQUEUE] == 0) {
		ComputerSystem_DebugMessage(109,SHORTTERMSCHEDULE);
	}
	else if (numberOfReadyToRunProcesses[DAEMONSQUEUE] <= 1) {
		ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE,readyToRunQueue[DAEMONSQUEUE][0].info,processTable[readyToRunQueue[DAEMONSQUEUE][0].info].priority);
	} else { 
		for (int i=0;i<numberOfReadyToRunProcesses[DAEMONSQUEUE];i++) {
			if (i == numberOfReadyToRunProcesses[DAEMONSQUEUE]-1) { 
				ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE,readyToRunQueue[DAEMONSQUEUE][i].info,processTable[readyToRunQueue[DAEMONSQUEUE][i].info].priority);
			} 
			else {
				ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE,readyToRunQueue[DAEMONSQUEUE][i].info,processTable[readyToRunQueue[DAEMONSQUEUE][i].info].priority);
			}
		}
	}

}

// Method that compares 2 processes priority and returns if the first process given as parameter (a)
// has more priority that the second one (b)
int comparePrioritys(int a, int b)
{
	if (processTable[a].queueID == USERPROCESSQUEUE && processTable[b].queueID == DAEMONSQUEUE)
		return 1;
	if (processTable[a].queueID == processTable[b].queueID && processTable[a].priority < processTable[b].priority)
		return 1;

	return 0;
}

void OperatingSystem_HandleClockInterrupt(){ 
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120,INTERRUPT,numberOfClockInterrupts);

	//int procLeft = numberOfSleepingProcesses;
	int process = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses);
	int counter = 0;
	while (processTable[process].whenToWakeUp == numberOfClockInterrupts) {
		OperatingSystem_MoveToTheREADYState(Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses));
		process = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses);
		counter++;
	}

	int newProcess = OperatingSystem_LongTermScheduler();

	if (counter > 0 || newProcess > 0) {
		for (int i=0;i<=processTable[executingProcessID].queueID;i++) {
			int proceso = Heap_getFirst(readyToRunQueue[i], numberOfReadyToRunProcesses[i]);
			if (comparePrioritys(proceso, executingProcessID))
			{
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, proceso, programList[processTable[proceso].programListIndex]->executableName);
				OperatingSystem_PreemptRunningProcess();
				OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
				OperatingSystem_PrintStatus();
				break;
			}
		}
	} else {
		if (numberOfNotTerminatedUserProcesses <= 0 && numberOfProgramsInArrivalTimeQueue <= 0) {
			OperatingSystem_ReadyToShutdown();
		}
	}

	return; 	
}

int OperatingSystem_GetExecutingProcessID() {
	return executingProcessID;
}




