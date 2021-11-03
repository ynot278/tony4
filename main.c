#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
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

static int shmid = -1;
static int queueID = -1;
static struct shmem *shm = NULL;

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
}