#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

#define max(a, b) (a > b ? a : b)

int allocate_and_map_pages(uint32 start_address, uint32 end_address)
{
    uint32 current_page = start_address;
    uint32 permissions = PERM_PRESENT | PERM_WRITEABLE;

    while (current_page < end_address) {
        struct FrameInfo* Frame;

        if (allocate_frame(&Frame) != 0) {
            return -1;
        }

        if (map_frame(ptr_page_directory, Frame, current_page, permissions) != 0) {
            free_frame(Frame);
            return 0;
        }

        current_page += PAGE_SIZE;
    }

    return 1;
}

inline bool is_valid_kheap_address(uint32 virtual_address){
	return (virtual_address < KERNEL_HEAP_MAX &&
			virtual_address >= page_allocator_start &&
			virtual_address % PAGE_SIZE == 0);
}

inline uint32 address_to_page(void* virtual_address){
	return ((uint32)virtual_address - page_allocator_start) / PAGE_SIZE;
}

inline void* relocate(void* old_va, uint32 copy_size, uint32 new_size){
	void* new_va = kmalloc(new_size);
	if(new_va == NULL) return NULL;
	memcpy(new_va, old_va, copy_size);
	kfree(old_va);
	return new_va;
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
		if(get_value(cur << 1) >= count) r = mid, cur <<= 1;
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

	void* va = (void*)(page_allocator_start + page_idx * PAGE_SIZE);
	int ecode = allocate_and_map_pages((uint32)va, (uint32)va + count * PAGE_SIZE);

	if(ecode == -1 || ecode == 0) return NULL;

	update_node(cur, count, 1);
	for(int i = 1; i < count; i++)
		set_info(cur + i, 0, 1);

	if(free_pages > count)
		update_node(cur + count, free_pages - count, 0);

	return va;
}

bool TREE_free(uint32 page_idx){

	uint32 cur = TREE_get_node(page_idx);

	if(!is_allocated(cur)) return 1; // is already free

	uint32 count = get_value(cur);

	if(count == 0) return 0; // is not a start of an allocation

	uint32 cur_va = page_allocator_start + page_idx * PAGE_SIZE, *ptr_page_table;

	for(int i = 0; i < count; i++){
		set_info(cur + i, 0, 0);
		free_frame(get_frame_info(ptr_page_directory, cur_va, &ptr_page_table));
		unmap_frame(ptr_page_directory, cur_va);
		cur_va += PAGE_SIZE;
	}

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

void* TREE_realloc(uint32 page_idx, uint32 new_size){

	uint32 new_count = ROUNDUP(new_size, PAGE_SIZE) / PAGE_SIZE;
	uint32 cur = TREE_get_node(page_idx);
	uint32 old_count = get_value(cur);

	if(!is_allocated(cur) || old_count == 0)
		return NULL;

	uint32 nxt = cur + old_count;
	uint32 next_count = get_free_value(nxt);

	if(old_count + next_count < new_count) // relocate
		return relocate((void*)(page_allocator_start + page_idx * PAGE_SIZE), old_count * PAGE_SIZE, new_size);

	if(!is_allocated(nxt))
		update_node(nxt, 0, 0);

	if(new_count <= old_count){ // shrink

		uint32 cur_va = page_allocator_start + (page_idx + new_count) * PAGE_SIZE, *ptr_page_table;

		for(int i = new_count; i < old_count; i++){
			set_info(cur + i, 0, 0);
			free_frame(get_frame_info(ptr_page_directory, cur_va, &ptr_page_table));
			unmap_frame(ptr_page_directory, cur_va);
			cur_va += PAGE_SIZE;
		}
	}
	else{ // expand
		uint32 cur_va = page_allocator_start + (page_idx + old_count) * PAGE_SIZE, *ptr_page_table;

		for(int i = old_count; i < new_count; i++)
			set_info(cur + i, 0, 1);
	}

	set_info(cur, new_count, 1);

	if(old_count + next_count - new_count > 0)
		update_node(page_idx + new_count, old_count + next_count - new_count, 0);

	return (void*)(page_allocator_start + page_idx * PAGE_SIZE);
}

//[PROJECT'24.MS2] Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//    On success: 0
//    Otherwise (if no memory OR initial size exceed the given limit): E_NO_MEM
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
    if(daStart + initSizeToAllocate > daLimit)
        panic("Initial dynamic allocation size exceeds the dynamic allocation's hard limit.");

    Kernel_Heap_start = daStart;
    segment_break = daStart+initSizeToAllocate;
    Hard_Limit = daLimit;

    int result = allocate_and_map_pages(daStart, segment_break);
    if(result== -1)
    {
        panic("Failed to allocate frame.");
    }
    else if (result== 0)
    {
		panic("Failed to map frame to page.");
    }
    initialize_dynamic_allocator(daStart, initSizeToAllocate);

    info_tree = (void*)(Hard_Limit + PAGE_SIZE);
    uint32 DS_Size = 2 * PAGES_COUNT * sizeof(int);
    page_allocator_start = Hard_Limit + PAGE_SIZE + ROUNDUP(DS_Size, PAGE_SIZE);
    allocate_and_map_pages(Hard_Limit + PAGE_SIZE, page_allocator_start);

    update_node(TREE_get_node(0), (KERNEL_HEAP_MAX - page_allocator_start) / PAGE_SIZE, 0);

    return 0;
}

void* sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the kernel to increase the size of its heap by the given numOfPages,
	 * 				you should allocate pages and map them into the kernel virtual address space,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) Allocating additional pages for a kernel dynamic allocator will fail if the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sbrk fails, return -1
	 */

	uint32 added_size= numOfPages*PAGE_SIZE;
	uint32 old_segment_break=segment_break;
	if(numOfPages== 0)
	{
		return (void*)segment_break;
	}
	if(segment_break+added_size>Hard_Limit)
	{
		return (void*)-1;
	}
	int result = allocate_and_map_pages(segment_break,segment_break+added_size);
	if(result== -1 || result== 0)
	{
		return (void*)-1;
	}
	uint32* old_end_block=(uint32*)(old_segment_break-META_DATA_SIZE/2);
	*old_end_block=0;
	segment_break = segment_break+added_size;
	uint32* END_Block=(uint32*)(segment_break-META_DATA_SIZE/2);
	*END_Block = 1;

	return (void*)old_segment_break;
}

void* kmalloc(unsigned int size)
{
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
		return alloc_block_FF(size);

	uint32 pages_count = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	return TREE_alloc_FF(pages_count);
}

void kfree(void* virtual_address)
{
	if((uint32)virtual_address <= Hard_Limit)
		free_block(virtual_address);
	else if(!is_valid_kheap_address((uint32)virtual_address) || !TREE_free(address_to_page(virtual_address)))
		panic("Trying to free an address that is not allocated\n");
}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address
	// Write your code here, remove the panic and write your code
	panic("kheap_physical_address() is not implemented yet...!!");

	//return the physical address corresponding to given virtual_address
	//refer to the project presentation and documentation for details

	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address
	// Write your code here, remove the panic and write your code
	panic("kheap_virtual_address() is not implemented yet...!!");

	//return the virtual address corresponding to given physical_address
	//refer to the project presentation and documentation for details

	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================
}
//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, if moved to another loc: the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
	if(virtual_address == NULL) // allocate when va = NULL
		return kmalloc(new_size);

	if((uint32)virtual_address <= Hard_Limit && new_size <= DYN_ALLOC_MAX_BLOCK_SIZE) // handled in block allocator
		return realloc_block_FF(virtual_address, new_size);

	if(new_size == 0){ // free when new_size = 0
		kfree(virtual_address);
		return NULL;
	}

	if((uint32)virtual_address <= Hard_Limit) // relocate from block allocator -> page allocator
		return relocate(virtual_address, get_block_size(virtual_address), new_size);

	if(!is_valid_kheap_address((uint32)virtual_address)) // check if address is a valid page start
		return NULL;

	if(new_size <= DYN_ALLOC_MAX_BLOCK_SIZE) // relocate from page allocator -> block allocator
		return relocate(virtual_address, new_size, new_size);

	return TREE_realloc(address_to_page(virtual_address), new_size);
}
