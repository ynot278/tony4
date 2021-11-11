#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>

#include "oss.h"

static int seconds = MAX_REAL_SECONDS;

static unsigned int logLines = 0;
const char *logFile = "ouput.txt";

static int shmid = -1;
static int queueID = -1;
static struct shmem *shm = NULL;

struct queue{
	unsigned int id[MAX_PROCESS];
	unsigned int length;
};
static struct queue processQueue[queueCount];

static unsigned int usersStart = 0;
static unsigned int usersDel = 0;
static unsigned int usersBlock = 0;
static unsigned int id = 1;

static struct timespec createNext = {.tv_sec = 0, .tv_nsec = 0};

static struct timespec CPUIdleTime = {.tv_sec = 0, .tv_nsec = 0};
static struct timespec schedulerTime = {.tv_sec = 0, .tv_nsec = 0};
static struct timespec schedulerWaitTime = {.tv_sec = 0, .tv_nsec = 0};

static const struct timespec maxTimeBetweenNewProcsSecs = {.tv_sec = 1};
static const struct timespec maxTimeBetweenNewProcsNS = {.tv_nsec = 1};

static unsigned char bitMap[MAX_PROCESS / 8];

static void printHelp(){
	printf("oss [-h] [-s t] [-l f]\n");
	printf("-h Describe how the project should be run and then, terminate.\n");
	printf("-s t Indicate how many maximum seconds before the system terminates.\n");
	printf("-l f Specify a particular name for the log file.\n");
}

static int bitMapBytes(const int byte, const int num){
	return (byte & (1 << num)) >> num;
}

static void bitMapSwitch(const int num){
	int byte = num / 8;
	int mask = 1 << (num % 8);

	bitMap[byte] ^= mask;
}

static int freeBitMap(){
	int i;
	for(i = 0; i < MAX_PROCESS; ++i){
		int byte = i / 8;
		int bit = i % 8;

		if(bitMapBytes(bitMap[byte], bit) == 0){
			bitMapSwitch(i);
			return i;
		}
	}
	return -1;
}

static int createSHM(){
	const key_t key = ftok("main.c", shmKey);

	if(key == -1){
		perror("./oss error: key ftok shmKey");
		return -1;
	}

	shmid = shmget(key, sizeof(struct shmem), IPC_CREAT | IPC_EXCL | 0666);
	if(shmid == -1){
		perror("./oss error: shmget shmid");
		return -1;
	}

	shm = (struct shmem *)shmat(shmid, NULL, 0);
	if(shm == (void *) -1){
		perror("./oss error: shmat shm");
		return -1;
	}

	key_t msggerKey = ftok("main.c", queueKey);
	if(msggerKey == -1){
		perror("./oss error: mssgerKey ftok queueKey");
		return -1;
	}

	queueID = msgget(msggerKey, IPC_CREAT | IPC_EXCL | 0666);
	if(queueID == -1){
		perror("./oss error: msgget queueID");
		return -1;
	}

	return 0;
}

static void removeSHM(){
	if(shm != NULL){
		if(shmdt(shm) == -1){
			perror("./oss error: shm shmdt");
		}
		shm = NULL;
	}

	if(shmid > 0){
		if(shmctl(shmid, IPC_RMID, NULL) == -1){
			perror("./oss error: shmid shmctl");
		}
		shmid = 0;
	}

	if(queueID > 0){
		if(msgctl(queueID, IPC_RMID, NULL) == -1){
			perror("./oss error: queueID msgctl");
		}
		queueID = -1;
	}
}

static int push(const int queueID, const int bit){
	struct queue *q = &processQueue[queueID];
	q->id[q->length++] = bit;
	return queueID;
}

static int pop(struct queue *processQueue, const int index){
	unsigned int i;
	unsigned int user = processQueue->id[index];

	for(i = index; i < processQueue->length - 1; i++){
		processQueue->id[i] = processQueue->id[i + 1];
	}
	return user;
}

static void addTime(struct timespec *timeone, const struct timespec *timetwo){
	static const unsigned int nsMax = 1000000000;

	timeone->tv_sec += timetwo->tv_sec;
	timeone->tv_nsec += timetwo->tv_nsec;
	if (timeone->tv_nsec > nsMax){
		timeone->tv_sec++;
		timeone->tv_nsec -= nsMax;
	}
}

