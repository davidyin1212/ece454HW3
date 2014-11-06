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
#define NEXT_FREE_BLKP(bp) (GET(bp))
#define PREV_FREE_BLKP(bp) (GET(bp + WSIZE))

void* heap_listp = NULL;
void* free_list = NULL;

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
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        remove_from_list(NEXT_BLKP(bp));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        remove_from_list(bp);
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        remove_from_list(bp);
        remove_from_list(NEXT_BLKP(bp));
        return (PREV_BLKP(bp));
    }
}

void remove_from_list(void *p) {
    fprintf(stderr, "value of p: %p\n", (uintptr_t)p);
    //get p next's pred pointer
    void *pred_of_next = NULL;
    if (GET(p) != NULL)
        pred_of_next = GET(p) + WSIZE;
    //get p pred's next pointer
    void *next_of_pred = GET(p + WSIZE);
    fprintf(stderr, "value of pred: %p value of next: %p\n", (uintptr_t)next_of_pred, GET(p));
    if (next_of_pred != NULL) {
        fprintf(stderr, "executing next\n");
        PUT(next_of_pred, (uintptr_t) GET(p));
    }
    if (pred_of_next != NULL) {
        fprintf(stderr, "executing pred\n");
        PUT(pred_of_next, (uintptr_t) GET(p + WSIZE));
    }
}

// add to front of list
void push(void * bp) {
    void *next = bp;
    PUT(next, free_list);
    PUT(next + WSIZE, NULL);
    free_list = bp;
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
    fprintf(stderr, "allocated this much space: %d\n", size);
    fprintf(stderr, "extend heap free_list: %p\n", (uintptr_t) free_list);
    fprintf(stderr, "extend heap bp: %p\n", (uintptr_t) bp);
    PUT(HDRP(bp) + WSIZE, (uintptr_t) free_list);
    PUT(HDRP(bp) + DSIZE, (uintptr_t) NULL);
    free_list = bp;


    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    void *bp;
    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    // {
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    //     {
    //         return bp;
    //     }
    // }

    //explicit free list
    if (free_list == NULL) {
        return NULL;
    }
    fprintf(stderr, "next free blk: %p\n", (uintptr_t) NEXT_FREE_BLKP(free_list));
    fprintf(stderr, "free_list: %p\n", (uintptr_t) free_list);
    fprintf(stderr, "size: %d\n", (uintptr_t) GET_SIZE(HDRP(free_list)));
    fprintf(stderr, "asize: %d\n", (uintptr_t) asize);
    fprintf(stderr, "is allocated: %d\n", GET_ALLOC(HDRP(free_list)));

    for (bp = (void *) free_list; bp != NULL; bp = (void *) NEXT_FREE_BLKP(bp))
    {
        // fprintf(stderr, "bp: %p\n", (uintptr_t) bp);
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
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
    int isHead = 0;
    // char * pred;
    // char * next;
    // pred = GET(HDRP(bp) + DSIZE);
    // next = GET(HDRP(bp) + WSIZE);
    remove_from_list(bp);  

    fprintf(stderr, "asize:%d, bsize:%d\n", asize, bsize);

    //check to see if not already head of list
    if (GET(bp + WSIZE) == NULL) {
        isHead = 1;
        free_list = GET(bp);
    }

    if (GET(bp) != NULL)
        fprintf(stderr, "next: %p, pred: %p\n", GET(bp), GET(GET(bp) + WSIZE));

    //split
    PUT(FTRP(bp), PACK(bsize - asize, 0));
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(FTRP(bp) + WSIZE, PACK(bsize - asize, 0));
    // free_list = FTRP(bp) + DSIZE;

    fprintf(stderr, "isHead: %d\n", isHead);
    //insert free left back in at head of list
    if (bsize - asize > 0 && isHead != 1) {
        push(FTRP(bp) + DSIZE);
        // PUT(FTRP(bp) + DSIZE, (uintptr_t) next);
        // PUT(FTRP(bp) + DSIZE + WSIZE, (uintptr_t) pred);
        // free_list = FTRP(bp) + DSIZE;
    } else if (isHead == 1) {
        PUT(FTRP(bp) + DSIZE, free_list);
        PUT(FTRP(bp) + DSIZE + WSIZE, NULL);
        free_list = FTRP(bp) + DSIZE;
    } 
    if (bsize - asize == 0) {
        free_list = NULL;
    }
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    fprintf(stderr, "----------------freee timeeeee\n");
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
    PUT(HDRP(bp) + WSIZE, (uintptr_t) free_list);
    PUT(free_list + WSIZE, (uintptr_t) bp);
    free_list = bp;
    PUT(HDRP(bp) + DSIZE, (uintptr_t) NULL); 
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
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    fprintf(stderr, "------------------malloc size: %d\n", asize);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
        place(bp, asize);

    // PUT(FTRP(bp) + DSIZE, (uintptr_t) next);
    // PUT(FTRP(bp) + DSIZE + WSIZE, (uintptr_t) pred);
    // fprintf(stderr, "malloc heap free_list: %p\n", (uintptr_t) free_list);
    // free_list = FTRP(bp) + DSIZE;
    fprintf(stderr, "after malloc heap free_list: %p\n", (uintptr_t) free_list);
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

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
  return 1;
}
