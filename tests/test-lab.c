#include <assert.h>
#include <stdlib.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include "harness/unity.h"
#include "../src/lab.h"
void setUp(void) {
// set stuff up here
}
void tearDown(void) {
// clean stuff up here
}
/**
* Check the pool to ensure it is full.
*/
void check_buddy_pool_full(struct buddy_pool *pool)
{
//A full pool should have all values 0-(kval-1) as empty
  for (size_t i = 0; i < pool->kval_m; i++)
  {
    assert(pool->avail[i].next == &pool->avail[i]);
    assert(pool->avail[i].prev == &pool->avail[i]);
    assert(pool->avail[i].tag == BLOCK_UNUSED);
    assert(pool->avail[i].kval == i);
  }
  //The avail array at kval should have the base block
  assert(pool->avail[pool->kval_m].next->tag == BLOCK_AVAIL);
  assert(pool->avail[pool->kval_m].next->next == &pool->avail[pool->kval_m]);
  assert(pool->avail[pool->kval_m].prev->prev == &pool->avail[pool->kval_m]);
  //Check to make sure the base address points to the starting pool
  //If this fails either buddy_init is wrong or we have corrupted the
  //buddy_pool struct.
  assert(pool->avail[pool->kval_m].next == pool->base);
}
/**
* Check the pool to ensure it is empty.
*/
void check_buddy_pool_empty(struct buddy_pool *pool)
{
//An empty pool should have all values 0-(kval) as empty
  for (size_t i = 0; i <= pool->kval_m; i++)
  {
    assert(pool->avail[i].next == &pool->avail[i]);
    assert(pool->avail[i].prev == &pool->avail[i]);
    assert(pool->avail[i].tag == BLOCK_UNUSED);
    assert(pool->avail[i].kval == i);
  }
}
/**
* Test allocating 1 byte to make sure we split the blocks all the way down
* to MIN_K size. Then free the block and ensure we end up with a full
* memory pool again
*/
void test_buddy_malloc_one_byte(void)
{
  fprintf(stderr, "->Test allocating and freeing 1 byte\n");
  struct buddy_pool pool;
  int kval = MIN_K;
  size_t size = UINT64_C(1) << kval;
  buddy_init(&pool, size);
  void *mem = buddy_malloc(&pool, 1);
  //Make sure correct kval was allocated
  buddy_free(&pool, mem);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}
/**
* Tests the allocation of one massive block that should consume the entire memory
* pool and makes sure that after the pool is empty we correctly fail subsequent
calls.
*/
void test_buddy_malloc_one_large(void)
{
  fprintf(stderr, "->Testing size that will consume entire memory pool\n");
  struct buddy_pool pool;
  size_t bytes = UINT64_C(1) << MIN_K;
  buddy_init(&pool, bytes);
  //Ask for an exact K value to be allocated. This test makes assumptions on
  //the internal details of buddy_init.
  size_t ask = bytes - sizeof(struct avail);
  void *mem = buddy_malloc(&pool, ask);
  assert(mem != NULL);
  //Move the pointer back and make sure we got what we expected
  struct avail *tmp = (struct avail *)mem - 1;
  assert(tmp->kval == MIN_K);
  assert(tmp->tag == BLOCK_RESERVED);
  check_buddy_pool_empty(&pool);
  //Verify that a call on an empty tool fails as expected and errno is set to
  // ENOMEM.
  void *fail = buddy_malloc(&pool, 5);
  assert(fail == NULL);
  assert(errno = ENOMEM);
  //Free the memory and then check to make sure everything is OK
  buddy_free(&pool, mem);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}
/**
* Tests to make sure that the struct buddy_pool is correct and all fields
* have been properly set kval_m, avail[kval_m], and base pointer after a
* call to init
*/
void test_buddy_init(void)
{
  fprintf(stderr, "->Testing buddy init\n");
  //Loop through all kval MIN_k-DEFAULT_K and make sure we get the correct amount
  // allocated.
  //We will check all the pointer offsets to ensure the pool is all configured
  // correctly
  for (size_t i = MIN_K; i <= DEFAULT_K; i++)
  {
    size_t size = UINT64_C(1) << i;
    struct buddy_pool pool;
    buddy_init(&pool, size);
    check_buddy_pool_full(&pool);
    buddy_destroy(&pool);
  }
}

/**
 * Test allocating multiple blocks of varying sizes and ensure proper behavior.
 */
void test_buddy_malloc_multiple_blocks(void) {
  fprintf(stderr, "->Testing multiple block allocations\n");
  struct buddy_pool pool;
  size_t pool_size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, pool_size);

  // Allocate a small block
  void *block1 = buddy_malloc(&pool, 32);
  assert(block1 != NULL);

  // Allocate a medium block
  void *block2 = buddy_malloc(&pool, 128);
  assert(block2 != NULL);

  // Allocate a large block
  void *block3 = buddy_malloc(&pool, 512);
  assert(block3 != NULL);

  // Ensure the pool is not full
  assert(pool.avail[DEFAULT_K].next != pool.base);

  // Free the blocks
  buddy_free(&pool, block1);
  buddy_free(&pool, block2);
  buddy_free(&pool, block3);

  // Check if the pool is back to full state
  check_buddy_pool_full(&pool);

  buddy_destroy(&pool);
}


