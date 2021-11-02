#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "oss.h"
#include "user.h"

static int sid = -1;
static int qid = -1;

static struct shmem *createSHM(){
	struct shmem *shm = NULL;

	key_t key = ftok("main.c", shmKey);
	if (key == -1){
		perror("./user.c error: key ftok shmKey");
		return NULL;
	}

	sid = shmget(key, sizeof(struct shmem), 0);
	if (sid == -1){
		perror("./user.c error: shmget");
		return NULL;
	}

	shm = (struct shmem *)shmat(sid, NULL, 0);
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

int main(const int argc, char *const argv[]){
	if (argc != 2){
		perror("user.c error: Args: ./user [IO_BOUND = 0 or 1]\n");
		return EXIT_FAILURE;
	}

	const int isIO = atoi(argv[1]);

	srand(getpid() + isIO);

	struct shmem *shm = createSHM();
	if (shm == NULL){
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}