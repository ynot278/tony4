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
#include "user.h"

static int shmid = -1;
static int qid = -1;

static struct shmem *createSHM(){
	struct shmem *shm = NULL;

	key_t key = ftok("main.c", shmKey);
	if (key == -1){
		perror("./user.c error: key ftok shmKey");
		return NULL;
	}

	shmid = shmget(key, sizeof(struct shmem), 0);
	if (shmid == -1){
		perror("./user.c error: shmget");
		return NULL;
	}

	shm = (struct shmem *)shmat(shmid, NULL, 0);
	if (shm == (void *)-1){
		perror("./user.c error: shmat");
		return NULL;
	}

	key = ftok("main.c", queueKey);
	if (key == -1){
		perror("./user.c error: queue ftok key queue");
		return NULL;
	}

	qid = msgget(key, 0);
	if (qid == -1){
		perror("./user.c error: msgget");
		return NULL;
	}

	return shm;
}

static int removeSHM(struct shmem *shm){
	if(shmdt(shm) == -1){
		perror("./user.c error: shmdt shm");
		return -1;
	}
	return 0;
}

static int simulation(const int isIO){

	int inIO = 1;
	int blockChance;

	if(isIO){
		blockChance = 25;
	} else{
		blockChance = 5;
	}

	while(inIO){
		struct ossMsgger msg;

		msg.pid = getpid();
		if(msgrcv(qid, (void *)&msg, (sizeof(struct ossMsgger) - sizeof(long)), msg.pid, 0) == -1){
			perror("./user error: msgrcv failed");
			break;
		}

		const int timeslice = msg.timeslice;
		if(timeslice == 0){
			break;
		}

		memset(&msg, '\0', sizeof(struct shmem));

	  int terminate;

	  if ((rand() % 100 + 1) <= 10){
			terminate = 1;
		} else{
			terminate = 0;
		}

		if(terminate){
			msg.timeslice = TERMINATE;
			msg.clock.tv_nsec = rand() % timeslice;
			inIO = 0;
		} else{
			int interrupt;
			
			if ((rand() % 100 + 1) < blockChance){
				interrupt = 1;
			} else{
				interrupt = 0;
			}

			if(interrupt){
				msg.timeslice = BLOCK;
				msg.clock.tv_nsec = rand() % timeslice;
				msg.io.tv_sec = rand() % 5 + 1;
				msg.io.tv_nsec = rand() % 1000 + 1;
			} else{
				msg.timeslice = RDY;
				msg.clock.tv_nsec = timeslice;
			}
		}

		msg.type = getppid();
		msg.pid = getpid();
		if(msgsnd(qid, (void *)&msg, (sizeof(struct ossMsgger) - sizeof(long)), 0) == -1){
			perror("./user error: msgsnd error");
			break;
		}
	}
	return 0;
}

int main(const int argc, char *const argv[]){
	if (argc != 2){
		perror("user.c error: Args: ./user [IO Bound = 0 or 1]\n");
		return EXIT_FAILURE;
	}

	const int isIO = atoi(argv[1]);

	srand(getpid() + isIO);

	struct shmem *shm = createSHM();
	if (shm == NULL){
		return EXIT_FAILURE;
	}

	simulation(isIO);

	if(shmdt(shm) == -1){
		perror("./user.c error: shmdt shm");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}