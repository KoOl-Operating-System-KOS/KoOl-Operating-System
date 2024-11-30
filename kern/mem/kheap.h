#ifndef FOS_KERN_KHEAP_H_
#define FOS_KERN_KHEAP_H_

#ifndef FOS_KERNEL
# error "This is a FOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <inc/queue.h>


/*2017*/
uint32 _KHeapPlacementStrategy;
//Values for user heap placement strategy
#define KHP_PLACE_CONTALLOC 0x0
#define KHP_PLACE_FIRSTFIT 	0x1
#define KHP_PLACE_BESTFIT 	0x2
#define KHP_PLACE_NEXTFIT 	0x3
#define KHP_PLACE_WORSTFIT 	0x4

static inline void setKHeapPlacementStrategyCONTALLOC(){_KHeapPlacementStrategy = KHP_PLACE_CONTALLOC;}
static inline void setKHeapPlacementStrategyFIRSTFIT(){_KHeapPlacementStrategy = KHP_PLACE_FIRSTFIT;}
static inline void setKHeapPlacementStrategyBESTFIT(){_KHeapPlacementStrategy = KHP_PLACE_BESTFIT;}
static inline void setKHeapPlacementStrategyNEXTFIT(){_KHeapPlacementStrategy = KHP_PLACE_NEXTFIT;}
static inline void setKHeapPlacementStrategyWORSTFIT(){_KHeapPlacementStrategy = KHP_PLACE_WORSTFIT;}

static inline uint8 isKHeapPlacementStrategyCONTALLOC(){if(_KHeapPlacementStrategy == KHP_PLACE_CONTALLOC) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyFIRSTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_FIRSTFIT) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyBESTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_BESTFIT) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyNEXTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_NEXTFIT) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyWORSTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_WORSTFIT) return 1; return 0;}

//***********************************

void* kmalloc(unsigned int size);
void kfree(void* virtual_address);
void *krealloc(void *virtual_address, unsigned int new_size);

unsigned int kheap_virtual_address(unsigned int physical_address);
unsigned int kheap_physical_address(unsigned int virtual_address);

int numOfKheapVACalls ;

//TODO: [PROJECT'24.MS2 - #01] [1] KERNEL HEAP - add suitable code here

uint32 Kernel_Heap_start;
uint32 segment_break;
uint32 Hard_Limit;


#define MST(X) (31 - __builtin_clz(X))
#define CEIL_POWER_OF_2(X) ((1 << MST((X))) * (1 + (((X) & ((X)-1)) > 0)))

#define KHEAP_PAGES_COUNT (CEIL_POWER_OF_2(NUM_OF_KHEAP_PAGES + 2))
#define ALLOC_FLAG ((uint32)1 << 31)
#define VAL_MASK (((uint32)1 << 31)-1)

uint32 page_allocator_start;
uint32 PAGES_COUNT;
uint32* info_tree;
void free_and_unmap_pages(uint32 start_address, uint32 frame_count);
int allocate_and_map_pages(uint32 start_address, uint32 end_address);
inline bool is_valid_kheap_address(uint32 virtual_address);
inline uint32 address_to_page(void* virtual_address);
inline void* relocate(void* old_va, uint32 copy_size, uint32 new_size);
inline uint32 set_info(uint32 cur, uint32 val, bool isAllocated);
inline bool is_allocated(uint32 cur);
inline uint32 get_free_value(uint32 cur);
inline uint32 get_value(uint32 cur);
inline void update_node(uint32 cur, uint32 val, bool isAllocated);
uint32 TREE_get_node(uint32 page_idx);
uint32 TREE_first_fit(uint32 count, uint32* page_idx);
void* TREE_alloc_FF(uint32 count);
bool TREE_free(uint32 page_idx);
void* TREE_realloc(uint32 page_idx, uint32 new_count);

#endif // FOS_KERN_KHEAP_H_
