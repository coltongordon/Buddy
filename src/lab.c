#include <stdio.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include "lab.h"
#define handle_error_and_die(msg) \
do \
{ \
    perror(msg); \
    raise(SIGKILL); \
} while (0)

#define BLOCK_USED 1


/**
* @brief Convert bytes to the correct K value
*
* @param bytes the number of bytes
* @return size_t the K value that will fit bytes
*/
/**
 * @brief Convert bytes to the correct K value
 *
 * This function calculates the smallest power of 2 (K) that can accommodate
 * the given number of bytes. The K value is used in the buddy memory allocation
 * system to determine the size class of memory blocks.
 *
 * Instructions to implement:
 * 1. Validate the input parameter `bytes` to ensure it is greater than zero.
 * 2. Start with an initial K value of 0.
 * 3. Use a loop to repeatedly shift the value 1 (representing 2^K) to the left
 *    until it is greater than or equal to `bytes`.
 * 4. Increment the K value during each iteration of the loop.
 * 5. Return the calculated K value.
 *
 * Notes:
 * - Avoid using floating-point operations or math libraries like `math.pow`.
 * - Ensure the function handles edge cases, such as very small or very large
 *   values of `bytes`.
 */
size_t btok(size_t bytes)
{
    // Ensure the input is greater than zero
    assert(bytes > 0);

    // Add the size of the header to the requested bytes
    bytes += sizeof(struct avail);

    // Initialize K value
    size_t kval = 0;

    // Calculate the smallest power of 2 (K) that can accommodate the bytes
    while ((UINT64_C(1) << kval) < bytes)
    {
        kval++;
    }

    return kval;
}



/**
 * Calculates the buddy block for a given block in a buddy memory pool.
 *
 * @param pool A pointer to the buddy memory pool structure. This structure
 *             contains information about the memory pool and its configuration.
 * @param buddy A pointer to the structure representing the block for which
 *              the buddy block needs to be calculated. This structure should
 *              contain details such as the block's address or index.
 * 
 * @return A pointer to the structure representing the calculated buddy block.
 *         This structure will contain the details of the buddy block, such as
 *         its address or index.
 *
 * Instructions to implement:
 * - Use the block's address or index to determine its buddy block.
 * - Ensure the calculation adheres to the buddy memory allocation algorithm.
 * - Validate the input parameters to ensure they are not NULL and are within
 *   the bounds of the memory pool.
 * - Return the calculated buddy block structure or handle errors appropriately
 *   if the calculation fails.
 */
struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy)
{
    assert(pool != NULL);
    assert(buddy != NULL);

    // Calculate the offset of the buddy block
    size_t block_offset = (unsigned char *)buddy - (unsigned char *)pool->base;

    // Calculate the size of the block using its kval
    size_t block_size = UINT64_C(1) << buddy->kval;

    // XOR the block offset with the block size to find the buddy's offset
    size_t buddy_offset = block_offset ^ block_size;

    // Return the pointer to the buddy block
    return (struct avail *)((unsigned char *)pool->base + buddy_offset);
}

/**
 * Allocates a block of memory from the buddy memory pool.
 *
 * This function implements the buddy memory allocation algorithm, which is 
 * designed to efficiently allocate and free memory blocks of sizes that are 
 * powers of two. It takes a pointer to a buddy memory pool and the requested 
 * size of memory to allocate.
 *
 * Instructions to implement this function:
 * 1. Validate the input parameters:
 *    - Ensure the `pool` pointer is not NULL.
 *    - Ensure the requested `size` is greater than zero.
 * 2. Adjust the requested size to the nearest power of two that can accommodate it.
 * 3. Traverse the buddy memory pool to find a free block of the appropriate size:
 *    - If a larger block is found, split it into smaller blocks until the desired size is reached.
 *    - Mark the allocated block as used.
 * 4. Return a pointer to the allocated memory block.
 * 5. If no suitable block is available, return NULL to indicate allocation failure.
 *
 * Parameters:
 * - pool: A pointer to the buddy memory pool from which memory is to be allocated.
 * - size: The size of the memory block to allocate (in bytes).
 *
 * Returns:
 * - A pointer to the allocated memory block on success.
 * - NULL if the allocation fails due to insufficient memory or invalid input.
 *
 * Notes:
 * - Ensure thread safety if the buddy memory pool is accessed concurrently.
 * - Consider implementing coalescing of adjacent free blocks during deallocation
 *   to optimize memory usage.
 * - Handle edge cases, such as alignment requirements and minimum block sizes.
 */
void *buddy_malloc(struct buddy_pool *pool, size_t size)
{
    assert(pool != NULL);
    assert(size > 0);

    // Add header size to the requested size
    size += sizeof(struct avail);

    // Get the kval for the requested size
    size_t kval = MIN_K;
    while ((UINT64_C(1) << kval) < size)
    {
        kval++;
    }

    // R1: Find a block
    for (size_t i = kval; i <= pool->kval_m; i++)
    {
        if (pool->avail[i].next != NULL && pool->avail[i].next != &pool->avail[i])
        {
            // R2: Remove from list
            struct avail *block = pool->avail[i].next;
            block->prev->next = block->next;
            block->next->prev = block->prev;

            // R3: Split required?
            while (i > kval)
            {
                i--;
                size_t block_size = UINT64_C(1) << i;
                struct avail *buddy = (struct avail *)((unsigned char *)block + block_size);

                // R4: Split the block
                buddy->tag = BLOCK_AVAIL;
                buddy->kval = i;
                buddy->next = &pool->avail[i];
                buddy->prev = pool->avail[i].prev;
                pool->avail[i].prev->next = buddy;
                pool->avail[i].prev = buddy;

                block->kval = i;
            }

            block->tag = BLOCK_RESERVED;
            return (void *)((unsigned char *)block + sizeof(struct avail));
        }
    }

    // No suitable block found
    errno = ENOMEM;
    return NULL;
}

