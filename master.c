#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include "palindrome.h"

//maximum time to run
#define MAX_RUNTIME 100
//maximum children to create
#define MAX_CHILDREN 20

static unsigned int arg_n = 4;
static unsigned int arg_s = 2;
static unsigned int arg_t = MAX_RUNTIME;

static pid_t childpids[MAX_CHILDREN];  //array for user pids
static unsigned int N=0, S=0, X=0;       //child counters for started, running, terminated
static int shmid=-1, semid=-1;    //shared memory ids
static unsigned int interrupted = 0;

static FILE * input = NULL; //input file
static FILE * output = NULL;
static struct shared * shmp = NULL; //pointer to shared memory

//Called when we receive a signal
static void sign_handler(const int sig)
{
  interrupted = 1;
  crit_enter(semid, CR_SHM);
	fprintf(output, "[%i:%i] Signal %i received\n", shmp->sec, shmp->ns, sig);
	crit_exit(semid, CR_SHM);	//exit critical section
}

//Create a child process
static pid_t master_fork(const char *prog, const unsigned int xx)
{
  char arg[3];  //up to 20 children, so max arg len is 2 + 1 for \0
  snprintf(arg, sizeof(arg), "%u", xx);

	const pid_t pid = fork();  //create process
	if(pid < 0){
		perror("fork");
		return -1;

	}else if(pid == 0){
    //run the specified program
		execl(prog, prog, arg, NULL);
		perror("execl");
		exit(1);

	}else{
    //save child pid
		childpids[N++] = pid;
	}
	return pid;
}

//Wait for all processes to exit
static void master_waitall()
{
  int i;
  for(i=0; i < N; ++i){ //for each process
    if(childpids[i] == 0){  //if pid is zero, process doesn't exist
      continue;
    }

    int status;
    if(waitpid(childpids[i], &status, WNOHANG) > 0){
      --S;
      ++X;

      if (WIFEXITED(status)) {  //if process exited
        crit_enter(semid, CR_SHM);
        fprintf(output,"[%i:%i] child %u terminated with exit code %i\n",
          shmp->sec, shmp->ns, childpids[i], WEXITSTATUS(status));
      	crit_exit(semid, CR_SHM);	//exit critical section

      }else if(WIFSIGNALED(status)){  //if process was signalled
        crit_enter(semid, CR_SHM);
        fprintf(output,"[%i:%i] child %u killed (signal %i)\n",
          shmp->sec, shmp->ns, childpids[i], WTERMSIG(status));
        crit_exit(semid, CR_SHM);	//exit critical section
      }
      childpids[i] = 0;
    }
  }
}

//Called at end to cleanup all resources and exit
static void master_exit(const int ret)
{
  //tell all users to terminate
  int i;
  for(i=0; i < N; i++){
    if(childpids[i] <= 0){
      continue;
    }
  	kill(childpids[i], SIGTERM);
  }
  master_waitall();

	if(input){
    fclose(input);
  }

  if(shmp){
    shmdt(shmp);
    shmctl(shmid, IPC_RMID, NULL);
  }

  if(semid > 0){
    semctl(semid, 0, IPC_RMID);
  }

  fclose(output);
	exit(ret);
}

//Move time forward
static void update_timer(struct shared *shmp)
{

  //time is shared, so we need to lock/unlock
  crit_enter(semid, CR_SHM);

  shmp->ns += 100;
	if(shmp->ns > 1000000000){ //nanosecond in 1 second
		shmp->sec++;
		shmp->ns = 0;
	}

  crit_exit(semid, CR_SHM);
}

//Process program options
static int update_options(const int argc, char * const argv[])
{

  int opt;
	while((opt=getopt(argc, argv, "hn:s:t:")) != -1){
		switch(opt){
			case 'h':
				fprintf(output,"Usage: master [-h]\n");
        fprintf(output,"Usage: master [-n x] [-s x] [-t time] infile\n");
				fprintf(output," -h Describe program options\n");
				fprintf(output," -n x Total of child processes (Default is 4)\n");
        fprintf(output," -s x Children running together (Default is 2)\n");
        fprintf(output," -t x Maximum runtime (Default is 100)\n");
				fprintf(output," infile Input filename\n");

				return 1;

      case 'n':
        arg_n	= atoi(optarg); //convert value -n from string to int
        break;

      case 's':
        arg_s	= atoi(optarg) % MAX_CHILDREN;
        break;

      case 't':
        arg_t	= atoi(optarg);
        break;

      case 'i':
        //open input file
				input = fopen(optarg, "r");
				if(input == NULL){
					perror("fopen");
					return -1;
				}
				break;

			default:
				fprintf(output, "Error: Invalid option '%c'\n", opt);
				return -1;
		}
	}

	if(input == NULL){ //if input file wasn't specified
		input = fopen("infile.txt", "r"); //open default input file
		if(input == NULL){
			perror("fopen");
			return -1;
		}
	}
  return 0;
}

