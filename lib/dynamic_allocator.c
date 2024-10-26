/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"


//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
__inline__ uint32 get_block_size(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (*curBlkMetaData) & ~(0x1);
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
__inline__ int8 is_free_block(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (~(*curBlkMetaData) & 0x1) ;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================

void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================

void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockElement* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", get_block_size(blk), is_free_block(blk)) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

bool is_initialized = 0;
//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (initSizeOfAllocatedSpace % 2 != 0) initSizeOfAllocatedSpace++; //ensure it's multiple of 2
		if (initSizeOfAllocatedSpace == 0)
			return ;
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================

	//Initializing BEG_Block and END_Block
	uint32* BEG_Block = (uint32*)daStart;
	uint32* Free_Block = (uint32*)(daStart + 2 * sizeof(uint32));
	uint32* END_Block = (uint32*)(daStart + initSizeOfAllocatedSpace - sizeof(uint32));
	//Setting the meta data for BEG_Block,END_Block blocks and Free_Block
	*BEG_Block = 1;
	*END_Block = 1;
	set_block_data((void*)(Free_Block), initSizeOfAllocatedSpace - DYN_ALLOC_MIN_BLOCK_SIZE, 0);
	//Initializing the freeBlocksList
	LIST_INIT(&freeBlocksList);
	LIST_INSERT_TAIL(&freeBlocksList, (struct BlockElement*)Free_Block);

}
//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================
void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{
	uint32 meta_data = totalSize | (uint32)isAllocated;
	*((uint32*)va - 1) = meta_data;
	*((uint32*)((char*)va + (totalSize - 2 * sizeof(uint32)))) = meta_data;
}


//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================

bool alloc(struct BlockElement *current_free_block, uint32 required_size)
{
    bool is_enough_space = 0;
    void *start_of_block = (void*)(current_free_block);
    uint32 block_size = get_block_size(start_of_block);

    if(block_size >= required_size)
    {
        is_enough_space = 1;
        set_block_data(start_of_block, block_size, 1);

        if(block_size - required_size >= 2 * DYN_ALLOC_MIN_BLOCK_SIZE)
        {
            struct BlockElement* extra_space = (struct BlockElement*)((char*)start_of_block + required_size);
            LIST_INSERT_BEFORE(&freeBlocksList, current_free_block, extra_space);
            set_block_data(start_of_block, required_size, 1);
            set_block_data((void*)(extra_space), block_size - required_size, 0);
        }

        LIST_REMOVE(&freeBlocksList, current_free_block);
    }

    return is_enough_space;
}

void *alloc_block_FF(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE ;
		if (!is_initialized)
		{
			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			uint32 da_break = (uint32)sbrk(0);
			initialize_dynamic_allocator(da_start, da_break - da_start);
		}
	}
	//==================================================================================
	//==================================================================================

	uint32 required_size = size + DYN_ALLOC_MIN_BLOCK_SIZE;
	bool found_fitting_size = 0;

	struct BlockElement *current_free_block;

	LIST_FOREACH(current_free_block, &freeBlocksList){
		if(alloc(current_free_block, required_size))
		{
			found_fitting_size = 1;
			break;
		}
	}

	if (!found_fitting_size)
	{
	 current_free_block = (struct BlockElement*) sbrk(required_size);
	 if (current_free_block == (struct BlockElement*)-1) return NULL;
	}

	return (void*)current_free_block;
}
//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{

	uint32 required_size = size + DYN_ALLOC_MIN_BLOCK_SIZE;
	uint32 min_diffrience = (1 << 30);
	bool found_fitting_size = 0;
	struct BlockElement *best_block;
	struct BlockElement *current_free_block;

	LIST_FOREACH(current_free_block, &freeBlocksList){
		if(get_block_size(current_free_block) < required_size) continue;

		if(get_block_size(current_free_block) - required_size < min_diffrience){
			min_diffrience = get_block_size(current_free_block) - required_size;
			found_fitting_size = 1;
			best_block = current_free_block;
		}
	}

	if (found_fitting_size == 0)
	{
		best_block = (struct BlockElement *)sbrk(required_size);
		if (best_block == (struct BlockElement *)-1)
		  return NULL;
	}
	else
	{
		alloc(best_block, required_size);
	}

	return (void*)best_block;
}

//===================================================
// [5] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{

	if(va == NULL || is_free_block(va))return;

    uint32 cur_size = get_block_size(va);

    void *next_block = (void*)(((char*)va) + cur_size);
    void *prev_block = (void*)((uint32*)va - 1);

    uint32 next_size = get_block_size(next_block);
    uint32 prev_size = get_block_size(prev_block);

    prev_block = (void*)(((char*)prev_block) - prev_size + 4);

    if(is_free_block(next_block) && is_free_block(prev_block)){
    	set_block_data(prev_block, prev_size + cur_size + next_size, 0);
    	LIST_REMOVE(&freeBlocksList,(struct BlockElement*)next_block);
    }
    else if(is_free_block(next_block)){
    	set_block_data(va, cur_size + next_size, 0);
    	LIST_INSERT_BEFORE(&freeBlocksList, (struct BlockElement*)next_block, (struct BlockElement*)va);
    	LIST_REMOVE(&freeBlocksList,(struct BlockElement*)next_block);
    }
    else if(is_free_block(prev_block)){
    	set_block_data(prev_block, prev_size + cur_size, 0);
    }
    else{
    	set_block_data(va, cur_size, 0);

    	if(LIST_SIZE(&freeBlocksList) == 0 || (void*)LIST_LAST(&freeBlocksList) < va){
    		LIST_INSERT_TAIL(&freeBlocksList, (struct BlockElement*)va);
    		return;
    	}

    	struct BlockElement* cur;
    	LIST_FOREACH (cur, &freeBlocksList) if((void*)cur > va) break;
    	LIST_INSERT_BEFORE(&freeBlocksList, cur, (struct BlockElement*)va);
    }

}

//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void* va, uint32 new_size)
{
	//TODO: [PROJECT'24.MS1 - #08] [3] DYNAMIC ALLOCATOR - realloc_block_FF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("realloc_block_FF is not implemented yet");
	//Your Code is Here...

}

/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}
