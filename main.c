#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAX_REAL_SECONDS 3
#define MAX_USERS 50

static int seconds = MAX_REAL_SECONDS;
const char *logFile = "ouput.txt";

static void printHelp(){
	printf("oss [-h] [-s t] [-l f]\n");
	printf("-h Describe how the project should be run and then, terminate.\n");
	printf("-s t Indicate how many maximum seconds before the system terminates.\n");
	printf("-l f Specify a particular name for the log file.\n");
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
  if ((parseCommands(argc, argv) < 0)){
		return EXIT_FAILURE;
	}
}