//Return file size
static long get_file_size(FILE * f)
{
  fseek(input, 0L, SEEK_END);
  const long size = ftell(input);
  rewind(input);
  return size;
}

//Create a file
static int create_file(const char * name)
{
  FILE * f = fopen(name, "w");
  if(f == NULL){
    perror("fopen");
    return -1;
  }
  fclose(f);
  return 0;
}

//Initialize the shared memory
static int shared_initialize()
{
  key_t key = ftok(FTOK_SEM_PATH, FTOK_SHM_KEY);  //get a key for the shared memory
	if(key == -1){
		perror("ftok");
		return -1;
	}

  const long shared_size = get_file_size(input)
                         + sizeof(struct shared);


	shmid = shmget(key, shared_size, IPC_CREAT | IPC_EXCL | S_IRWXU);
	if(shmid == -1){
		perror("shmget");
		return -1;
	}

  shmp = (struct shared*) shmat(shmid, NULL, 0);
  if(shmp == NULL){
		perror("shmat");
		return -1;
	}

  key = ftok(FTOK_SHM_PATH, FTOK_SEM_KEY);
	if(key == -1){
		perror("ftok");
		return -1;
	}

  semid = semget(key, 3, IPC_CREAT | IPC_EXCL | S_IRWXU);
	if(semid == -1){
    fprintf(output, "Error: Failed to create semaphore with key 0x%x\n", key);
		perror("semget");
		return -1;
	}

  union semun un;
	un.val = 1;

  int i;
  for(i=0; i < 3; i++){
  	if(semctl(semid, i, SETVAL, un) ==-1){
  		perror("semid");
  		return -1;
  	}
  }
  return 0;
}

//Load the input data to shared memory
static int master_load()
{

  char * line = NULL;
  size_t lsize = 0;
  int total_len = 0, len;

  //read input file, until end
  while((len = getline(&line, &lsize, input)) > 0){

    line[len-1] = '\0'; //each palindrome ends with NULL

    //copy string to shared memory
    strncpy(&shmp->mylist[total_len], line, len);
    total_len += len;

    shmp->mylist_len++;
  }
  free(line);

  shmp->mylist[total_len] = EOF; //mark end of all strings

  crit_enter(semid, CR_SHM);	//exit critical section
  fprintf(output,"[%i:%i] Master loaded %d word\n", shmp->sec, shmp->ns, shmp->mylist_len);
  crit_exit(semid, CR_SHM);	//exit critical section

  return shmp->mylist_len;
}

//Initialize the master process
static int master_initialize()
{

  if(shared_initialize() < 0){
    return -1;
  }

  //clear timer and pid
  bzero(childpids, sizeof(pid_t)*MAX_CHILDREN);
  shmp->sec	= 0;
	shmp->ns	= 0;

  //create result files
  create_file(PALIN_FILENAME);
  create_file(NOPALIN_FILENAME);

  //load strings
  return master_load();
}

int main(const int argc, char * const argv[])
{

  output = fopen("output.log", "w");
  if(output == NULL){
    perror("fopen");
    return 1;
  }

  if(update_options(argc, argv) < 0){
    master_exit(1);
  }

  //signal(SIGCHLD, master_waitall);
  signal(SIGTERM, sign_handler);
  signal(SIGALRM, sign_handler);
  alarm(arg_t);

  const int xx_max = master_initialize();
  if(xx_max <= 0){
    master_exit(1);
  }

  //determine max string index
  int xx = 0;
  const int x_max = (xx_max >= arg_n) ? arg_n : xx_max;

  //run until interrupted or maximum index reached
  while(!interrupted && (X < x_max)){

    //if we have strings, if we can start another process, and car run another process
    if( (xx < xx_max) && (N < arg_n) && (S < arg_s)){
      const pid_t pid = master_fork("./palin", xx);

      crit_enter(semid, CR_SHM);	//exit critical section
      fprintf(output,"[%d:%d] Started Palin '%i %d' for %s\n", shmp->sec, shmp->ns, pid, xx, my_list_at(shmp->mylist, xx));
      crit_exit(semid, CR_SHM);	//exit critical section
      ++S;
      ++xx;
    }else{
      //can't start more processes, take some sleep
      usleep(10);
    }

		update_timer(shmp);
    master_waitall();  //clean any zombies
	}

  fprintf(output,"[%i:%i] Master exit\n", shmp->sec, shmp->ns);
	master_exit(0);

	return 0;
}
