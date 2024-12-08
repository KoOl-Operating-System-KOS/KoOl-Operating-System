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

    struct FrameInfo** arrFramesInfo = kmalloc(numOfFrames * sizeof(struct FrameInfo*));

    if (arrFramesInfo == NULL) {
	   cprintf("Memory allocation for frames storage failed.\n");
	   return NULL;
    }

    for(int i = 0; i < numOfFrames; i++)
    	arrFramesInfo[i] = 0;

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

	if(size==0){
		return NULL;
	}

	if(isWritable > 1){
		return NULL;
	}

	int noFrames = ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;

	struct Share* nShare = kmalloc(sizeof(struct Share)); //allocation of the shared object;

	if (nShare == NULL) {
		   cprintf("Memory allocation for share failed.\n");
		   return NULL;
	}

	nShare->ID = ((int32)nShare & 0x7FFFFFFF);

	nShare->ownerID = ownerID;

	strcpy(nShare->name , shareName);

	nShare->size = size;

	nShare->references = 1;

	nShare->isWritable = isWritable;

	nShare->framesStorage = create_frames_storage(noFrames); //array of the frames allocated for
															 // this shared object
	if(nShare->framesStorage == NULL){
		kfree((void*)nShare);
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

	struct Share* share;
	if (!holding_spinlock(&AllShares.shareslock))
		acquire_spinlock(&AllShares.shareslock);

	LIST_FOREACH(share, &AllShares.shares_list){
		if(share->ownerID == ownerID && !strcmp(share->name, name))
		{
			if (holding_spinlock(&AllShares.shareslock))
				release_spinlock(&AllShares.shareslock);
			return share;
		}
	}

	if (holding_spinlock(&AllShares.shareslock))
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

	struct Env* myenv = get_cpu_proc(); //The calling environment

	//firstly call the create share
	if(get_share(ownerID, shareName) != NULL)
		return E_SHARED_MEM_EXISTS;

	struct Share* ret = create_share(ownerID,shareName,size,isWritable);;

	if(ret == NULL)
		return E_NO_SHARE;

	int noFrames = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

	uint32 permissions = PERM_USER | PERM_WRITEABLE;

    uint32 current_page = (uint32)virtual_address;

	for(int i = 0; i < noFrames; i++){

		struct FrameInfo* Frame;

		allocate_frame(&Frame);

		map_frame(myenv->env_page_directory, Frame, current_page, permissions);

		ret->framesStorage[i] = Frame;

		current_page += PAGE_SIZE;
	}

	if (!holding_spinlock(&AllShares.shareslock))
		acquire_spinlock(&AllShares.shareslock);

	//if return not null: add the shares list
	LIST_INSERT_TAIL(&AllShares.shares_list, ret);

	if (holding_spinlock(&AllShares.shareslock))
		release_spinlock(&AllShares.shareslock);

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

	struct Share* shared_object = (struct Share*)NULL;

	shared_object = get_share(ownerID, shareName);

	if(shared_object == NULL)
	{
		return E_SHARED_MEM_NOT_EXISTS;
	}

	int noFrames = ROUNDUP(getSizeOfSharedObject(ownerID,shareName),PAGE_SIZE)/PAGE_SIZE;

	uint32 permissions = PERM_USER;

	if(shared_object->isWritable)
		permissions |= PERM_WRITEABLE;

    uint32 current_page = (uint32)virtual_address;

	for(int i=0;i<noFrames;i++){
		map_frame(myenv->env_page_directory , shared_object->framesStorage[i] , current_page , permissions);

		current_page += PAGE_SIZE;
	}
	if (!holding_spinlock(&AllShares.shareslock))
		acquire_spinlock(&AllShares.shareslock);

	shared_object->references++;

	if (holding_spinlock(&AllShares.shareslock))
		release_spinlock(&AllShares.shareslock);

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
	if (!holding_spinlock(&AllShares.shareslock))
		acquire_spinlock(&AllShares.shareslock);

	LIST_REMOVE(&AllShares.shares_list, ptrShare);

	if (holding_spinlock(&AllShares.shareslock))
		release_spinlock(&AllShares.shareslock);

	kfree((void*)ptrShare->framesStorage);
	kfree((void*)ptrShare);
}
//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	if (!holding_spinlock(&AllShares.shareslock))
		acquire_spinlock(&AllShares.shareslock);

	struct Share* share = NULL;

	LIST_FOREACH(share, &AllShares.shares_list){

		if(share->ID == sharedObjectID){
			if (holding_spinlock(&AllShares.shareslock))
				release_spinlock(&AllShares.shareslock);
			break;
		}
	}

	if(share == NULL){
		if (!holding_spinlock(&AllShares.shareslock))
			release_spinlock(&AllShares.shareslock);
		return E_NO_SHARE;
	}

	struct Env* myenv = get_cpu_proc();

	uint32 current_page = (uint32)startVA;

	uint32 * ptr_page_table;

	uint32 frames_count = ROUNDUP(share->size , PAGE_SIZE) / PAGE_SIZE;
	for(int i = 0; i < frames_count; i++) {
		get_page_table(myenv->env_page_directory , current_page , &ptr_page_table);
		unmap_frame(myenv->env_page_directory , current_page);
		bool flag = 1;
		uint32* table = ptr_page_table;

		for(int i = 0; i < PAGE_SIZE / 4; i++) {
			if(table[i] & PERM_PRESENT) {
				flag = 0;
				break;
			}
		}
		if(flag) {
			pd_clear_page_dir_entry(myenv->env_page_directory, current_page);
			kfree((void*)ptr_page_table);
		}
		current_page += PAGE_SIZE;
	}

	if (!holding_spinlock(&AllShares.shareslock))
		acquire_spinlock(&AllShares.shareslock);

	share->references--;
	if(share->references==0)
		free_share(share);
	tlbflush();

	if (holding_spinlock(&AllShares.shareslock))
		release_spinlock(&AllShares.shareslock);

	return 0;

}
