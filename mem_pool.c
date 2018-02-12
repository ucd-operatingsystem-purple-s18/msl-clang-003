/*
 * Created by Ivo Georgiev on 2/9/16.
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h> // for perror()

#include "mem_pool.h"

/*************/
/*           */
/* Constants */
/*           */
/*************/
static const float      MEM_FILL_FACTOR                 = 0.75;
static const unsigned   MEM_EXPAND_FACTOR               = 2;

static const unsigned   MEM_POOL_STORE_INIT_CAPACITY    = 20;
static const float      MEM_POOL_STORE_FILL_FACTOR      = 0.75;
static const unsigned   MEM_POOL_STORE_EXPAND_FACTOR    = 2;

static const unsigned   MEM_NODE_HEAP_INIT_CAPACITY     = 40;
static const float      MEM_NODE_HEAP_FILL_FACTOR       = 0.75;
static const unsigned   MEM_NODE_HEAP_EXPAND_FACTOR     = 2;

static const unsigned   MEM_GAP_IX_INIT_CAPACITY        = 40;
static const float      MEM_GAP_IX_FILL_FACTOR          = 0.75;
static const unsigned   MEM_GAP_IX_EXPAND_FACTOR        = 2;



/*********************/
/*                   */
/* Type declarations */
/*                   */
/*********************/
typedef struct _alloc {
    char *mem;
    size_t size;
} alloc_t, *alloc_pt;

typedef struct _node {
    alloc_t alloc_record;
    unsigned used;
    unsigned allocated;
    struct _node *next, *prev; // doubly-linked list for gap deletion
} node_t, *node_pt;

typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

typedef struct _pool_mgr {
    pool_t pool;
    node_pt node_heap;
    unsigned total_nodes;
    unsigned used_nodes;
    gap_pt gap_ix;
    unsigned gap_ix_capacity;
} pool_mgr_t, *pool_mgr_pt;



/***************************/
/*                         */
/* Static global variables */
/*                         */
/***************************/
static pool_mgr_pt *pool_store = NULL; // an array of pointers, only expand
static unsigned pool_store_size = 0;
static unsigned pool_store_capacity = 0;



/********************************************/
/*                                          */
/* Forward declarations of static functions */
/*                                          */
/********************************************/
static alloc_status _mem_resize_pool_store();
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr);
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status
        _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                           size_t size,
                           node_pt node);
static alloc_status
        _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                size_t size,
                                node_pt node);
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status _mem_invalidate_gap_ix(pool_mgr_pt pool_mgr);


/****************************************/
/*                                      */
/* Definitions of user-facing functions */
/*                                      */
/****************************************/
alloc_status mem_init()
{
    // ensure that it's called only once until mem_free
    // allocate the pool store with initial capacity
    // note: holds pointers only, other functions to allocate/deallocate

    // first check if pool_store == NULL
    if(pool_store == NULL)
    {
        /* empty so we need to init
         * first point pool_store to a pool of memory
         * then set initial capacity
         * then make the size of pool store 0
         *
         */
        pool_store = (pool_mgr_pt*) calloc(MEM_POOL_STORE_INIT_CAPACITY, sizeof(pool_mgr_pt));
        pool_store_capacity = MEM_POOL_STORE_INIT_CAPACITY;
        pool_store_size = 0;

        return ALLOC_OK;
    }
    // non-empty, which means it has already been called
    return ALLOC_CALLED_AGAIN;
}

alloc_status mem_free()
{
    // ensure that it's called only once for each mem_init
    // make sure all pool managers have been deallocated
    // can free the pool store array
    // update static variables

    // first check if pool_store == NULL
    if(pool_store == NULL)
    {
        return ALLOC_CALLED_AGAIN;
    }
    /* pool_store is an array of pointers
     * first loop through each element of array
     */
    for(int i = 0; i < pool_store_size; i++)
    {
        // if not null we fail
        if(pool_store[i] != NULL)
        {
            return ALLOC_FAIL;
        }

    }
    // free pool_store and set to null
    free(pool_store);
    pool_store = NULL;

    // update static variables
    pool_store_size = 0;
    pool_store_capacity = 0;
    return ALLOC_OK;
}

