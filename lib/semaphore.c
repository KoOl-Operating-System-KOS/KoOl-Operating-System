// User-level Semaphore

#include "inc/lib.h"

struct semaphore create_semaphore(char *semaphoreName, uint32 value)
{
	//TODO: [PROJECT'24.MS3 - #02] [2] USER-LEVEL SEMAPHORE - create_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_semaphore is not implemented yet");
	//Your Code is Here...

	struct semaphore samphoor = smalloc(semaphoreName,sizeof(struct semaphore),1);
	samphoor.semdata = malloc(sizeof(samphoor.semdata));
	samphoor.semdata->count = value;
	// system call for queue functions
	// LIST_INIT(&samphoor.semdata->queue);

	return samphoor;
}

struct semaphore get_semaphore(int32 ownerEnvID, char* semaphoreName)
{
	//TODO: [PROJECT'24.MS3 - #03] [2] USER-LEVEL SEMAPHORE - get_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("get_semaphore is not implemented yet");
	//Your Code is Here...

	struct semaphore samphoor = get_share(ownerEnvID,semaphoreName);
	return samphoor;
}

void wait_semaphore(struct semaphore sem)
{
	//TODO: [PROJECT'24.MS3 - #04] [2] USER-LEVEL SEMAPHORE - wait_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("wait_semaphore is not implemented yet");
	//Your Code is Here...

	int keyw =1;
	while(keyw != 0){
	     xchg(&keyw,&sem.lock);
	     semaphore_count(sem)--;
	     if(semaphore_count(sem) < 0){
	    	 //put process in sem.queue.
	    	 //block process.
	     }
	     sem.lock =0;
	}
}

void signal_semaphore(struct semaphore sem)
{
	//TODO: [PROJECT'24.MS3 - #05] [2] USER-LEVEL SEMAPHORE - signal_semaphore
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("signal_semaphore is not implemented yet");
	//Your Code is Here...
    int keys = 1;
    while(keys != 0){
         xchg(&keys,&sem.lock);
		 semaphore_count(sem)++;
         if(semaphore_count(sem) <= 0){
        	//remove process from queue.
        	//place process on ready list.
         }
         sem.lock = 0;
    }
}

int semaphore_count(struct semaphore sem)
{
	return sem.semdata->count;
}
