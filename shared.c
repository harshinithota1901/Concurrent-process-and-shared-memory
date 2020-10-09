#include <stdio.h>
#include <errno.h>
#include <sys/sem.h>
#include "palindrome.h"

static int mysemop(const int semid, const int op, enum CRIT_RES n){
  static struct sembuf sops;

  sops.sem_num = n;
  sops.sem_flg = 0;
	sops.sem_op  = op;

  while(semop(semid, &sops, 1) != 0) {
    if(errno != EINTR){
  	  perror("semop");
  	  return -1;
    }
	}
  return 0;
};

//Enter critical section, by locking the specified semaphore
int crit_enter(const int semid, enum CRIT_RES n){
  return mysemop(semid, -1, n);
}

//Exit critical section, by unlocking the specified semaphore
int crit_exit(const int semid, enum CRIT_RES n){
  return mysemop(semid, 1, n);
}

//Return string at index from shared strings
const char * my_list_at(const char * mylist, const int index){
  int xx=0, i = 0;

  //read until EOF is reached
  while(mylist[i] != EOF){

    if(xx == index){  //if index matches
      return &mylist[i];
    }

    if(mylist[i] == '\0'){  //if we are at end of string
      ++xx;//increase string index 
    }
    ++i;
  }
  return NULL;
}
