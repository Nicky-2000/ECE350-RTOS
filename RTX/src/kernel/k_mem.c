/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

/** 
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#include "common_ext.h"
#ifdef DEBUG_0
#include "printf.h"
#endif  /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
#define MAGIC_NUM 42069000
extern TCB *gp_current_task;
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

//process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

free_space_header *head;
BOOL memory_initialized = FALSE;

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32* k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid+1];
}

U32* k_alloc_p_stack(U32 size)
{
	U32* ret_val = k_mem_alloc_with_tid(size, 256);

	if (!ret_val) {
		return ret_val;
	}

	ret_val = (U32*)((U8*)ret_val + size);

	return ret_val;
}

int k_dealloc_p_stack(void *ptr, U32 size)
{

	ptr = (void*)((U8*)ptr - size);

    return k_mem_dealloc_with_tid(ptr, 256);
}

int k_mem_init(void) {
    unsigned int end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
#ifdef DEBUG_0
    printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
    printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
#endif /* DEBUG_0 */

    if(memory_initialized){
    	return RTX_ERR;
    }

    U32 end_addr_aligned = end_addr;
    end_addr_aligned += 7;
    end_addr_aligned &= ~7;

    U32 total_size = (RAM_END - end_addr_aligned);

    // create the head
    head = (free_space_header*) end_addr_aligned;
	head->size = total_size - sizeof(free_space_header);
	head->next = NULL;

	memory_initialized = TRUE;

#ifdef DEBUG_0
	printf("free_head_size: %u\r\n", sizeof(free_space_header));
	printf("total size: %u\r\n", total_size);
#endif

	if (total_size < sizeof(free_space_header)) {
		return RTX_ERR;
	}

    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_alloc: requested memory size = %d\r\n", size);
#endif /* DEBUG_0 */
    task_t tid = gp_current_task->tid;

    return k_mem_alloc_with_tid(size, (U16)tid);
}

int k_mem_dealloc(void *ptr) {
#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);
    printf("k_mem_dealloc: TID: %d\r\n", gp_current_task->tid);

#endif /* DEBUG_0 */
    /*
     * check ptr they gave is valid
     * check if mem before allocated chunk is free
     * check if mem after is free
     * coalesce mem
     */

    task_t tid = gp_current_task->tid;
    return k_mem_dealloc_with_tid(ptr, (U16)tid);
}

int k_mem_count_extfrag(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */
    int count = 0;

    if (!memory_initialized)
    {
      return 0;
    }

    free_space_header *p_curr = head;

    while (p_curr != NULL)
    {
      if ((size_t)(p_curr->size + sizeof(free_space_header)) < size && p_curr->size > 0)
      {
        ++count;
      }
      p_curr = p_curr->next;
    }

    return count;
}

void init_allocated_space_header(allocated_space_header* ptr, U32 size, U16 tid){
	ptr->size = size;
	ptr->magic_num = MAGIC_NUM + tid;
}


void* k_mem_alloc_with_tid(size_t size, U16 tid) {
#ifdef DEBUG_0
    printf("k_mem_alloc_with_tid: requested memory size = %d\r\n", size);
#endif /* DEBUG_0 */
    if(!memory_initialized){
    	return NULL;
    }

	// loop though the linked list to see if there is a valid free space
	// if so then give it over and move the pointers around. Else Return NULL
    size_t total_bytes_needed = size + sizeof(allocated_space_header);
    total_bytes_needed += 7;
    total_bytes_needed &= ~7;

	// if head block does not have enough space
	if (total_bytes_needed > head->size + sizeof(free_space_header)){
		free_space_header* prev_ptr = head;
		free_space_header* ptr = head->next;

		while (ptr != NULL){
			if (total_bytes_needed > ptr->size + sizeof(free_space_header)){
				prev_ptr = ptr;
				ptr = ptr->next;

				// perfect fit or no room free_space_header
			} else if ((ptr->size + sizeof(free_space_header) - total_bytes_needed) < sizeof(free_space_header)){
				prev_ptr->next = ptr->next;
				void* user_addr = (allocated_space_header*) ptr + 1;
				U32 alloc_size = ptr->size + sizeof(free_space_header) - sizeof(allocated_space_header); // sizeof the block + size of free header - size of alloc header
				init_allocated_space_header((allocated_space_header*) ptr, alloc_size, tid);
				return user_addr;

			// does fit, enough space for at least free_space_header
			} else {
				// free space related
				free_space_header* ptr_tmp = ptr;
				ptr = (free_space_header*) ((U8*)ptr + total_bytes_needed);
				*ptr = *ptr_tmp;
				ptr->size -= total_bytes_needed;
				prev_ptr->next = ptr;

				// user  related
				void* user_addr = (allocated_space_header*)ptr_tmp + 1;
				U32 alloc_size = total_bytes_needed - sizeof(allocated_space_header);
				init_allocated_space_header((allocated_space_header*) ptr_tmp, alloc_size, tid);

				return user_addr;

			}
		} // if head block has perfect fit or no room free_space_header
	} else if ((head->size + sizeof(free_space_header) - total_bytes_needed) < sizeof(free_space_header)) {
		free_space_header* head_tmp = head;
		head = head->next;

		void* user_addr = (allocated_space_header*) head_tmp + 1;
		U32 alloc_size = total_bytes_needed - sizeof(allocated_space_header);
		init_allocated_space_header((allocated_space_header*) head_tmp, alloc_size, tid);

		return user_addr;
	} else { // does fit, enough space for at least a free_space_header
		// create the free region header
		free_space_header* head_tmp = head;
		head = (free_space_header*) ((U8*)head + total_bytes_needed);
		*head = *head_tmp;
		head->size = head_tmp->size - total_bytes_needed;

		// create user allocated space
		void* user_addr = (allocated_space_header*) head_tmp + 1;
		U32 alloc_size = total_bytes_needed - sizeof(allocated_space_header);
		init_allocated_space_header((allocated_space_header*) head_tmp, alloc_size, tid);

		return user_addr;
	}

	return NULL;
}