pool_pt mem_pool_open(size_t size, alloc_policy policy) {
    // make sure there the pool store is allocated
    // expand the pool store, if necessary
    // allocate a new mem pool mgr
    // check success, on error return null
    // allocate a new memory pool
    // check success, on error deallocate mgr and return null
    // allocate a new node heap
    // check success, on error deallocate mgr/pool and return null
    // allocate a new gap index
    // check success, on error deallocate mgr/pool/heap and return null
    // assign all the pointers and update meta data:
    //   initialize top node of node heap
    //   initialize top node of gap index
    //   initialize pool mgr
    //   link pool mgr to pool store
    // return the address of the mgr, cast to (pool_pt)

    // check to see if pool store needs to be allocated
    // we don't need to do anything unless ALLOC_FAIL returned

    if(pool_store == NULL)
    {
        return NULL;
    }


    _mem_resize_pool_store();

    // allocate a new mem pool mgr
    pool_mgr_pt mem_mgr = calloc(1, sizeof(pool_mgr_pt));
    // check if successful
    if(mem_mgr == NULL)
    {
        return NULL;
    }

    // allocate new memory pool
    mem_mgr->pool.mem = (char*) calloc(size, sizeof(char));
    mem_mgr->pool.num_allocs = 0;
    mem_mgr->pool.policy = policy;
    mem_mgr->pool.num_gaps = 1;
    mem_mgr->pool.alloc_size = 0;
    mem_mgr->pool.total_size = size;

    // check if successful
    if(&mem_mgr->pool == NULL)
    {
        free(mem_mgr);
        return NULL;
    }


    // allocate new node heap
    mem_mgr->node_heap = (node_pt) calloc(MEM_NODE_HEAP_INIT_CAPACITY, sizeof(node_t));
    // check if successful
    if(mem_mgr->node_heap == NULL)
    {
        // free pool and mem mgr
        free(mem_mgr->pool.mem);
        free(mem_mgr);
        return NULL;
    }

    // allocate new gap index
    mem_mgr->gap_ix = (gap_pt) calloc(MEM_GAP_IX_INIT_CAPACITY, sizeof(gap_t));
    // check if successful
    if(mem_mgr->gap_ix == NULL)
    {
        // free pool and mem mgr and node heap
        free(mem_mgr->pool.mem);
        free(mem_mgr->node_heap);
        free(mem_mgr);
        return NULL;
    }

    // initialize top node of node heap
    mem_mgr->node_heap[0].allocated = 0;
    mem_mgr->node_heap[0].next = NULL;
    mem_mgr->node_heap[0].prev = NULL;
    mem_mgr->node_heap[0].used = 1;
    mem_mgr->node_heap[0].alloc_record.size = size;
    mem_mgr->node_heap[0].alloc_record.mem = mem_mgr->pool.mem;

     // initialize top node of gap index
    mem_mgr->gap_ix[0].size = size;
    mem_mgr->gap_ix[0].node = mem_mgr->node_heap;

    // initialize pool mgr
    mem_mgr->gap_ix_capacity = MEM_GAP_IX_INIT_CAPACITY;
    mem_mgr->total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;
    mem_mgr->used_nodes = 1;

    // link pool mgr to pool store
    pool_store[pool_store_size] = mem_mgr;
    // increment pool_store_size
    pool_store_size++;

    // return the address of the mgr, cast to (pool_pt)
    return (pool_pt)mem_mgr;
}

alloc_status mem_pool_close(pool_pt pool) {
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // check if this pool is allocated
    // check if pool has only one gap
    // check if it has zero allocations
    // free memory pool
    // free node heap
    // free gap index
    // find mgr in pool store and set to null
    // note: don't decrement pool_store_size, because it only grows
    // free mgr

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt mem_mgr = (pool_mgr_pt) pool;

    // check if this pool is allocated
    if(mem_mgr->pool.mem == NULL)
    {
        return ALLOC_NOT_FREED;
    }

    // check if pool has only one gap
    if(mem_mgr->pool.num_gaps == 1)
    {
        return ALLOC_NOT_FREED;
    }

    // check if it has zero allocations
    if(mem_mgr->pool.alloc_size == 0)
    {
        return ALLOC_NOT_FREED;
    }

    // free memory pool
    free(mem_mgr->pool.mem);

    // free node heap
    free(mem_mgr->node_heap);

    // free gap index
    free(mem_mgr->gap_ix);

    // find mgr in pool store and set to null
    for(int i = 0; i < pool_store_size; i++)
    {
        if(pool_store[i] == mem_mgr)
        {
            pool_store[i] = NULL;
            break;
        }
    }

    // free mgr
    free(mem_mgr);

    return ALLOC_OK;
}