void test_btok_exact_power_of_two(void) {
  fprintf(stderr, "->Testing btok with exact power of two\n");
  assert(btok(1) == 0);  // 2^0 = 1
  assert(btok(2) == 1);  // 2^1 = 2
  assert(btok(4) == 2);  // 2^2 = 4
  assert(btok(8) == 3);  // 2^3 = 8
  assert(btok(16) == 4); // 2^4 = 16
  fprintf(stderr, "Passed exact power of two test\n");
}

void test_btok_non_power_of_two(void) {
  fprintf(stderr, "->Testing btok with non-power of two values\n");
  assert(btok(3) == 2);  // 3 <= 2^2
  assert(btok(5) == 3);  // 5 <= 2^3
  assert(btok(9) == 4);  // 9 <= 2^4
  assert(btok(17) == 5); // 17 <= 2^5
  fprintf(stderr, "Passed non-power of two test\n");
}

void test_btok_large_values(void) {
  fprintf(stderr, "->Testing btok with large values\n");
  assert(btok(1024) == 10);       // 1024 = 2^10
  assert(btok(2048) == 11);       // 2048 = 2^11
  assert(btok(4096) == 12);       // 4096 = 2^12
  assert(btok(5000) == 13);       // 5000 <= 2^13
  assert(btok(1 << 20) == 20);    // 2^20 = 1 MiB
  fprintf(stderr, "Passed large values test\n");
}

/**
 * Test buddy_calc with a valid buddy in the middle of the pool.
 */
