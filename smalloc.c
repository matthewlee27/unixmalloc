#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "smalloc.h"


/*
 * my_init() is called one time by the application program to to perform any 
 * necessary initializations, such as allocating the initial heap area.
 * size_of_region is the number of bytes that you should request from the OS using
 * mmap().
 * Note that you need to round up this amount so that you request memory in 
 * units of the page size, which is defined as 4096 Bytes in this project.
 */

struct block_header {
    int block_size;
    int allocated;
    struct block_header *next;
    struct block_header *prev;
};

struct block_header *explicit_list = NULL;
void *malloc_addr;
int init_block_size;

int my_init(int size_of_region) {
    int fd = open("/dev/zero", O_RDWR);
    init_block_size = ((size_of_region + 4096 - 1) / 4096) * 4096;
    malloc_addr = mmap(NULL, init_block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (malloc_addr == (void *) -1){
        return -1;
    }
    close(fd);
    
    struct block_header *current_header = (struct block_header *)malloc_addr;
    current_header->block_size = init_block_size;
    current_header->allocated = 0;
    current_header->prev = NULL;
    current_header->next = NULL;

    explicit_list = current_header;

    return 0;
}

/*
 * smalloc() takes as input the size in bytes of the payload to be allocated and 
 * returns a pointer to the start of the payload. The function returns NULL if 
 * there is not enough contiguous free space within the memory allocated 
 * by my_init() to satisfy this request.
 */
void *smalloc(int size_of_payload, Malloc_Status *status) {

    if (explicit_list == NULL) {
        status->success = 0;
        status->payload_offset = -1;
        status->hops = -1;
        return NULL;
    }

    /*round the payload*/
    int aligned_payload = ((size_of_payload + 7) / 8) * 8;
    // decl
    struct block_header *list_pointer = explicit_list;
    status->hops = 0;
    while (list_pointer->block_size - 24 < aligned_payload){
        if (list_pointer->next == NULL){
            status->success = 0;
            status->payload_offset = -1;
            status->hops = -1;
            return NULL;
        } else {
            list_pointer = list_pointer->next;
            status->hops++;
        }
    }

    /*dealing with the new partition*/
    if (aligned_payload <= list_pointer->block_size - 48){
        /* new partition */
        struct block_header *new_malloc_header = (struct block_header *)((char *)list_pointer + 24 + aligned_payload);
        new_malloc_header->block_size = list_pointer->block_size - (24 + aligned_payload);
        new_malloc_header->allocated = 0;
        new_malloc_header->prev = list_pointer->prev;
        new_malloc_header->next = list_pointer->next;
        if (list_pointer->prev != NULL){
            list_pointer->prev->next = new_malloc_header;
        }
        if (list_pointer->next != NULL){
            list_pointer->next->prev = new_malloc_header;
        }

        /*update explicit list if list_pointer is first in the explicit list*/
        if (new_malloc_header->prev == NULL){
            explicit_list = new_malloc_header;
        }
        
        list_pointer->block_size = aligned_payload + 24;
    } else {
        /* there is no new partition so we remove the current from the linked list*/
        list_pointer->allocated = 1;

        if (list_pointer->prev != NULL){
            list_pointer->prev->next = list_pointer->next;
        }
        if (list_pointer->next != NULL){
            list_pointer->next->prev = list_pointer->prev;
        }
        
        // update explicit list if list_pointer is first in the explicit list*/
        if (list_pointer->prev == NULL){
            explicit_list = list_pointer->next;
        }
    }
    /* old partition */
    list_pointer->allocated = 1;
    list_pointer->prev = NULL;
    list_pointer->next = NULL;

    status->success = 1;

    void *payload_pointer = (struct block_header *)(list_pointer + 1);
    status->payload_offset = (char *)payload_pointer - (char *)malloc_addr;
    return payload_pointer;
}


/*
 * sfree() frees the target block. "ptr" points to the start of the payload.
 * NOTE: "ptr" points to the start of the payload, rather than the block (header).
 */
void sfree(void *ptr)
{   
    if (ptr == NULL){
        return;
    }

    struct block_header *free_head = (struct block_header *)((char *)ptr - 24);
    free_head->allocated = 0;
    int p = free_head->block_size;

    int left_is_filled = 0;
    int right_is_filled = 0;
    
    /*Edge case: we free when the whole block is filled*/
    if (explicit_list == NULL) {
        free_head->allocated = 0;
        free_head->next = NULL;
        free_head->prev = NULL;
        explicit_list = free_head;
        return;
    }

    /* finding the right/left block */
    struct block_header *right_head;
    if ((char *)free_head + p == (char *)malloc_addr + init_block_size) {
        right_is_filled = 1;
    } else {
        right_head = (struct block_header *)((char *)free_head + p);
        if (right_head->allocated == 1){
            right_is_filled = 1;
        }
    }

    struct block_header *searcher = explicit_list;
    while (searcher->next != NULL && searcher->next < free_head){
        searcher = searcher->next;
    }
    struct block_header *left_head = searcher;
    if ((char *)left_head + left_head->block_size == (char *)free_head){
        left_is_filled = 0;
    } else {
        left_is_filled = 1;
        left_head = (struct block_header *)((char *)left_head + left_head->block_size);
    }
    
    /*Edge case: we free the first block*/
    if (free_head == malloc_addr && right_is_filled == 1){
        free_head->next = explicit_list;
        explicit_list->prev = free_head;
        explicit_list = free_head;
        return;
    }

    /*Case 1: filled - FREE - filled */
    if (left_is_filled == 1 && right_is_filled == 1){
        searcher = explicit_list;
        if (free_head < searcher){
            /*Case where we free a block before the first explicit free block*/
            free_head->next = searcher;
            free_head->prev = NULL;
            searcher->prev = free_head;
            explicit_list = free_head;
        } else {
            /*Case where we free a block that's in between explicit free blocks or at end*/
            while (searcher->next != NULL && searcher->next < free_head){
                searcher = searcher->next;
            }
            free_head->prev = searcher;
            free_head->next = searcher->next;
            if (searcher->next != NULL) {
                searcher->next->prev = free_head;
            }
            searcher->next = free_head;
        }
        free_head->allocated = 0;
        return;
    } else if (left_is_filled == 1 && right_is_filled == 0){
    /*Case #2: filled - FREE - FREE */
        free_head->block_size += right_head->block_size;
        if (right_head->prev != NULL){
            right_head->prev->next = free_head;
        } else {
            explicit_list = free_head;
        }
        if (right_head->next != NULL){
            right_head->next->prev = free_head;
        }
        free_head->next = right_head->next;
        free_head->prev = right_head->prev;
        
        free_head->allocated = 0;
        right_head->next = NULL;
        right_head->prev = NULL;
        return;
    } else if (left_is_filled == 0 && right_is_filled == 1){
    /*Case #3: FREE - FREE - filled */
        left_head->block_size += p;
        free_head->allocated = 0;

        return;
    } else {
    /*Case #4: FREE - FREE - FREE */
        left_head->block_size += p + right_head->block_size;
        if (right_head->next != NULL){
            right_head->next->prev = left_head;
        }
        left_head->next = right_head->next;

        free_head->allocated = 0;
        right_head->prev = NULL;
        right_head->next = NULL;
        return;
    }
}