/**
 * Frees a previously allocated memory block in the buddy memory pool.
 *
 * This function is responsible for returning a memory block, identified by 
 * the pointer `ptr`, back to the buddy memory pool `pool`. It should:
 * 
 * 1. Validate the input parameters to ensure `ptr` is within the bounds of 
 *    the memory pool and that `pool` is not NULL.
 * 2. Determine the size of the block being freed based on the buddy system 
 *    allocation rules.
 * 3. Mark the block as free in the buddy system's internal data structures.
 * 4. Coalesce adjacent free blocks, if possible, to maintain the buddy system's 
 *    efficiency and reduce fragmentation.
 * 
 * Proper error handling should be implemented to handle invalid inputs or 
 * inconsistencies in the memory pool's state.
 *
 * @param pool A pointer to the buddy memory pool structure.
 * @param ptr  A pointer to the memory block to be freed.
 */
void buddy_free(struct buddy_pool *pool, void *ptr)
{
    assert(pool != NULL);
    assert(ptr != NULL);

    // Calculate the address of the block header
    struct avail *block = (struct avail *)((unsigned char *)ptr - sizeof(struct avail));

    // Validate that the block is within the pool's memory range
    assert((unsigned char *)block >= (unsigned char *)pool->base);
    assert((unsigned char *)block < (unsigned char *)pool->base + pool->numbytes);

    // Mark the block as available
    block->tag = BLOCK_AVAIL;

    // Coalesce adjacent free blocks
    while (true)
    {
        // Calculate the buddy block
        struct avail *buddy = buddy_calc(pool, block);

        // Check if the buddy block is free and has the same kval
        if ((unsigned char *)buddy < (unsigned char *)pool->base ||
            (unsigned char *)buddy >= (unsigned char *)pool->base + pool->numbytes ||
            buddy->tag != BLOCK_AVAIL || buddy->kval != block->kval)
        {
            break;
        }

        // Remove the buddy block from its free list
        buddy->prev->next = buddy->next;
        buddy->next->prev = buddy->prev;

        // Determine the lower address between the block and its buddy
        if (buddy < block)
        {
            block = buddy;
        }

        // Increase the kval of the coalesced block
        block->kval++;
    }

    // Add the coalesced block back to the free list
    block->next = pool->avail[block->kval].next;
    block->prev = &pool->avail[block->kval];
    pool->avail[block->kval].next->prev = block;
    pool->avail[block->kval].next = block;

    // Check if the coalesced block spans the entire pool
    if (block->kval == pool->kval_m && block == (struct avail *)pool->base) {
        // Reset the pool to its initial state
        for (size_t i = 0; i <= pool->kval_m; i++) {
            pool->avail[i].next = pool->avail[i].prev = &pool->avail[i];
        }
        block->next = block->prev = &pool->avail[block->kval];
    }
}



/**
* @brief This is a simple version of realloc.
*
* @param poolThe memory pool
* @param ptr The user memory
* @param size the new size requested
* @return void* pointer to the new user memory
*/




void buddy_init(struct buddy_pool *pool, size_t size)
{
size_t kval = 0;
if (size == 0)
kval = DEFAULT_K;
else
kval = btok(size);
if (kval < MIN_K)
kval = MIN_K;
if (kval > MAX_K)
kval = MAX_K - 1;
//make sure pool struct is cleared out
memset(pool,0,sizeof(struct buddy_pool));
pool->kval_m = kval;
pool->numbytes = (UINT64_C(1) << pool->kval_m);
//Memory map a block of raw memory to manage
pool->base = mmap(
NULL, /*addr to map to*/
pool->numbytes, /*length*/
PROT_READ | PROT_WRITE, /*prot*/
MAP_PRIVATE | MAP_ANONYMOUS, /*flags*/
-1, /*fd -1 when using MAP_ANONYMOUS*/
0 /* offset 0 when using MAP_ANONYMOUS*/
);
if (MAP_FAILED == pool->base)
{
handle_error_and_die("buddy_init avail array mmap failed");
}
//Set all blocks to empty. We are using circular lists so the first elements
//just point
//to an available block. Thus the tag, and kval feild are unused burning a
//small bit of
//memory but making the code more readable. We mark these blocks as UNUSED to
//aid in debugging.
for (size_t i = 0; i <= kval; i++)
{
pool->avail[i].next = pool->avail[i].prev = &pool->avail[i];
pool->avail[i].kval = i;
pool->avail[i].tag = BLOCK_UNUSED;
}
//Add in the first block
pool->avail[kval].next = pool->avail[kval].prev = (struct avail *)pool->base;
struct avail *m = pool->avail[kval].next;
m->tag = BLOCK_AVAIL;
m->kval = kval;
m->next = m->prev = &pool->avail[kval];
}
void buddy_destroy(struct buddy_pool *pool)
{
int rval = munmap(pool->base, pool->numbytes);
if (-1 == rval)
{
handle_error_and_die("buddy_destroy avail array");
}
//Zero out the array so it can be reused it needed
memset(pool,0,sizeof(struct buddy_pool));
}
#define UNUSED(x) (void)x
/**
* This function can be useful to visualize the bits in a block. This can
* help when figuring out the buddy_calc function!
*/
static void printb(unsigned long int b)
{
size_t bits = sizeof(b) * 8;
unsigned long int curr = UINT64_C(1) << (bits - 1);
for (size_t i = 0; i < bits; i++)
{
if (b & curr)
{
printf("1");
}
else
{
printf("0");
}
curr >>= 1L;
}
}