void test_buddy_calc_middle(void) {
  fprintf(stderr, "->Testing buddy_calc with a middle block\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  struct avail *block = pool.base;
  block->kval = 3; // Assume block is at kval 3
  struct avail *buddy = buddy_calc(&pool, block);

  // Calculate expected buddy address
  uintptr_t block_addr = (uintptr_t)block;
  uintptr_t buddy_addr = block_addr ^ (UINT64_C(1) << block->kval);
  assert((uintptr_t)buddy == buddy_addr);

  buddy_destroy(&pool);
}

/**
 * Test buddy_calc with the first block in the pool.
 */
void test_buddy_calc_first_block(void) {
  fprintf(stderr, "->Testing buddy_calc with the first block\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  struct avail *block = pool.base;
  block->kval = 2; // Assume block is at kval 2
  struct avail *buddy = buddy_calc(&pool, block);

  // Calculate expected buddy address
  uintptr_t block_addr = (uintptr_t)block;
  uintptr_t buddy_addr = block_addr ^ (UINT64_C(1) << block->kval);
  assert((uintptr_t)buddy == buddy_addr);

  buddy_destroy(&pool);
}

/**
 * Test buddy_calc with the last block in the pool.
 */
void test_buddy_calc_last_block(void) {
  fprintf(stderr, "->Testing buddy_calc with the last block\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  struct avail *block = (struct avail *)((uintptr_t)pool.base + size - sizeof(struct avail));
  block->kval = 4; // Assume block is at kval 4
  struct avail *buddy = buddy_calc(&pool, block);

  // Calculate expected buddy address
  uintptr_t block_addr = (uintptr_t)block;
  uintptr_t buddy_addr = block_addr ^ (UINT64_C(1) << block->kval);
  assert((uintptr_t)buddy == buddy_addr);

  buddy_destroy(&pool);
}

/**
 * Test freeing a valid block and ensuring the pool returns to a full state.
 */
void test_buddy_free_valid_block(void) {
  fprintf(stderr, "->Testing buddy_free with a valid block\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  void *block = buddy_malloc(&pool, 64);
  assert(block != NULL);

  buddy_free(&pool, block);
  check_buddy_pool_full(&pool);

  buddy_destroy(&pool);
}

/**
 * Test freeing a null pointer and ensuring no changes occur in the pool.
 */
void test_buddy_free_null_pointer(void) {
  fprintf(stderr, "->Testing buddy_free with a null pointer\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  check_buddy_pool_full(&pool); // Ensure pool is initially full
  buddy_free(&pool, NULL);      // Freeing NULL should do nothing
  check_buddy_pool_full(&pool); // Pool should remain full

  buddy_destroy(&pool);
}

/**
 * Test freeing a block not allocated by buddy_malloc and ensure undefined behavior is handled.
 */
void test_buddy_free_invalid_pointer(void) {
  fprintf(stderr, "->Testing buddy_free with an invalid pointer\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  int invalid_block;
  void *invalid_ptr = &invalid_block;

  // Freeing an invalid pointer should not corrupt the pool
  buddy_free(&pool, invalid_ptr);

  // Check if the pool is still in a valid state
  check_buddy_pool_full(&pool);

  buddy_destroy(&pool);
}

/**
 * Test buddy_malloc with a NULL pool.
 */
void test_buddy_malloc_null_pool(void) {
  fprintf(stderr, "->Testing buddy_malloc with a NULL pool\n");
  void *result = buddy_malloc(NULL, 64);
  assert(result == NULL);
}

/**
 * Test buddy_malloc with size 0.
 */
void test_buddy_malloc_zero_size(void) {
  fprintf(stderr, "->Testing buddy_malloc with size 0\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  void *result = buddy_malloc(&pool, 0);
  assert(result == NULL);

  buddy_destroy(&pool);
}

/**
 * Test buddy_malloc with a size larger than the pool.
 */
void test_buddy_malloc_size_larger_than_pool(void) {
  fprintf(stderr, "->Testing buddy_malloc with size larger than pool\n");
  struct buddy_pool pool;
  size_t pool_size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, pool_size);

  void *result = buddy_malloc(&pool, pool_size + 1);
  assert(result == NULL);
  assert(errno == ENOMEM);

  buddy_destroy(&pool);
}

/**
 * Test buddy_malloc with a size that is an exact power of two.
 */
void test_buddy_malloc_exact_power_of_two(void) {
  fprintf(stderr, "->Testing buddy_malloc with exact power of two size\n");
  struct buddy_pool pool;
  size_t pool_size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, pool_size);

  void *result = buddy_malloc(&pool, 128); // 128 = 2^7
  assert(result != NULL);

  buddy_free(&pool, result);
  check_buddy_pool_full(&pool);

  buddy_destroy(&pool);
}

/**
 * Test buddy_malloc with a size that is not a power of two.
 */
void test_buddy_malloc_non_power_of_two(void) {
  fprintf(stderr, "->Testing buddy_malloc with non-power of two size\n");
  struct buddy_pool pool;
  size_t pool_size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, pool_size);

  void *result = buddy_malloc(&pool, 150); // 150 is not a power of two
  assert(result != NULL);

  buddy_free(&pool, result);
  check_buddy_pool_full(&pool);

  buddy_destroy(&pool);
}

/**
 * Test buddy_malloc with multiple allocations until the pool is exhausted.
 */
void test_buddy_malloc_exhaust_pool(void) {
  fprintf(stderr, "->Testing buddy_malloc with multiple allocations until pool is exhausted\n");
  struct buddy_pool pool;
  size_t pool_size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, pool_size);

  size_t block_size = 64; // Allocate blocks of size 64 bytes
  size_t num_blocks = pool_size / block_size; // Define num_blocks based on pool size and block size
  void **blocks = malloc(num_blocks * sizeof(void *));
  if (blocks == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    buddy_destroy(&pool);
    return;
  }

  for (size_t i = 0; i < num_blocks; i++) {
    blocks[i] = buddy_malloc(&pool, block_size);
    assert(blocks[i] != NULL);
  }

  // Attempt to allocate one more block, which should fail
  errno = 0; // Reset errno before the call
  void *extra_block = buddy_malloc(&pool, block_size);
  assert(extra_block == NULL);
  assert(errno == ENOMEM);

  // Free all allocated blocks
  for (size_t i = 0; i < num_blocks; i++) {
    buddy_free(&pool, blocks[i]);
  }

  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

int main(void) {
time_t t;
  unsigned seed = (unsigned)time(&t);
  fprintf(stderr, "Random seed:%d\n", seed);
  srand(seed);
  printf("Running memory tests.\n");
  UNITY_BEGIN();
  RUN_TEST(test_buddy_init);
  RUN_TEST(test_buddy_malloc_one_byte);
  RUN_TEST(test_buddy_malloc_one_large);
  RUN_TEST(test_buddy_malloc_multiple_blocks);
  RUN_TEST(test_btok_exact_power_of_two);
  RUN_TEST(test_btok_non_power_of_two);
  RUN_TEST(test_btok_large_values);
  RUN_TEST(test_buddy_calc_middle);
  RUN_TEST(test_buddy_calc_first_block);
  RUN_TEST(test_buddy_calc_last_block);
  RUN_TEST(test_buddy_free_valid_block);
  RUN_TEST(test_buddy_free_null_pointer);
  RUN_TEST(test_buddy_free_invalid_pointer);
  RUN_TEST(test_buddy_malloc_null_pool);
  RUN_TEST(test_buddy_malloc_zero_size);
  RUN_TEST(test_buddy_malloc_size_larger_than_pool);
  RUN_TEST(test_buddy_malloc_exact_power_of_two);
  RUN_TEST(test_buddy_malloc_non_power_of_two);
  RUN_TEST(test_buddy_malloc_exhaust_pool);
  return UNITY_END();
}