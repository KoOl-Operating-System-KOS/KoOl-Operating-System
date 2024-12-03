#include <inc/lib.h>

#define ALLOC_FLAG ((uint32)1 << 31)
#define VAL_MASK (((uint32)1 << 31)-1)
#define PAGE_ALLOCATOR_START (myEnv->uheap_hard_limit + PAGE_SIZE)
#define PAGES_COUNT myEnv->uheap_pages_count
#define max(a, b) (a > b ? a : b)

uint32 info_tree[1<<19];
bool init = 0;
uint32 shared_id_directory[1<<19];
inline uint32 address_to_page(void* virtual_address){
	return ((uint32)virtual_address - PAGE_ALLOCATOR_START) / PAGE_SIZE;
}

inline uint32 set_info(uint32 cur, uint32 val, bool isAllocated){
	return info_tree[cur] = val | (isAllocated ? ALLOC_FLAG : 0);
}

inline bool is_allocated(uint32 cur){
	return (info_tree[cur] & ALLOC_FLAG);
}

inline uint32 get_free_value(uint32 cur){
	return (is_allocated(cur) ? 0 : info_tree[cur]);
}

inline uint32 get_value(uint32 cur){
	return info_tree[cur] & VAL_MASK;
}

inline void update_node(uint32 cur, uint32 val, bool isAllocated){
	set_info(cur, val, isAllocated);
	cur >>= 1;
	while(cur){
		set_info(cur, max(get_free_value(cur << 1), get_free_value(cur << 1 | 1)), 0);
		cur >>= 1;
	}
}

uint32 TREE_get_node(uint32 page_idx){
	uint32 cur = 1, l = 0, r = PAGES_COUNT-1;
	while(l < r){
		uint32 mid = (l + r) >> 1;
		if(page_idx <= mid) r = mid, cur <<= 1;
		else l = mid + 1, cur = cur << 1 | 1;
	}
	return cur;
}

uint32 TREE_first_fit(uint32 count, uint32* page_idx){
	uint32 cur = 1, l = 0, r = PAGES_COUNT-1;
	while(l < r){
		uint32 mid = (l + r) >> 1;
		if(get_free_value(cur << 1) >= count) r = mid, cur <<= 1;
		else l = mid + 1, cur = cur << 1 | 1;
		*page_idx = l;
	}
	return cur;
}

void* TREE_alloc_FF(uint32 count){

	if(get_value(1) < count) return NULL;

	uint32 page_idx;
	uint32 cur = TREE_first_fit(count, &page_idx);

	uint32 free_pages = get_free_value(cur);

	uint32 va = (PAGE_ALLOCATOR_START + page_idx * PAGE_SIZE);

	sys_allocate_user_mem(va, count * PAGE_SIZE);

	update_node(cur, count, 1);

	for(int i = 1; i < count; i++)
		set_info(cur + i, 0, 1);

	if(free_pages > count)
		update_node(cur + count, free_pages - count, 0);

	return (void*)va;
}

void* TREE_salloc_FF(uint32 count){

	if(get_value(1) < count) return NULL;

	uint32 page_idx;
	uint32 cur = TREE_first_fit(count, &page_idx);

	uint32 free_pages = get_free_value(cur);

	uint32 va = (PAGE_ALLOCATOR_START + page_idx * PAGE_SIZE);

	update_node(cur, count, 1);

	for(int i = 1; i < count; i++)
		set_info(cur + i, 0, 1);

	if(free_pages > count)
		update_node(cur + count, free_pages - count, 0);

	return (void*)va;
}


bool TREE_free(uint32 page_idx){

	uint32 cur = TREE_get_node(page_idx);

	if(!is_allocated(cur)) return 1; // is already free

	uint32 count = get_value(cur);

	if(count == 0) return 0; // is not a start of an allocation

	uint32 va = PAGE_ALLOCATOR_START + page_idx * PAGE_SIZE;

	sys_free_user_mem(va, count * PAGE_SIZE);

	for(int i = 0; i < count; i++)
		set_info(cur + i, 0, 0);

	if(!is_allocated(cur + count)){
		uint32 temp_count = get_free_value(cur + count);
		update_node(cur + count, 0, 0);
		count += temp_count;
	}

	if(page_idx == 0 || is_allocated(cur-1)){
		update_node(cur, count, 0);
	}
	else{
		int levels = 0, cur_node = cur;

		while(get_value(cur_node) == 0)
			cur_node >>= 1, levels++;

		while(levels--){
			if(get_value(cur_node << 1 | 1) > 0)
				cur_node = cur_node << 1 | 1;
			else
				cur_node = cur_node << 1;
		}

		update_node(cur_node, count + get_free_value(cur_node), 0);
	}

	return 1;
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment)
{
	return (void*) sys_sbrk(increment);
}