void * mem_new_alloc(pool_pt pool, size_t size) {
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // check if any gaps, return null if none
    // expand heap node, if necessary, quit on error
    // check used nodes fewer than total nodes, quit on error
    // get a node for allocation:
    // if FIRST_FIT, then find the first sufficient node in the node heap
    // if BEST_FIT, then find the first sufficient node in the gap index
    // check if node found
    // update metadata (num_allocs, alloc_size)
    // calculate the size of the remaining gap, if any
    // remove node from gap index
    // convert gap_node to an allocation node of given size
    // adjust node heap:
    //   if remaining gap, need a new node
    //   find an unused one in the node heap
    //   make sure one was found
    //   initialize it to a gap node
    //   update metadata (used_nodes)
    //   update linked list (new node right after the node for allocation)
    //   add to gap index
    //   check if successful
    // return allocation record by casting the node to (alloc_pt)

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt mem_mgr = (pool_mgr_pt) pool;

    // check if any gaps, return null if none
    if(mem_mgr->pool.num_gaps == 0)
    {
        return NULL;
    }

    // expand heap node, if necessary, quit on error
    if(_mem_resize_node_heap(mem_mgr) == ALLOC_FAIL)
    {
        return NULL;
    }

    // check used nodes fewer than total nodes, quit on error
    if(mem_mgr->used_nodes >= mem_mgr->total_nodes)
    {
        return NULL;
    }

    // get a node for allocation:
    node_pt temp_node = NULL;

    // if FIRST_FIT, then find the first sufficient node in the node heap
    if(mem_mgr->pool.policy == FIRST_FIT)
    {
        /* need to check if node is used, is allocated, and if
         * the allocated size is larger than size
         */
        for(int i = 0; i < mem_mgr->total_nodes; i++)
        {
            if(!(mem_mgr->node_heap[i].allocated) && mem_mgr->node_heap[i].used && mem_mgr->node_heap[i].alloc_record.size >= size)
            {
                temp_node = &mem_mgr->node_heap[i];
                break;
            }
        }
    }

    // if BEST_FIT, then find the first sufficient node in the gap index
    if(mem_mgr->pool.policy == BEST_FIT)
    {
        /* need to check if gap size is greater than size
         */
        for(int i = 0; i < mem_mgr->gap_ix_capacity; i++)
        {
            if (mem_mgr->gap_ix[i].size >= size)
            {
                temp_node = mem_mgr->gap_ix[i].node;
                break;
            }
        }
    }

    // check if node found
    if(temp_node == NULL)
    {
        return NULL;
    }

    // update metadata (num_allocs, alloc_size)
    mem_mgr->pool.num_allocs++;
    mem_mgr->pool.alloc_size += size;

    // calculate the size of the remaining gap, if any
    size_t rem_gap = mem_mgr->gap_ix->size - size;

    // remove node from gap index
    _mem_remove_from_gap_ix(mem_mgr, size, temp_node);

    // convert gap_node to an allocation node of given size
    temp_node->alloc_record.size = size;
    temp_node->allocated = 1;
    temp_node->used = 1;

    // adjust node heap:
    //   if remaining gap, need a new node
    if(rem_gap != 0)
    {
        node_pt new_node = NULL;
        //   find an unused one in the node heap
        for(int i = 0; i < mem_mgr->total_nodes; i++)
        {
            if(!(mem_mgr->node_heap[i].used))
            {
                new_node = &mem_mgr->node_heap[i];
                break;
            }
        }
        //   make sure one was found
        if(new_node == NULL)
        {
            return NULL;
        }

        //   initialize it to a gap node
        new_node->allocated = 0;
        new_node->used = 1;
        new_node->next = NULL;
        new_node->prev = NULL;
        new_node->alloc_record.size = rem_gap;
        new_node->alloc_record.mem = temp_node->alloc_record.mem + size;

        //   update metadata (used_nodes)
        mem_mgr->used_nodes++;

        //   update linked list (new node right after the node for allocation)
        new_node->prev = temp_node;
        new_node->next = temp_node->next;
        // update temp_node next information
        if(temp_node->next != NULL)
        {
            temp_node->next->prev = new_node;
        }
        temp_node->next = new_node;

        //   add to gap index
        //   check if successful
        if(_mem_add_to_gap_ix(mem_mgr, rem_gap, new_node) == ALLOC_FAIL)
        {
            return NULL;
        }
    }

    // return allocation record by casting the node to (alloc_pt)
    return (alloc_pt) temp_node;

}

