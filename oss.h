#ifndef OSS_H
#define OSS_H

#define MAX_PROCESS 18
#define MAX_REAL_SECONDS 3
#define MAX_USERS 50

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

struct shmem{
	struct timespec clock;
	struct userProcess user[MAX_PROCESS];
};

struct ossMsgger{
	long type;
	pid_t pid;

	int timeslice;

	struct timespec clock;
	struct timespec io;
};


#endif