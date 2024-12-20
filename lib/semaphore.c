// User-level Semaphore

#include "inc/lib.h"

struct semaphore create_semaphore(char *semaphoreName, uint32 value)
{
    //TODO: [PROJECT'24.MS3 - #02] [2] USER-LEVEL SEMAPHORE - create_semaphore
    //COMMENT THE FOLLOWING LINE BEFORE START CODING
    //panic("create_semaphore is not implemented yet");
    //Your Code is Here...

    struct semaphore samphoor;
    samphoor.semdata = smalloc(semaphoreName,sizeof(struct __semdata),1);
    samphoor.semdata->count = value;
    samphoor.semdata->lock=0;
    strcpy(samphoor.semdata->name, semaphoreName);
    // system call for queue functions to initialize the queue inside the semdata
    sys_queue_initialize(&samphoor.semdata->queue);

    return samphoor;
}

struct semaphore get_semaphore(int32 ownerEnvID, char* semaphoreName)
{
	//TODO: [PROJECT'24.MS3 - #03] [2] USER-LEVEL SEMAPHORE - get_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("get_semaphore is not implemented yet");
	//Your Code is Here...

	struct semaphore samphoor;
	samphoor.semdata = (struct __semdata*) sget(ownerEnvID,semaphoreName);

	return samphoor;
}

void wait_semaphore(struct semaphore sem)
{
	//TODO: [PROJECT'24.MS3 - #04] [2] USER-LEVEL SEMAPHORE - wait_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("wait_semaphore is not implemented yet");
	//Your Code is Here...

	int keyw = 1;
	while(xchg(&(sem.semdata->lock),keyw) != 0);
	sem.semdata->count--;
	 if(semaphore_count(sem) < 0){
	    struct Env* env = sys_getCurrentProc();
	    sys_proc_enqueue_block(env,&sem.semdata->queue);
	    sem.semdata->lock =0;
	  }
      sem.semdata->lock =0;
}

void signal_semaphore(struct semaphore sem)
{
	//TODO: [PROJECT'24.MS3 - #05] [2] USER-LEVEL SEMAPHORE - signal_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("signal_semaphore is not implemented yet");
	//Your Code is Here...
    int keys = 1;
    while(xchg(&(sem.semdata->lock),keys) != 0)
    sem.semdata->count++;
    if(semaphore_count(sem) <= 0){
       sys_proc_dequeue_ready(&sem.semdata->queue);
       sem.semdata->lock = 0;
    }
    sem.semdata->lock = 0;
}

int semaphore_count(struct semaphore sem)
{
	return sem.semdata->count;
}
