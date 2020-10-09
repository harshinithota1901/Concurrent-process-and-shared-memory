#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "palindrome.h"

static int shmid = -1, semid = -1;  
static struct shared * shmp = NULL;

//Initialize the shared memory pointer
static int shared_initialize()
{
	key_t key = ftok(FTOK_SEM_PATH, FTOK_SHM_KEY);  //get a key for the shared memory
	if(key == -1){
		perror("ftok");
		return -1;
	}

	shmid = shmget(key, 0, 0);
	if(shmid == -1){
		perror("shmget");
		return -1;
	}

  shmp = (struct shared*) shmat(shmid, NULL, 0); //attach it
  if(shmp == NULL){
		perror("shmat");
		return -1;
	}

  key = ftok(FTOK_SHM_PATH, FTOK_SEM_KEY);
	if(key == -1){
		perror("ftok");
		return -1;
	}

  semid = semget(key, 3, 0);
	if(semid == -1){
    fprintf(stderr, "Error: Failed to create semaphore with key 0x%x\n", key);
		perror("semget");
		return -1;
	}

	return 0;
}

//Check if word is a palindrome
static int palindrome_check(const char * word){
	int i;
	const size_t len = strlen(word);

	for(i=len / 2; i >=0 ; --i){
		if(word[i] != word[len-i-1]){
			return 0;
		}
	}
	return 1;
}

//Save a palindrome word to result fule
static int save_word(const char * filename, enum CRIT_RES crit_id, const char * word){
	/* Critical section */
	crit_enter(semid, crit_id);

	sleep(2);

	FILE * fout = fopen(filename, "a");
	if(fout == NULL){
		perror("fopen");

	}else{
		fprintf(fout, "%s\n", word);
		fclose(fout);
	}

	sleep(2);
	crit_exit(semid, crit_id);
	return 0;
}

//Print a message, in synchronized way
static void crit_printf(const char * msg){
	crit_enter(semid, CR_SHM);
	fprintf(stderr, "[%i:%i] Palin %i %s\n", shmp->sec, shmp->ns, getpid(), msg);
	crit_exit(semid, CR_SHM);	//exit critical section
}

int main(const int argc, char * const argv[]){

	if(argc != 2){
		fprintf(stderr, "Usage: palin xx\n");
		return EXIT_FAILURE;
	}

	if(shared_initialize() < 0){
		return EXIT_FAILURE;
	}

	//initialize the rand() function
	srand(getpid());

	//read oss clock and add duration
	const int xx = atoi(argv[1]);
	const char * word = my_list_at(shmp->mylist, xx);

	sleep(rand() % 3);

	crit_printf("before critical section");

	if(palindrome_check(word)){
		save_word(PALIN_FILENAME, CR_FPALIN, word);
	}else{
		save_word(NOPALIN_FILENAME, CR_FNOPALIN, word);
	}

	crit_printf("after critical section");

	shmdt(shmp);
	return EXIT_SUCCESS;
}
