/*
* channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>


//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct spinlock* lk)
{
	//TODO: [PROJECT'24.MS1 - #10] [4] LOCKS - sleep
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("sleep is not implemented yet");
	//Your Code is Here...

	// struct spinlock queueslock = ProcessQueues.qlock;

	// struct spinlock *queueslock_ptr = &queueslock;

	acquire_spinlock(&ProcessQueues.qlock); //firstly acquire the spin lock to protect the blocked queue

	struct Env *current = get_cpu_proc();  //getting the process running

	release_spinlock(lk);            //release the spin lock after protection

	current->env_status = ENV_BLOCKED;

	enqueue(&chan->queue,current);

	sched();

	release_spinlock(&ProcessQueues.qlock);

	acquire_spinlock(lk); //?????

}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'24.MS1 - #11] [4] LOCKS - wakeup_one
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("wakeup_one is not implemented yet");
	//Your Code is Here...

	//struct spinlock queueslock = ProcessQueues.qlock;

	//struct spinlock *queueslock_ptr = &queueslock;

	acquire_spinlock(&ProcessQueues.qlock);

	struct Env *CurrentEnv = dequeue(&chan->queue);

	if(CurrentEnv!=NULL){

		CurrentEnv->env_status=ENV_READY;

		sched_insert_ready0(CurrentEnv);

	}

	release_spinlock(&ProcessQueues.qlock);


}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'24.MS1 - #12] [4] LOCKS - wakeup_all
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("wakeup_all is not implemented yet");
	//Your Code is Here...

	//struct spinlock queueslock = ProcessQueues.qlock;

	//struct spinlock *queueslock_ptr = &queueslock;

	acquire_spinlock(&ProcessQueues.qlock);


	while(queue_size(&chan->queue)!=0){

		struct Env *CurrentEnv = dequeue(&chan->queue);

		if(CurrentEnv!=NULL){

			CurrentEnv->env_status=ENV_READY;

			sched_insert_ready0(CurrentEnv);


		}
	}

	release_spinlock(&ProcessQueues.qlock);
}

