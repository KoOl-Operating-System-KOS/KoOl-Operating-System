#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

int allocate_and_map_pages(uint32 startaddress,uint32 segment_break)
{
	uint32 current_page = startaddress;
	uint32 permissions = PERM_USER | PERM_WRITEABLE;

	while (current_page < segment_break) {
	    struct FrameInfo* Frame;

	    if (allocate_frame(&Frame) != 0) {
	    	return -1;
	    }

	    if (map_frame(ptr_page_directory, Frame, current_page, permissions) != 0) {
	    	return 0;
	    }

	    current_page += PAGE_SIZE;
	}

	return 1;
}

//[PROJECT'24.MS2] Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//    On success: 0
//    Otherwise (if no memory OR initial size exceed the given limit): E_NO_MEM
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
    //[PROJECT'24.MS2] [USER HEAP - KERNEL SIDE] initialize_kheap_dynamic_allocator
    // Write your code here, remove the panic and write your code
    //panic("initialize_kheap_dynamic_allocator() is not implemented yet...!!");
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

	//TODO: [PROJECT'24.MS2 - #02] [1] KERNEL HEAP - sbrk
	// Write your code here, remove the panic and write your code
	//panic("sbrk() is not implemented yet...!!");

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
	//TODO: [PROJECT'24.MS2 - #03] [1] KERNEL HEAP - kmalloc
	// Write your code here, remove the panic and write your code
	if(size<=DYN_ALLOC_MAX_BLOCK_SIZE)
		return alloc_block_FF(size);

	uint32 free_address=0;
	//free_address=free_spaces.get_first_fit_address(size);
	//if(free_address==NULL)
	//	return NULL;
	size=ROUNDUP(size,PAGE_SIZE);
	int result = allocate_and_map_pages(free_address,free_address+size);
	if(result== -1 || result== 0)
	{
		return NULL;
	}
	//mapped_spaces.insert(free_address,size);
	//uint32 old_size = free_spaces.get_size(free_address);
	//free_spaces.delete(free_address,old_size);
	//free_spaces.insert(free_address+size,old_size-size);
	return (void*)free_address;
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #04] [1] KERNEL HEAP - kfree
	// Write your code here, remove the panic and write your code
	panic("kfree() is not implemented yet...!!");

	//you need to get the size of the given allocation using its address
	//refer to the project presentation and documentation for details
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
