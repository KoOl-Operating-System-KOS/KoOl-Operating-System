#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

#define max(a, b) (a > b ? a : b)

int allocate_and_map_pages(uint32 startaddress,uint32 segment_break)
{
    uint32 current_page = startaddress;
    uint32 permissions = PERM_PRESENT | PERM_WRITEABLE;

    while (current_page < segment_break) {
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

inline int set_value(int cur, int val, bool isAllocated){
	return info_tree[cur] = val * (isAllocated ? -1 : 1);
}

inline int get_value(int cur){
	return max(info_tree[cur], 0);
}

inline int get_address(int cur){
	return -info_tree[cur];
}

inline int is_allocated(int cur){
	return (info_tree[cur] < 0);
}

void TREE_add_free(int page_idx, int count, int cur, int l, int r){
	if(l == r){
		set_value(cur, count, 0);
		return;
	}

	int mid = (l + r) >> 1;

	if(page_idx <= mid)
		TREE_add_free(page_idx, count, cur << 1, l, mid);
	else
		TREE_add_free(page_idx, count, cur << 1 | 1, mid + 1, r);

	set_value(cur, max(get_value(cur << 1), get_value(cur << 1 | 1)), 0);
}

void* TREE_alloc_FF(int count, int cur, int l, int r){

	if(l == r){
		int page_idx = l;
		int free_pages = get_value(cur);

		void* va = (void*)(page_allocator_start + page_idx * PAGE_SIZE);
		int ecode = allocate_and_map_pages((uint32)va, (uint32)va + count * PAGE_SIZE);

		if(ecode == -1 || ecode == 0) return NULL;

		for(int i = 0; i < count; i++)
			set_value(cur + i, page_idx + 1, 1);

		if(free_pages > count) set_value(cur + count, free_pages - count, 0);

		int s = cur / 2, e = (cur + count - (free_pages == count)) / 2;

		while(s > 0){
			for(int i = s; i <= e; i++)
				set_value(i, max(get_value(i << 1), get_value(i << 1 | 1)), 0);
			s >>= 1, e >>= 1;
		}

		return va;
	}

	int mid = (l + r) >> 1;

	if(get_value(cur << 1) >= count)
		return TREE_alloc_FF(count, cur << 1, l, mid);
	else
		return TREE_alloc_FF(count, (cur << 1) + 1, mid + 1, r);
}

bool TREE_free(int page_idx, int cur, int l, int r){

	if(l == r){
		if(!is_allocated(cur) || get_address(cur) != page_idx + 1)
			return 0;

		int cur_node = cur, cur_va = page_allocator_start + page_idx * PAGE_SIZE;
		int count = 0, total_count = 0;

		while(is_allocated(cur_node) && get_address(cur_node) == page_idx + 1){
			uint32 *ptr_page_table;
			free_frame(get_frame_info(ptr_page_directory, cur_va, &ptr_page_table));
			unmap_frame(ptr_page_directory, cur_va);

			set_value(cur_node, 0, 0);
			cur_va += PAGE_SIZE;
			count++, cur_node++;
		}
		total_count = count;

		if(!is_allocated(cur_node)){
			total_count += get_value(cur_node);
			set_value(cur_node, 0, 0);
		}

		int s = cur / 2, e = (cur + count - (total_count == count)) / 2;

		while(s > 0){
			for(int i = s; i <= e; i++)
				set_value(i, max(get_value(i << 1), get_value(i << 1 | 1)), 0);
			s >>= 1, e >>= 1;
		}

		cur_node = cur;
		if(page_idx == 0 || is_allocated(cur-1)){
			set_value(cur_node, total_count, 0);
		}
		else{
			int levels = 0;

			while(get_value(cur_node) == 0)
				cur_node >>= 1, levels++;

			while(levels--){
				if(get_value(cur_node << 1 | 1) > 0)
					cur_node = cur_node << 1 | 1;
				else
					cur_node = cur_node << 1;
			}

			set_value(cur_node, total_count + get_value(cur_node), 0);
		}

		cur_node >>= 1;
		while(cur_node){
			set_value(cur_node, max(get_value(cur_node << 1), get_value(cur_node << 1 | 1)), 0);
			cur_node >>= 1;
		}

		return 1;
	}

	int mid = (l + r) >> 1;

	if(page_idx <= mid)
		return TREE_free(page_idx, cur << 1, l, mid);
	else
		return TREE_free(page_idx, cur << 1 | 1, mid + 1, r);
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

    page_allocator_start = Hard_Limit + PAGE_SIZE;

    TREE_add_free(0, (KERNEL_HEAP_MAX - page_allocator_start) / PAGE_SIZE, 1, 0, PAGES_COUNT-1);

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

//TODO: [PROJECT'24.MS2 - BONUS#2] [1] KERNEL HEAP - Fast Page Allocator

void* kmalloc(unsigned int size)
{
	if(size<=DYN_ALLOC_MAX_BLOCK_SIZE)
		return alloc_block_FF(size);

	uint32 pages_count = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;

	if(get_value(1) < pages_count)
		return NULL;

	return TREE_alloc_FF(pages_count, 1, 0, PAGES_COUNT-1);
}

void kfree(void* virtual_address)
{
	if((uint32)virtual_address <= Hard_Limit){
		free_block(virtual_address);
		return;
	}

	uint32 address_dir = ((uint32)virtual_address - page_allocator_start) / PAGE_SIZE;

	if((uint32)virtual_address % PAGE_SIZE != 0 || !TREE_free(address_dir, 1, 0, PAGES_COUNT-1))
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
	//TODO: [PROJECT'24.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc
	// Write your code here, remove the panic and write your code
	return NULL;
	panic("krealloc() is not implemented yet...!!");
}