//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #12] [3] USER HEAP [USER SIDE] - malloc()
	// Write your code here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");

	if(!init){
		init = 1;
		update_node(TREE_get_node(0), (USER_HEAP_MAX - (myEnv->uheap_hard_limit + PAGE_SIZE)) / PAGE_SIZE, 0);
		memset(shared_id_directory,-1,sizeof shared_id_directory);
	}

	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
		return alloc_block_FF(size);
	uint32 pages_count = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

	if(sys_isUHeapPlacementStrategyFIRSTFIT())
		return TREE_alloc_FF(pages_count);

	return NULL;
	//Use sys_isUHeapPlacementStrategyFIRSTFIT() and	sys_isUHeapPlacementStrategyBESTFIT()
	//to check the current strategy

}

//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #14] [3] USER HEAP [USER SIDE] - free()
	// Write your code here, remove the panic and write your code
	//panic("free() is not implemented yet...!!");

	if(!init){
		init = 1;
		update_node(TREE_get_node(0), (USER_HEAP_MAX - (myEnv->uheap_hard_limit + PAGE_SIZE)) / PAGE_SIZE, 0);
		memset(shared_id_directory,-1,sizeof shared_id_directory);
	}

	if((uint32)virtual_address <= myEnv->uheap_segment_break-(DYN_ALLOC_MIN_BLOCK_SIZE+META_DATA_SIZE/2))
		free_block(virtual_address);
	else if(!TREE_free(address_to_page(virtual_address)))
		panic("Address given is not the start of the allocated space\n");
}

//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	 if (size == 0) return NULL ;

	if(!init){
		init = 1;
		update_node(TREE_get_node(0), (USER_HEAP_MAX - (myEnv->uheap_hard_limit + PAGE_SIZE)) / PAGE_SIZE, 0);
		memset(shared_id_directory,-1,sizeof shared_id_directory);
	}

	 uint32 pages_count = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;

	 void* va =TREE_salloc_FF(pages_count);

	 if(va == NULL)
	 {
		 return NULL;
	 }
	 int ret=sys_createSharedObject(sharedVarName,size,isWritable,va);
	 if(ret==E_NO_SHARE || ret==E_SHARED_MEM_EXISTS)
	 {
		 return NULL;
	 }
	 uint32 indx = ((uint32)va - PAGE_ALLOCATOR_START)/PAGE_SIZE;
	 shared_id_directory[indx] = ret;
	 return va;
}

//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//TODO: [PROJECT'24.MS2 - #20] [4] SHARED MEMORY [USER SIDE] - sget()
	// Write your code here, remove the panic and write your code
	//panic("sget() is not implemented yet...!!");

	int size =sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);
	if(size == E_SHARED_MEM_NOT_EXISTS || 0)
		return NULL;

	if(!init){
		init = 1;
		update_node(TREE_get_node(0), (USER_HEAP_MAX - (myEnv->uheap_hard_limit + PAGE_SIZE)) / PAGE_SIZE, 0);
	}


	uint32 pages_count = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	void* va =TREE_salloc_FF(pages_count);

	 if(va == NULL)
	 {
		 return NULL;
	 }
	 int ret=sys_getSharedObject(ownerEnvID,sharedVarName,va); //ret->id??
	 if(ret==E_SHARED_MEM_NOT_EXISTS)
	 {
		 return NULL;
	 }
	 uint32 indx = ((uint32)va - PAGE_ALLOCATOR_START)/PAGE_SIZE;
	 shared_id_directory[indx] = ret;
	 return va;
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	uint32 indx = ((uint32)virtual_address - PAGE_ALLOCATOR_START)/PAGE_SIZE;
	if(shared_id_directory[indx] == -1)
		panic("Invalid sfree address\n");
	if(sys_freeSharedObject(shared_id_directory[indx],virtual_address) == E_NO_SHARE)
		panic("No Share found\n");
	shared_id_directory[indx] = -1;
}


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

void expand(uint32 newSize)
{
	panic("Not Implemented");

}
void shrink(uint32 newSize)
{
	panic("Not Implemented");

}
void freeHeap(void* virtual_address)
{
	panic("Not Implemented");

}
