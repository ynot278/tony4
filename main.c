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


#define MAX_REAL_SECONDS 3
#define MAX_USERS 50

static int seconds = MAX_REAL_SECONDS;

const char *logFile = "ouput.txt";

struct userProcess{
	pid_t pid;
	unsigned int id;
	enum processState state;

	struct timespec cpuTime;
	struct timespec totalTime;
	struct timespec burstTime;
	struct timespec blockTime;
	struct timespec startTime;
};

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

static struct timespec createNext = {.tv_sec = 0, .tv_nsec = 0};

static const struct timespec maxTimeBetweenNewProcsSecs = {.tv_sec = 1};
static const struct timespec maxTimeBetweenNewProcsNS = {.tv_nsec = 1};

static void printHelp(){
	printf("oss [-h] [-s t] [-l f]\n");
	printf("-h Describe how the project should be run and then, terminate.\n");
	printf("-s t Indicate how many maximum seconds before the system terminates.\n");
	printf("-l f Specify a particular name for the log file.\n");
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

static void addTime(struct timespec *first, const struct timespec *second){
	static const unsigned int nsMax = 1000000000;

	first->tv_sec += second->tv_sec;
	first->tv_nsec += second->tv_nsec;
	if (first->tv_nsec > nsMax){
		first->tv_sec++;
		first->tv_nsec -= nsMax;
	}
}

static int startTimer(){
	struct timespec timer = {.tv_sec = 1, .tv_nsec = 0};
	addTime(&shm->clock, &timer);

	if((shm->clock.tv_sec >= createNext.tv_sec) || ((shm->clock.tv_sec == createNext.tv_sec) && (shm->clock.tv_nsec >= createNext.tv_nsec))){
		createNext.tv_sec = (rand() % maxTimeBetweenNewProcsSecs.tv_sec);
		createNext.tv_nsec = (rand() % maxTimeBetweenNewProcsNS.tv_nsec);

		addTime(&createNext, &shm->clock);

		if(usersStart < MAX_USERS){
			
		}

	}
	return 0;
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
	memset(processQueue, '\0', sizeof(struct queue) *queueCount);

	while(usersDel < MAX_USERS){

	}
	
}