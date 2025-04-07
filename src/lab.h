#ifndef LAB_H
#define LAB_H
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif
/**
* The default amount of memory that this memory manger will manage unless
* explicitly set with buddy_init. The number of bytes is calculated as
2^DEFAULT_K
*/
#define DEFAULT_K 30
/**
* The minimum size of the buddy memory pool.
*/
#define MIN_K 20
/**
* The maximum size of the buddy memory pool. This is 1 larger than needed
* to allow indexes 1-N instead of 0-N. Internally the maximum amount of
* memory is MAX_K-1
*/
#define MAX_K 48
/**
* The smallest memory block size that can be returned by buddy_malloc value must
* be large enough to account for the avail header.
*/
#define SMALLEST_K 6
#define BLOCK_AVAIL 1 /*Block is available to allocate*/
#define BLOCK_RESERVED 0 /*Block has been handed to user*/
#define BLOCK_UNUSED 3 /*Block is not used at all*/
/**
* Struct to represent the table of all available blocks do not reorder members
* of this struct because internal calculations depend on the ordering.
*/
struct avail
{
unsigned short int tag; /*Tag for block status BLOCK_AVAIL,
BLOCK_RESERVED*/
unsigned short int kval; /*The kval of this block*/
struct avail *next; /*next memory block*/
struct avail *prev; /*prev memory block*/
};
/**
* The buddy memory pool.
*/
struct buddy_pool
{
};
#endif // LAB_H
