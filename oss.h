#ifndef OSS_H
#define OSS_H

#define MAX_PROCESS 18

enum ftokKeys{
	shmKey = 1000, queueKey		 
};

enum queueStatus{
	queueReady = 0,
	queueBlock,
	queueCount
};

enum processState{
	NEW = 0,
	RDY,
	BLOCK,
	TERMINATE
};

struct shmem{
	struct timespec clock;
	struct userProcess user[MAX_PROCESS];
};


#endif