alloc_status mem_del_alloc(pool_pt pool, void * alloc) {
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // get node from alloc by casting the pointer to (node_pt)
    // find the node in the node heap
    // this is node-to-delete
    // make sure it's found
    // convert to gap node
    // update metadata (num_allocs, alloc_size)
    // if the next node in the list is also a gap, merge into node-to-delete
    //   remove the next node from gap index
    //   check success
    //   add the size to the node-to-delete
    //   update node as unused
    //   update metadata (used nodes)
    //   update linked list:
    /*
                    if (next->next) {
                        next->next->prev = node_to_del;
                        node_to_del->next = next->next;
                    } else {
                        node_to_del->next = NULL;
                    }
                    next->next = NULL;
                    next->prev = NULL;
     */

    // this merged node-to-delete might need to be added to the gap index
    // but one more thing to check...
    // if the previous node in the list is also a gap, merge into previous!
    //   remove the previous node from gap index
    //   check success
    //   add the size of node-to-delete to the previous
    //   update node-to-delete as unused
    //   update metadata (used_nodes)
    //   update linked list
    /*
                    if (node_to_del->next) {
                        prev->next = node_to_del->next;
                        node_to_del->next->prev = prev;
                    } else {
                        prev->next = NULL;
                    }
                    node_to_del->next = NULL;
                    node_to_del->prev = NULL;
     */
    //   change the node to add to the previous node!
    // add the resulting node to the gap index
    // check success


    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt mem_mgr = (pool_mgr_pt) pool;

    // get node from alloc by casting the pointer to (node_pt)
    node_pt temp_node = (node_pt) alloc;

    // boolean for found
    int bool_found = 0;

    // find the node in the node heap
    for(int i = 0; i < mem_mgr->total_nodes; i++)
    {
        if(temp_node == &mem_mgr->node_heap[i])
        {
            bool_found = 1;
            break;
        }
    }
    // this is node-to-delete
    // make sure it's found
    if(!bool_found)
    {
        return ALLOC_NOT_FREED;
    }

    // convert to gap node
    temp_node->allocated = 0;
    temp_node->used = 1;

    // update metadata (num_allocs, alloc_size)
    mem_mgr->pool.num_allocs--;
    mem_mgr->pool.alloc_size -= temp_node->alloc_record.size;

    // if the next node in the list is also a gap, merge into node-to-delete
    if(temp_node->next != NULL && temp_node->next->allocated == 0)
    {
        //   remove the next node from gap index
        //   check success
        if(_mem_remove_from_gap_ix(mem_mgr,temp_node->alloc_record.size, temp_node->next) == ALLOC_NOT_FREED)
        {
            return ALLOC_NOT_FREED;
        }

        //   add the size to the node-to-delete
        temp_node->alloc_record.size += temp_node->next->alloc_record.size;

        //   update node as unused
        temp_node->used = 0;
        temp_node->alloc_record.size = 0;
        temp_node->alloc_record.mem = NULL;

        //   update metadata (used nodes)
        mem_mgr->used_nodes--;

        //   update linked list:
        /*
                    if (next->next) {
                        next->next->prev = node_to_del;
                        node_to_del->next = next->next;
                    } else {
                        node_to_del->next = NULL;
                    }
                    next->next = NULL;
                    next->prev = NULL;
        */
        if (temp_node->next->next != NULL)
        {
            temp_node->next->next->prev = temp_node;
            temp_node->next = temp_node->next->next;
        }
        else
        {
            temp_node->next = NULL;
        }
        temp_node->next->next = NULL;
        temp_node->next->prev = NULL;
    }

    // this merged node-to-delete might need to be added to the gap index
    // but one more thing to check...
    // if the previous node in the list is also a gap, merge into previous!
    if(temp_node->prev != NULL && !(temp_node->prev->allocated))
    {

        //   remove the previous node from gap index
        //   check success
        if(_mem_remove_from_gap_ix(mem_mgr, temp_node->prev->alloc_record.size, temp_node) == ALLOC_NOT_FREED)
        {
            return ALLOC_NOT_FREED;
        }

        //   add the size of node-to-delete to the previous
        temp_node->prev->alloc_record.size += temp_node->alloc_record.size;

        //   update node-to-delete as unused
        temp_node->used = 0;
        temp_node->alloc_record.size = 0;
        temp_node->alloc_record.mem = NULL;

        //   update metadata (used_nodes)
        mem_mgr->used_nodes--;

        //   update linked list
        /*
                        if (node_to_del->next) {
                            prev->next = node_to_del->next;
                            node_to_del->next->prev = prev;
                        } else {
                            prev->next = NULL;
                        }
                        node_to_del->next = NULL;
                        node_to_del->prev = NULL;
         */
        if(temp_node->next != NULL)
        {
            temp_node->prev->next = temp_node->next;
            temp_node->next->prev = temp_node->prev;
        }
        else
        {
            temp_node->prev->next = NULL;
        }
        temp_node->next = NULL;
        temp_node->prev = NULL;

        //   change the node to add to the previous node!
        temp_node = temp_node->prev;
    }

    // add the resulting node to the gap index
    // check success
    if(_mem_add_to_gap_ix(mem_mgr, temp_node->alloc_record.size, temp_node) == ALLOC_FAIL)
    {
        return ALLOC_NOT_FREED;
    }

    return ALLOC_OK;
}