static void subTime(struct timespec *timeone, struct timespec *timetwo, struct timespec *timethree){
  if (timetwo->tv_nsec < timeone->tv_nsec){
    timethree->tv_sec = timetwo->tv_sec - timeone->tv_sec - 1;
    timethree->tv_nsec = timeone->tv_nsec - timetwo->tv_nsec;
  }
  else{
    timethree->tv_sec = timetwo->tv_sec - timeone->tv_sec;
    timethree->tv_nsec = timetwo->tv_nsec - timeone->tv_nsec;
  }
}

static void divTime(struct timespec *timeone, const int d){
  timeone->tv_sec /= d;
  timeone->tv_nsec /= d;
}

static int runUser(){
	struct ossMsgger msg;
	const int process = pop(&processQueue[queueReady], 0);
	processQueue[queueReady].length--;

	struct userProcess *user = &shm->user[process];
	memset(&msg, '\0', sizeof(struct ossMsgger));

	++logLines;
	printf("OSS: Dispatching process with PID %u from queue 0 at time %lu:%lu,\n", user->id, shm->clock.tv_sec, shm->clock.tv_nsec);

	msg.timeslice = 10000000;
	msg.type = user->pid;
	msg.pid = getpid();

	if((msgsnd(queueID, (void *)&msg, (sizeof(struct ossMsgger) - sizeof(long)), 0) == -1) || (msgrcv(queueID, (void *)&msg, (sizeof(struct ossMsgger) - sizeof(long)), getpid(), 0) == -1)){
		perror("./oss error: msgsnd and msgrcv failure");
		return -1;
	}

	const int state = msg.timeslice;
	switch(state){
		case RDY:
			user->state = RDY;

			user->burstTime.tv_sec = 0;
			user->burstTime.tv_nsec = msg.clock.tv_nsec;

			addTime(&shm->clock, &user->burstTime);
			addTime(&user->cpuTime, &user->burstTime);

			++logLines;
			printf("OSS: Receiving that process with PID %u ran for %lu nanoseconds,\n", user->id, user->burstTime.tv_nsec);

			if(msg.clock.tv_nsec != 10000000){
				++logLines;
				printf("OSS: not using its entire time quantum\n");
			}

			push(queueReady, process);
			break;

		case BLOCK:
			user->state = BLOCK;

			user->burstTime.tv_sec = 0;
			user->burstTime.tv_nsec = msg.clock.tv_nsec;

			addTime(&user->cpuTime, &user->burstTime);

			user->blockTime.tv_sec = msg.io.tv_sec;
    	user->blockTime.tv_nsec = msg.io.tv_nsec;

			addTime(&user->blockTime, &shm->clock);

			usersBlock++;
			++logLines;
			printf("OSS: Putting process with PID %u into blocked queue\n", user->id);
			push(queueBlock, process);
			break;

		case TERMINATE:
			user->state = TERMINATE;
    	user->burstTime.tv_sec = 0;
   		user->burstTime.tv_nsec = msg.clock.tv_nsec;
    	addTime(&user->cpuTime, &user->burstTime);

			++logLines;
			printf("OSS: Process with PID %u has been terminated\n", user->id);
			++usersDel;

			subTime(&user->startTime, &shm->clock, &user->totalTime);
			addTime(&schedulerTime, &user->totalTime);

			struct timespec reset;
			subTime(&user->cpuTime, &user->totalTime, &reset);
			addTime(&schedulerWaitTime, &reset);

			memset(user, '\0', sizeof(struct userProcess));

			user->state = NEW;
			bitMapSwitch(process);
			break;

		default:
			printf("OSS: It broke");
			break;
	}
	return 1;
}


