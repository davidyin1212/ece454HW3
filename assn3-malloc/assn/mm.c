/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Void",
    /* First member's full name */
    "Yi Ping Sun",
    /* First member's email address */
    "...",
    /* Second member's full name (leave blank if none) */
    "Ling Zhong",
    /* Second member's email address (leave blank if none) */
    "..."
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) > (y)?(y) :(x))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//#define NEXT_FREE_BLKP(bp)	((char *)(*(uintptr_t *)(bp)))
//#define PREV_FREE_BLKP(bp)	((char *)(*(uintptr_t *)(bp + WSIZE)))

void* heap_listp = NULL;

typedef struct free_block
{
	struct free_block* prev;
    struct free_block* next;    
} free_block;

#define NUM_FREE_LISTS	1
free_block* free_lists[NUM_FREE_LISTS];

#define MIN_FREE_SIZE_POW   5
#define MIN_FREE_SIZE       (1<<5)





int list_index(size_t words)
{
    int count = 0;
    words--;
    while (words != 0) {
        words >>= 1;
        count++;
    }
    count = MAX(0, count-MIN_FREE_SIZE_POW);
    count = MIN(NUM_FREE_LISTS-1, count);
    return count;
}

void list_remove(free_block* bp)
{
    if (bp==NULL)
        return;
        
    int index = list_index(GET_SIZE(HDRP(bp)));
    
    if (bp->next==bp)
        free_lists[index] = NULL;
    else {
        bp->prev->next = bp->next;
        bp->next->prev = bp->prev;
        if (free_lists[index] == bp)
            free_lists[index] = bp->next;
        //PUT(NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)), NEXT_FREE_BLKP(bp));
        //PUT(PREV_FREE_BLKP(NEXT_FREE_BLKP(bp)), PREV_FREE_BLKP(bp));
        //if (free_lists[index]==bp)
        //    free_lists[index] = NEXT_FREE_BLKP(bp);
    }
}

void list_add(free_block* bp)
{
    if (bp==NULL)
        return;

    int index = list_index(GET_SIZE(HDRP(bp)));
    
    if (free_lists[index] == NULL) {
        free_lists[index] = bp;
        free_lists[index]->next = bp;
        free_lists[index]->prev = bp;
        //free_lists[index] = bp;
        //PUT(NEXT_FREE_BLKP(bp), bp);
        //PUT(PREV_FREE_BLKP(bp), bp);
    }
    else {
        bp->next = free_lists[index];
        bp->prev = free_lists[index]->prev;
        bp->prev->next = bp;
        bp->next->prev = bp;
        //PUT(NEXT_FREE_BLKP(bp), free_lists[index]);
        //PUT(PREV_FREE_BLKP(bp), PREV_FREE_BLKP(free_lists[index]));
        //PUT(NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)), bp);
        //PUT(PREV_FREE_BLKP(NEXT_FREE_BLKP(bp)), bp);
    }
        
    
}


/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                         // alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
    heap_listp += DSIZE;
    
    int i;
    for (i=0; i < NUM_FREE_LISTS; i++)
        free_lists[i] = NULL;

    return 0;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        list_remove((free_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        list_remove((free_block*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        list_remove((free_block*)PREV_BLKP(bp));
        list_remove((free_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return bp;
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    int index = list_index(asize);
    int i;
    for (i = index; i < NUM_FREE_LISTS; i++) {
        if (free_lists[i] != NULL) {
            free_block* p = free_lists[i];
            do {
                int size = GET_SIZE(HDRP(p));
                int free_size = size - asize;
                if (size >= asize && free_size < MIN_FREE_SIZE) {
                    list_remove(p);
                    return (void*)p;
                }
                else if (free_size >= MIN_FREE_SIZE) {
                    list_remove(p);
                    void* free_ptr = (void*)p + asize;
                    
                    PUT(HDRP(p), PACK(asize, 0));
                    PUT(FTRP(p), PACK(asize, 0));
                    
                    PUT(HDRP(free_ptr), PACK(free_size, 0));
                    PUT(FTRP(free_ptr), PACK(free_size, 0));
                    list_add(free_ptr);
                    return (void*)p;
                }    
                p = p->next;
            } while(p!=free_lists[i]);
        }
        
        
    }
    
    return NULL;
    
    /*void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
    */
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    list_add((free_block*)coalesce(bp));
}



/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    //size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
	
	// optimize for binary trace pattern
	// round to power of 2 blocks
	if (size < 512) { 
	   	int i = 1;
	   	while (i < size)
	   		i <<= 1;
	   	size = i;
	}
        
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */	
	if ((bp = extend_heap(asize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
	void *coal_ptr;
    void *new_ptr;
    
    size_t asize; /* adjusted block size */
    size_t old_size;
    size_t coal_size;
    size_t copy_size;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));
    
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
	
    old_size = GET_SIZE(HDRP(ptr));
    
    if (asize == old_size)
    	return ptr;
    
    // shrinking
    else if (asize < old_size) { 
    	int free_size = old_size - asize;
    	if (free_size >= MIN_FREE_SIZE) {
            void* free_ptr = (void*)ptr + asize;
            
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            
            PUT(HDRP(free_ptr), PACK(free_size, 0));
            PUT(FTRP(free_ptr), PACK(free_size, 0));
            list_add(free_ptr);
    	}
    	return ptr;
    }
	
    // expanding
    else {
		PUT(HDRP(ptr), PACK(old_size, 0));
		PUT(FTRP(ptr), PACK(old_size, 0));
		coal_ptr = coalesce(ptr);
		coal_size = GET_SIZE(HDRP(coal_ptr));
		
		// new size fits in coalesced block
		if (coal_size >= asize) {
			int free_size = coal_size - asize;
			if (free_size >= MIN_FREE_SIZE) {
		        void* free_ptr = (void*)coal_ptr + asize;
		        memmove(coal_ptr, ptr, old_size - DSIZE);
				PUT(HDRP(coal_ptr), PACK(asize, 1));
		    	PUT(FTRP(coal_ptr), PACK(asize, 1));
		        PUT(HDRP(free_ptr), PACK(free_size, 0));
		        PUT(FTRP(free_ptr), PACK(free_size, 0));
		        list_add(free_ptr);
		        
			}
			else {
				memmove(coal_ptr, ptr, old_size - DSIZE);
				PUT(HDRP(coal_ptr), PACK(coal_size, 1));
		    	PUT(FTRP(coal_ptr), PACK(coal_size, 1));
			}
			return coal_ptr;
		}

		// new size does not fit in coalesced block
		else {
			
			
			new_ptr = mm_malloc(size);
			if (new_ptr == NULL)
			  return NULL;
			/* Copy the old data. */
			memcpy(new_ptr, ptr, old_size - DSIZE);
			
			list_add((free_block*)coal_ptr); 
			return new_ptr;
		}
    }
    
    return NULL;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/

int mm_check(void){
	return 1;
}