void mem_inspect_pool(pool_pt pool,
                      pool_segment_pt *segments,
                      unsigned *num_segments) {
    // get the mgr from the pool
    // allocate the segments array with size == used_nodes
    // check successful
    // loop through the node heap and the segments array
    //    for each node, write the size and allocated in the segment
    // "return" the values:
    /*
                    *segments = segs;
                    *num_segments = pool_mgr->used_nodes;
     */

    // get the mgr from the pool
    pool_mgr_pt mem_mgr = (pool_mgr_pt) pool;

    // allocate the segments array with size == used_nodes
    pool_segment_pt pool_seg = malloc(mem_mgr->used_nodes * sizeof(pool_segment_pt));

    // check successful
    if(pool_seg != NULL)
    {

        // loop through the node heap and the segments array
        //    for each node, write the size and allocated in the segment
        for(int i = 0; i < mem_mgr->used_nodes; i++)
        {
            pool_seg->size = mem_mgr->node_heap[i].alloc_record.size;
            pool_seg->allocated = mem_mgr->node_heap[i].allocated;
        }
    }
    // "return" the values:
    /*
                    *segments = segs;
                    *num_segments = pool_mgr->used_nodes;
     */
    *segments = pool_seg;
    *num_segments = mem_mgr->used_nodes;
}



/***********************************/
/*                                 */
/* Definitions of static functions */
/*                                 */
/***********************************/
static alloc_status _mem_resize_pool_store() {
    // check if necessary
    /*
                if (((float) pool_store_size / pool_store_capacity)
                    > MEM_POOL_STORE_FILL_FACTOR) {...}
     */
    // don't forget to update capacity variables

    if(((float)pool_store_size / pool_store_capacity) > MEM_POOL_STORE_FILL_FACTOR){
        unsigned int oldSize = pool_store_size;
        pool_store_size = pool_store_size * MEM_POOL_STORE_EXPAND_FACTOR;
        pool_store = (pool_mgr_pt  *)realloc(pool_store,sizeof(pool_mgr_pt) * pool_store_size);
        for(size_t i = oldSize; i< pool_store_size; i++){
            pool_store[i] = NULL;
        }

        if(pool_store != NULL){
            return ALLOC_OK;
        }
    }

    return ALLOC_FAIL;
}

static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr) {
    // see above
    if(((float)pool_mgr->used_nodes / pool_mgr->total_nodes) > MEM_NODE_HEAP_FILL_FACTOR){
        pool_mgr->total_nodes *= MEM_NODE_HEAP_EXPAND_FACTOR;
        pool_mgr->node_heap = (node_pt) realloc(pool_mgr->node_heap, sizeof(node_pt) * pool_mgr->total_nodes);

        for(size_t i = pool_mgr->used_nodes ; i < pool_mgr->total_nodes; i++){
            node_t nd;
            nd.used = 0;
            nd.allocated = 0;
            pool_mgr->node_heap[i] = nd;
        }

        if(pool_mgr == NULL){
            return ALLOC_FAIL;
        }
    }

    return ALLOC_FAIL;
}