static int schedule(){
	static struct timespec idleTime = {.tv_sec = 0, .tv_nsec = 0};
	static int flag = 0;
	struct timespec timeone, timetwo, timethree;

	if(processQueue[queueReady].length == 0){
		idleTime = shm->clock;
		flag = 1;
		return 0;
	} else if(flag == 1){
		flag = 0;
		subTime(&idleTime, &shm->clock, &timetwo);
		addTime(&CPUIdleTime, &timetwo);

		idleTime.tv_sec = 0;
		idleTime.tv_nsec = 0;
	}

	clock_gettime(CLOCK_REALTIME, &timeone);

	runUser();

	clock_gettime(CLOCK_REALTIME, &timeone);

	subTime(&timeone, &timetwo, &timethree);
	addTime(&shm->clock, &timethree);

	++logLines;
	printf("OSS: total time this dispatch was %lu nanoseconds", timethree.tv_nsec);
	return 0;
}

static int startUserProcess(){
	char buf[10];

	const int freeBit = freeBitMap();
	if(freeBit == -1){
		return EXIT_SUCCESS;
	}

	struct userProcess *user = &shm->user[freeBit];

	int IOBound = 0;

	if ((rand() % 100 + 1) <= 25){
		IOBound = 1;
	} else{
		IOBound = 0;
	}

	const pid_t pid = fork();

	switch(pid){
		case 0:
			snprintf(buf, sizeof(buf), "%d", IOBound);
			execl("./user", "./user", buf, NULL);
			perror("./oss error: execl");
			exit(EXIT_FAILURE);
			break;
		
		case -1:
			perror("./oss error: forking");
			return EXIT_FAILURE;
			break;

		default:
			++usersStart;
			user->id = id++;
			user->pid = pid;
			user->startTime = shm->clock;

			user->state = RDY;
			push(queueReady, freeBit);
			++logLines;
			printf("OSS: Generating process with PID %u and putting it in queue 0 at time %lu:%lu\n", user->id, shm->clock.tv_sec, shm->clock.tv_nsec);
			break;
	}

	return EXIT_SUCCESS;
}

static int startTimer(){
	struct timespec timer = {.tv_sec = 1, .tv_nsec = 0};
	addTime(&shm->clock, &timer);

	if((shm->clock.tv_sec >= createNext.tv_sec) || ((shm->clock.tv_sec == createNext.tv_sec) && (shm->clock.tv_nsec >= createNext.tv_nsec))){
		createNext.tv_sec = (rand() % maxTimeBetweenNewProcsSecs.tv_sec);
		createNext.tv_nsec = (rand() % maxTimeBetweenNewProcsNS.tv_nsec);

		addTime(&createNext, &shm->clock);

		if(usersStart < MAX_USERS){
			startUserProcess();
		}
	}
	return 0;
}

static void unblock(){
	unsigned int i;
	for(i = 0; i < processQueue[queueBlock].length; ++i){
		const int user = processQueue[queueBlock].id[i];

		struct userProcess *users = &shm->user[user];

		if(((users->blockTime.tv_sec < shm->clock.tv_sec)) || ((users->blockTime.tv_sec == shm->clock.tv_sec) && (users->blockTime.tv_nsec <= shm->clock.tv_nsec))){
			users->state = RDY;
			users->blockTime.tv_sec = 0;
			users->blockTime.tv_nsec = 0;

			pop(&processQueue[queueBlock], i);
			processQueue[queueBlock].length--;

			push(queueReady, user);
		}
	}
}

static int parseCommands(const int argc, char *const argv[]){
	int opt;

	while ((opt = getopt(argc, argv, "hs:l:")) > 0){
		switch (opt){
			case 'h':
				printHelp();
				return -1;
			
			case 's':
				seconds = atoi(optarg);
				break;

			case 'l':
				logFile = optarg;
				break;

			default:
				printHelp();
				return -1;
		}
	}

	stdout = freopen(logFile, "w,", stdout);
	if(stdout == NULL){
		perror("freopen");
		return -1;
	}

	return 0;
}


int main(const int argc, char *const argv[]){
  if(parseCommands(argc, argv) < 0){
		return EXIT_FAILURE;
	}

	if(createSHM() < 0){
		return EXIT_FAILURE;
	}

	memset(shm, '\0', sizeof(struct shmem));
	memset(bitMap, '\0', sizeof(bitMap));
	memset(processQueue, '\0', sizeof(struct queue) *queueCount);

	while(usersDel < MAX_USERS){
		startTimer();
		unblock();
		schedule();
	}
	
}