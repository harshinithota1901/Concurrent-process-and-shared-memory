#include <unistd.h>

//The variables shared between master and palin processes
struct shared {
	int sec;
	int ns;
	int mylist_len;
	char mylist[1];	//\0 delimited array of strings
};

//shared memory constants
#define FTOK_SEM_PATH "/tmp"
#define FTOK_SHM_PATH "/tmp"
#define FTOK_SEM_KEY 6776
#define FTOK_SHM_KEY 7667

//Filenames for results
#define PALIN_FILENAME "palin.out"
#define NOPALIN_FILENAME "nopalin.out"

//macros for the 3 critical section - shared, file palin, file nopalin
enum CRIT_RES { CR_SHM=0, CR_FPALIN, CR_FNOPALIN };

//Return the string at index
const char * my_list_at(const char * mylist, const int index);

//Critical section enter/exit functions
int crit_enter(const int semid, const enum CRIT_RES id);
int crit_exit(const int semid, const enum CRIT_RES id);
