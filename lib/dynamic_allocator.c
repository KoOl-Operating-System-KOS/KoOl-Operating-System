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
//============================= HELPER FUNCTIONS ===================================//
//==================================================================================//

// adds a free block and merges it with adjacent free blocks if possible
// (MAKE SURE PREVIOUS AND NEXT BLOCKS' DATA ARE SET CORRECTLY BEFORE CALLING)
void* extend_mapped_region(uint32 size)
{
	uint32 no_of_pages=ROUNDUP(size,PAGE_SIZE)/PAGE_SIZE;
	void* va = sbrk(no_of_pages);
	if (va == (void*)-1) return NULL;

	va=add_free_block(va,no_of_pages*PAGE_SIZE);
	alloc(va,size);
	return va;
}

void* add_free_block(void* va, uint32 size){

	void *next_block = (void*)(((char*)va) + size);
	void *prev_block = (void*)((uint32*)va - 1);

	uint32 next_size = get_block_size(next_block);
	uint32 prev_size = get_block_size(prev_block);

	prev_block = (void*)(((char*)prev_block) - prev_size + 4);

	if(is_free_block(next_block) && is_free_block(prev_block)){
		LIST_REMOVE(&freeBlocksList,(struct BlockElement*)next_block);
		set_block_data(prev_block, prev_size + size + next_size, 0);
		return prev_block;
	}
	else if(is_free_block(prev_block)){
		set_block_data(prev_block, prev_size + size, 0);
		return prev_block;
	}
	else if(is_free_block(next_block)){
		LIST_REMOVE(&freeBlocksList,(struct BlockElement*)next_block);
		size = size + next_size;
	}

	set_block_data(va, size, 0);

	if(LIST_SIZE(&freeBlocksList) == 0 || (void*)LIST_LAST(&freeBlocksList) < va)
	{
		LIST_INSERT_TAIL(&freeBlocksList, (struct BlockElement*)va);
	}
	else
	{
		struct BlockElement* cur;
		LIST_FOREACH (cur, &freeBlocksList) if((void*)cur > va) break;
		LIST_INSERT_BEFORE(&freeBlocksList, cur, (struct BlockElement*)va);
	}
	return va;
}

bool alloc(struct BlockElement *current_free_block, uint32 required_size)
{
    bool is_enough_space = 0;
    void *start_of_block = (void*)(current_free_block);
    uint32 block_size = get_block_size(start_of_block);

    if(block_size >= required_size)
    {
        is_enough_space = 1;
        LIST_REMOVE(&freeBlocksList, current_free_block);

        if(block_size - required_size >= DYN_ALLOC_MIN_BLOCK_SIZE + META_DATA_SIZE)
        {
            set_block_data(start_of_block, required_size, 1);
            void* extra_space = (void*)((char*)start_of_block + required_size);
            add_free_block(extra_space, block_size - required_size);
        }
        else
        {
        	set_block_data(start_of_block, block_size, 1);
        }

    }

    return is_enough_space;
}

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
	set_block_data((void*)(Free_Block), initSizeOfAllocatedSpace - META_DATA_SIZE, 0);
	//Initializing the freeBlocksList
	LIST_INIT(&freeBlocksList);
	LIST_INSERT_TAIL(&freeBlocksList, (struct BlockElement*)Free_Block);

}

//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================
void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{
	if(totalSize & 1) panic("set_block_data called with odd Size");

	uint32 meta_data = totalSize | (uint32)isAllocated;
	*((uint32*)va - 1) = meta_data;
	*((uint32*)((char*)va + (totalSize - 2 * sizeof(uint32)))) = meta_data;
}

//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size)
{
	if(size == 0) return NULL;

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

	uint32 required_size = size + META_DATA_SIZE;
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
		return extend_mapped_region(required_size);
	}

	return (void*)current_free_block;
}

//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{
	if(size == 0) return NULL;

	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE;
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

	uint32 required_size = size + META_DATA_SIZE;
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
		return extend_mapped_region(required_size);
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
	if(va == NULL || is_free_block(va)) return;

    uint32 cur_size = get_block_size(va);
    add_free_block(va, cur_size);
}