int k_mem_dealloc_with_tid(void *ptr, U16 tid) {
#ifdef DEBUG_0
    printf("k_mem_dealloc_with_tid: freeing 0x%x\r\n", (U32) ptr);
#endif /* DEBUG_0 */
    /*
     * check ptr they gave is valid
     * check if mem before allocated chunk is free
     * check if mem after is free
     * coalesce mem
     */

    allocated_space_header *p_start = (allocated_space_header *)ptr - 1;

    // confirm ptr is valid
    if (ptr == NULL || ((p_start->magic_num - tid) != MAGIC_NUM) || !memory_initialized)
    {
      return RTX_ERR;
    }

    p_start->magic_num = 0; // to prevent double deallocation
    U32 block_size = p_start->size;
    // loop through free space LL

    // block is before head - make new head
    if (head > (free_space_header *)p_start || !head)
    {
      free_space_header *head_tmp = head;
      head = (free_space_header *)p_start;

      // allocated space is right before the current head
      if (head_tmp == (free_space_header *)((U8 *)p_start + block_size + sizeof(allocated_space_header)))
      {
        *head = *head_tmp;
        head->size += block_size + sizeof(allocated_space_header);
      }
      else
      { // allocated block some point before head, not directly preceding
        head->next = head_tmp;
        head->size = block_size + sizeof(allocated_space_header) - sizeof(free_space_header);
      }
    }
    else
    {
      free_space_header *p_prev = head;
      free_space_header *p_curr = head->next;
      while (p_curr != NULL)
      {
        if (p_curr < (free_space_header *)p_start)
        {
          p_prev = p_curr;
          p_curr = p_curr->next;
        }
        else
        {
          BOOL touching_prev = (free_space_header *)((U8 *)p_prev + p_prev->size + sizeof(free_space_header)) == (free_space_header *)p_start;
          BOOL touching_curr = p_curr == (free_space_header *)((U8 *)p_start + block_size + sizeof(allocated_space_header));
          //make into switch statement
          // p_prev is right before the allocated block
          if (touching_prev && !touching_curr)
          {
            p_prev->size += sizeof(allocated_space_header) + block_size;
          }
          else if (touching_prev && touching_curr)
          {
            p_prev->size += sizeof(allocated_space_header) + block_size + sizeof(free_space_header) + p_curr->size;
            p_prev->next = p_curr->next;
          }
          else if (!touching_prev && touching_curr)
          {
            free_space_header *p_curr_new = (free_space_header *)p_start;
            p_curr_new->size = p_curr->size + sizeof(allocated_space_header) + block_size;
            p_curr_new->next = p_curr->next;
            p_prev->next = p_curr_new;
          }
          else
          { // touching neither
            free_space_header *p_curr_new = (free_space_header *)p_start;
            p_prev->next = p_curr_new;
            p_curr_new->size = block_size + sizeof(allocated_space_header) - sizeof(free_space_header);
            p_curr_new->next = p_curr;
          }
          return RTX_OK;
        }
      }

      // Freed block is not touching the beginning of any block or the end
      // of any block except the tail of the free space linked list
      if ((allocated_space_header *)((U8 *)p_prev + p_prev->size + sizeof(free_space_header)) == p_start)
      {
        p_prev->size += sizeof(allocated_space_header) + block_size;
      }
      else
      {
        free_space_header *p_curr_new = (free_space_header *)p_start;
        p_prev->next = p_curr_new;
        p_curr_new->size = block_size + sizeof(allocated_space_header) - sizeof(free_space_header);
        p_curr_new->next = NULL;
      }
    }

    return RTX_OK;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
