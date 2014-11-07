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
    "Team one man army",
    /* First member's full name */
    "David Yin",
    /* First member's email address */
    "zhiwen.yin@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) < (y)?(x) :(y))

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

/* Given block ptr bp, find the next and previous free blocks */ 
#define NEXT_FREE_BLKP(bp) ((char *) GET(bp))
#define PREV_FREE_BLKP(bp) ((char *) GET(bp + WSIZE))

#define NUM_FREE_LISTS 10

typedef struct block
{
    struct block *next;
    struct block *pred;
} Node;

void* heap_listp = NULL;
Node* free_lists[NUM_FREE_LISTS];

/**
* List Operations
**/
//Get approriate free list pointer
int get_list_class(size_t size) {
    int result = 0;
    size--;
    while (size != 0) {
        size >>= 1;
        result++;
    }
    if (size <= 32) {
        result = 0;
    }
    result = MIN(result, NUM_FREE_LISTS - 1);
    return result;
}

//remove element from list
void remove_from_list(Node *p) {
    if (p == NULL) {
        return;
    }
    int class = get_list_class(GET_SIZE(HDRP(p)));

    if (p->next == NULL) {
        free_lists[class] = NULL;
    } else {
        p->next->pred = p->pred;
        if (p->pred != NULL)
            p->pred->next = p->next;
        if (free_lists[class] == p) {
            free_lists[class] = p->next;
        }   
    }
}

// add to front of list
void push(Node * bp) {
    if (bp == NULL) {
        return;
    }
    int class = get_list_class(GET_SIZE(HDRP(bp)));

    if (free_lists[class] == NULL)
    {
        free_lists[class] = bp;
        free_lists[class]->next = NULL;
        free_lists[class]->pred = NULL; 
    } else {
        bp->next = free_lists[class];
        bp->pred = NULL;
        bp->next->pred = bp;
        free_lists[class] = bp;
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
        remove_from_list((Node*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        remove_from_list((Node*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        remove_from_list((Node*)PREV_BLKP(bp));
        remove_from_list((Node*)NEXT_BLKP(bp));
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
    int class = get_list_class(asize);
    int i;
    for (i = class; i < NUM_FREE_LISTS; i++) {
        if (free_lists[i] != NULL) {
            Node* free_list = free_lists[i];
            do {
                int size = GET_SIZE(HDRP(free_list));
                int free_size = size - asize;
                //remove it from the free list
                if (size >= asize && free_size < 32) {
                    remove_from_list(free_list);
                    return (void*) free_list;
                } else if (free_size >= 32) {
                    remove_from_list(free_list);
                    void *p = (void*)free_list + asize;

                    PUT(HDRP(free_list), PACK(asize, 0));
                    PUT(FTRP(free_list), PACK(asize, 0));

                    PUT(HDRP(p), PACK(free_size,0));
                    PUT(FTRP(p), PACK(free_size,0));
                    push(p);
                    return (void*) free_list;
                }
                free_list = free_list->next;
            } while (free_list != NULL);
        }
    }
    return NULL;
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
    // fprintf(stderr, "free\n");
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    push((Node*)coalesce(bp));

    // PUT(HDRP(bp) + WSIZE, (uintptr_t) free_list);
    // PUT(free_list + WSIZE, (uintptr_t) bp);
    // free_list = bp;
    // PUT(HDRP(bp) + DSIZE, (uintptr_t) NULL); 
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
    // size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* round it to a power of two value */
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

    // fprintf(stderr, "------------------malloc size: %d\n", asize);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    // extendsize = MAX(asize, CHUNKSIZE);
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
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *coalesce_ptr;
    void *oldptr = ptr;
    void *new_ptr;
    size_t asize;
    size_t old_size;
    size_t coalesce_size;
    size_t copy_size;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    old_size = GET_SIZE(HDRP(ptr));
    
    if (asize == old_size)
        return ptr;
    
    // if shrinking just reallocate old sapce
    else if (asize < old_size) { 
        int free_size = old_size - asize;
        if (free_size >= 32) {
            void* free_ptr = (void*)ptr + asize;
            
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            
            PUT(HDRP(free_ptr), PACK(free_size, 0));
            PUT(FTRP(free_ptr), PACK(free_size, 0));
            push(coalesce(free_ptr));
        }
        return ptr;
    }
    
    // expanding
    else {
        PUT(HDRP(ptr), PACK(old_size, 0));
        PUT(FTRP(ptr), PACK(old_size, 0));
        coalesce_ptr = coalesce(ptr);
        coalesce_size = GET_SIZE(HDRP(coalesce_ptr));
        
        // new size fits in coalesced block
        if (coalesce_size >= asize) {
            int free_size = coalesce_size - asize;
            if (free_size >= 32) {
                void* free_ptr = (void*)coalesce_ptr + asize;
                memmove(coalesce_ptr, ptr, old_size - DSIZE);
                PUT(HDRP(coalesce_ptr), PACK(asize, 1));
                PUT(FTRP(coalesce_ptr), PACK(asize, 1));
                PUT(HDRP(free_ptr), PACK(free_size, 0));
                PUT(FTRP(free_ptr), PACK(free_size, 0));
                push(free_ptr);
                
            }
            else {
                memmove(coalesce_ptr, ptr, old_size - DSIZE);
                PUT(HDRP(coalesce_ptr), PACK(coalesce_size, 1));
                PUT(FTRP(coalesce_ptr), PACK(coalesce_size, 1));
            }
            return coalesce_ptr;
        }

        // new size does not fit in coalesced block
        else {
            
            
            new_ptr = mm_malloc(size);
            if (new_ptr == NULL)
              return NULL;
            /* Copy the old data. */
            memcpy(new_ptr, ptr, old_size - DSIZE);
            
            push((Node*)coalesce_ptr); 
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