//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void* va, uint32 new_size)
{
	// if address is null -> allocate a new block
	if(va == NULL) return alloc_block_FF(new_size);
	// if new size is zero -> free the block
	if(new_size == 0){
		free_block(va);
		return NULL;
	}

	if (new_size % 2 != 0) new_size++;	//ensure that the size is even (to use LSB as allocation flag)
	if (new_size < DYN_ALLOC_MIN_BLOCK_SIZE)
		new_size = DYN_ALLOC_MIN_BLOCK_SIZE ;

	uint32 required_size = new_size + META_DATA_SIZE;
	uint32 prev_size = get_block_size(va);
	void* next_block = (void*)((char*)va + prev_size);
	uint32 next_block_size = get_block_size(next_block);

	// reallocating to a smaller size
	if(required_size <= prev_size){

		// if there is enough space for a free block after resizing -> add free block
		// if next block is free -> add free space to the next block regardless of size to avoid fragmantation
		if(prev_size - required_size >= DYN_ALLOC_MIN_BLOCK_SIZE + META_DATA_SIZE || is_free_block(next_block)){
			set_block_data(va, required_size, 1);
			void* extra_space = (void*)((char*)va + required_size);
			add_free_block(extra_space, prev_size - required_size);
		}
		return va;
	}

	// if there is a free block next that has enough space -> expand to next
	if(is_free_block(next_block) && prev_size + next_block_size >= required_size)
	{
		LIST_REMOVE(&freeBlocksList,(struct BlockElement*)next_block);

		// if remaining size is enough to form a new free block -> insert it as a free block
		if(prev_size + next_block_size - required_size >= DYN_ALLOC_MIN_BLOCK_SIZE + META_DATA_SIZE)
		{
			set_block_data(va, required_size, 1);
			void* extra_space = (void*)((char*)va + required_size);
			add_free_block(extra_space, prev_size + next_block_size - required_size);
		}
		else
		{
			set_block_data(va, prev_size + next_block_size, 1);
		}

		return va;
	}


	void* new_block = alloc_block_FF(required_size);

	if(new_block != NULL){

		for(int i = 0; i < prev_size; i++)
			*((char*)new_block + i) = *((char*)va + i);

		free_block(va);
	}

	return new_block;
}

/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	if(size == 0) return NULL;

	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE;
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

	uint32 required_size = size + META_DATA_SIZE;
	uint32 max_diffrience = 0;
	bool found_fitting_size = 0;
	struct BlockElement *worst_block;
	struct BlockElement *current_free_block;

	LIST_FOREACH(current_free_block, &freeBlocksList){
		if(get_block_size(current_free_block) < required_size) continue;

		if(get_block_size(current_free_block) - required_size >= max_diffrience){
			max_diffrience = get_block_size(current_free_block) - required_size;
			found_fitting_size = 1;
			worst_block = current_free_block;
		}
	}

	if (found_fitting_size == 0)
	{
		return extend_mapped_region(required_size);
	}
	else
	{
		alloc(worst_block, required_size);
	}

	return (void*)worst_block;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	if(size == 0) return NULL;

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

	uint32 required_size = size + META_DATA_SIZE;

	static struct BlockElement *NF_free_block = NULL; //static pointer to keep last order

	// for the worst case, the pointer will reach the end then cycle to the begining in the second iteration
	for(int i = 0; i < 2; i++){

		// if the pointer is NULL (reached end of the list), restart from the begining
		if(NF_free_block == NULL) NF_free_block = LIST_FIRST(&freeBlocksList);

		// search until finding a fit or reaching the end of the list
		while(NF_free_block != NULL){
			struct BlockElement *allocBlock = NF_free_block;
			NF_free_block = LIST_NEXT(NF_free_block);
			if(alloc(allocBlock, required_size))
			{
				void* next_block = (void*)((char*)allocBlock + get_block_size(allocBlock));
				if(is_free_block(next_block))
					NF_free_block = next_block;
				return allocBlock;
			}
		}
	}

	// if no fit is matched after the loop, then there is no possible matches, call sbrk
	NF_free_block = extend_mapped_region(required_size);

	if (NF_free_block != NULL)
	{
		NF_free_block += required_size;
	}

	return NF_free_block;
}