static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr) {
    // see above
    if(((float)pool_mgr->pool.num_gaps /pool_mgr->gap_ix_capacity) > MEM_GAP_IX_FILL_FACTOR){
        unsigned int oldSize = pool_mgr->gap_ix_capacity;
        pool_mgr->gap_ix_capacity *= MEM_GAP_IX_EXPAND_FACTOR;
        pool_mgr->gap_ix = (gap_pt) realloc(pool_mgr->gap_ix,sizeof(gap_pt)*pool_mgr->gap_ix_capacity);

        for(size_t i = oldSize; i < pool_mgr->gap_ix_capacity;i++){
            gap_t gp;
            gp.node = NULL;
            gp.size = 0;
            pool_mgr->gap_ix[i] = gp;
        }
        if(pool_mgr->gap_ix != NULL) {
            return ALLOC_OK;
        }
    }

    return ALLOC_FAIL;
}

static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr,
                                       size_t size,
                                       node_pt node) {

    // expand the gap index, if necessary (call the function)
    // add the entry at the end
    // update metadata (num_gaps)
    // sort the gap index (call the function)
    // check success
    _mem_resize_gap_ix(pool_mgr);

    gap_t gap;
    gap.node = node;
    gap.size = size;


    pool_mgr->gap_ix[pool_mgr->pool.num_gaps] = gap;

    gap_pt cg = &pool_mgr->gap_ix[pool_mgr->pool.num_gaps];

    if(cg->node != node){
        printf("Failed to Allocate to Gaps\n");
        return ALLOC_FAIL;
    }

    pool_mgr->pool.num_gaps += 1;
    return _mem_sort_gap_ix(pool_mgr);

    return ALLOC_FAIL;
}

static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr,
                                            size_t size,
                                            node_pt node) {
    // find the position of the node in the gap index
    // loop from there to the end of the array:
    //    pull the entries (i.e. copy over) one position up
    //    this effectively deletes the chosen node
    // update metadata (num_gaps)
    // zero out the element at position num_gaps!

    size_t idx =0;
    while(idx < pool_mgr->pool.num_gaps && pool_mgr->gap_ix[idx].node != node){
        idx += 1;
    }

    if(idx == pool_mgr->pool.num_gaps){
        printf("Failed to Remove Gap\n");
        return ALLOC_FAIL;
    }

    for(size_t i = idx;i < pool_mgr->pool.num_gaps;i++){
        pool_mgr->gap_ix[i] = pool_mgr->gap_ix[i+1];
    }

    gap_t gap;
    gap.size =0;
    gap.node = NULL;
    pool_mgr->gap_ix[pool_mgr->pool.num_gaps] = gap;


    pool_mgr->pool.num_gaps -= 1;
    return ALLOC_OK;
}

// note: only called by _mem_add_to_gap_ix, which appends a single entry
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr) {
    // the new entry is at the end, so "bubble it up"
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //    or if the sizes are the same but the current entry points to a
    //    node with a lower address of pool allocation address (mem)
    //       swap them (by copying) (remember to use a temporary variable)
    size_t start = pool_mgr->pool.num_gaps - 1;
    while(start > 0 && pool_mgr->gap_ix[start].size < pool_mgr->gap_ix[start -1].size){
        gap_t temp = pool_mgr->gap_ix[start];
        pool_mgr->gap_ix[start] = pool_mgr->gap_ix[start -1];
        start -=1;
        pool_mgr->gap_ix[start] = temp;
    }


    //using start loop until lowest address
    while(start > 0 && pool_mgr->gap_ix[start].size == pool_mgr->gap_ix[start - 1].size && pool_mgr->gap_ix[start].node->alloc_record.mem < pool_mgr->gap_ix[start -1].node->alloc_record.mem){
        gap_t temp = pool_mgr->gap_ix[start];
        pool_mgr->gap_ix[start] = pool_mgr->gap_ix[start -1];
        start -=1;
        pool_mgr->gap_ix[start] = temp;
    }

    return ALLOC_OK;
}

static alloc_status _mem_invalidate_gap_ix(pool_mgr_pt pool_mgr) {
    return ALLOC_FAIL;
}
