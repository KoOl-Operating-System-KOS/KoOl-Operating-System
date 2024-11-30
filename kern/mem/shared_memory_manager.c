#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
struct Share* get_share(int32 ownerID, char* name);

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_spinlock(&AllShares.shareslock, "shares lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [2] Get Size of Share Object:
//==============================
int getSizeOfSharedObject(int32 ownerID, char* shareName)
{
	//[PROJECT'24.MS2] DONE
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = get_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}

//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] Create frames_storage:
//===========================
// Create the frames_storage and initialize it by 0
inline struct FrameInfo** create_frames_storage(int numOfFrames)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_frames_storage()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_frames_storage is not implemented yet");
	//Your Code is Here...

    struct FrameInfo** arrFramesInfo = (struct FrameInfo**)kmalloc(numOfFrames*sizeof(struct FrameInfo));

    if (arrFramesInfo == NULL) {
               cprintf("Memory allocation for frames storage failed.\n");
               return NULL;
    }

	for(int i = 0; i<numOfFrames;i++){
		arrFramesInfo[i]=0;
	}

	return arrFramesInfo;

}


//=====================================
// [2] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* create_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("create_share is not implemented yet");
	//Your Code is Here...

	if(sizeof(shareName)> 64){
		return NULL;
	}

	if(size==0){
		return NULL;
	}

	if(isWritable > 1){
		return NULL;
	}
	size=ROUNDUP(size,PAGE_SIZE);
	int noFrames = size/PAGE_SIZE;

	struct Share* nShare = (struct Share*)kmalloc(sizeof(struct Share*)); //allocation of the shared object;

	if (nShare == NULL) {
		   cprintf("Memory allocation for share failed.\n");
		   return NULL;
	}

	nShare->ID = ((int32)nShare & 0x7FFFFFFF);

	nShare->ownerID = ownerID;

	strncpy(nShare->name, shareName, 64);

	nShare->size = size;

	nShare->references = 1;

	nShare->isWritable = isWritable;

	nShare->framesStorage = create_frames_storage(noFrames); //array of the frames allocated for
															 // this shared object
	if(nShare->framesStorage == NULL){
		return NULL;
	}
	return nShare;
}

//=============================
// [3] Search for Share Object:
//=============================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* get_share(int32 ownerID, char* name)
{
	//TODO: [PROJECT'24.MS2 - #17] [4] SHARED MEMORY - get_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("get_share is not implemented yet");
	//Your Code is Here...

	struct Share* share=(struct Share*)NULL;

	acquire_spinlock(&AllShares.shareslock);

	LIST_FOREACH(share,&AllShares.shares_list){
		if(share->ownerID == ownerID && !strcmp(share->name,name))
		{
//			cprintf("LOOP SHARE ID: %d \n\n",share->ID);
//			cprintf("LOOP SHARE ownerID: %d \n\n",share->ownerID);
//			cprintf("LOOP SHARE SIZE: %d \n\n",share->size);
//			cprintf("LOOP SHARE references: %d \n\n",share->references);
//			cprintf("LOOP SHARE isWritable: %u \n\n",share->isWritable);
//			cprintf("LOOP SHARE framesStorage: %u \n\n",(uint32)share->framesStorage[0]);
			release_spinlock(&AllShares.shareslock);
			return share;
		}
	}

	release_spinlock(&AllShares.shareslock);
	return NULL;

}

//=========================
// [4] Create Share Object:
//=========================
int createSharedObject(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #19] [4] SHARED MEMORY [KERNEL SIDE] - createSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("createSharedObject is not implemented yet");
	//Your Code is Here...
	//bsmallah;

	struct Env* myenv = get_cpu_proc(); //The calling environment

	//firstly call the create share
	if(get_share(ownerID,shareName)!=NULL)
	{
		return E_SHARED_MEM_EXISTS;
	}
	size=ROUNDUP(size,PAGE_SIZE);
	struct Share* ret = (struct Share*)NULL;
	ret = create_share(ownerID,shareName,size,isWritable);

	if(ret==NULL){
		return E_NO_SHARE;
	}
	cprintf("ret size: %d \n\n",size);
	cprintf("ret writable: %u \n\n",isWritable);


	acquire_spinlock(&AllShares.shareslock);

	//if return not null: add the shares list
	LIST_INSERT_HEAD(&AllShares.shares_list,ret);

	release_spinlock(&AllShares.shareslock);

	cprintf("list size: %d \n\n",&AllShares.shares_list.lh_first->size);
	cprintf("list writable: %u \n\n",&AllShares.shares_list.lh_first->isWritable);
	cprintf("list size: %d \n\n",(&AllShares.shares_list)->size);

	int noFrames = ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	uint32 permissions = PERM_USER  | PERM_PRESENT | PERM_WRITEABLE;
    uint32 current_page = (uint32)virtual_address;
	for(int i=0;i<noFrames;i++){

		struct FrameInfo* Frame;

		allocate_frame(&Frame);

		map_frame(myenv->env_page_directory, Frame, current_page, permissions);

		ret->framesStorage[i]=Frame;
		current_page+=PAGE_SIZE;

	}

	//allocate and map the shared object in physical memory at the passed va and then
	//put each frame as we calculated in the frameslist of the object
	return ret->ID;

}


//======================
// [5] Get Share Object:
//======================
int getSharedObject(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #21] [4] SHARED MEMORY [KERNEL SIDE] - getSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("getSharedObject is not implemented yet");
	//Your Code is Here...

	struct Env* myenv = get_cpu_proc(); //The calling environment

	struct Share* shared_object=(struct Share*)NULL;
	shared_object = get_share(ownerID,shareName);
	if(shared_object == NULL)
	{
		return E_SHARED_MEM_NOT_EXISTS;
	}
	int noFrames = ROUNDUP(getSizeOfSharedObject(ownerID,shareName),PAGE_SIZE)/PAGE_SIZE;
	uint32 permissions = PERM_USER  | PERM_PRESENT | (uint32)shared_object->isWritable;
	for(int i=0;i<noFrames;i++){
		map_frame(myenv->env_page_directory, shared_object->framesStorage[i], (uint32)virtual_address, permissions);
	}

	shared_object->references++;

	return shared_object->ID;

}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//==========================
// [B1] Delete Share Object:
//==========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - free_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("free_share is not implemented yet");
	//Your Code is Here...

}
//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - freeSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("freeSharedObject is not implemented yet");
	//Your Code is Here...

}
