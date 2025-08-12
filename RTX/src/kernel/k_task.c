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
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"
#include "tcb_storage.h"
#include "mailbox.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks

//all nodes of the tree that will contain priorities that are in use
rb_tree_node 	priorities[PRIO_NULL + 2];
rb_tree			tree;
//array of linked lists of tcbs hashed based on their priority
TCB 			*prio_list[PRIO_NULL + 1];

extern void kcd_task(void);
task_t current_tid = 1;

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

//TODO fix adding to and removing from an empty rb_tree - done

TCB *scheduler(void)
{

	/* ********DONE********
	 * ********************
	 * call min on rb tree to find highest priority out possible
	 * check that current task has lower priority
	 * if current task has higher or equal priority, return current task tcb
	 * if not pop pl min, add current task to ready list, return new min task
	*/
	rb_tree_node *temp_node = (rb_min(tree.root, &tree));
	if (temp_node == tree.nil) {
		return gp_current_task;
	}

	U8 min_prio = temp_node->priority;

	if(gp_current_task->prio < min_prio && gp_current_task->state != DORMANT
			&& gp_current_task->state != BLK_MSG) {
	    return gp_current_task;
	}

	if (gp_current_task->state != DORMANT && gp_current_task->state != BLK_MSG) {
		pl_insert(prio_list, gp_current_task, &tree, priorities + gp_current_task->prio);
	}

	return pl_pop_min(prio_list, min_prio, &tree, priorities + min_prio);
}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
    extern U32 SVC_RESTORE;

    rb_initialize_tree(priorities, &tree);
//    root = &(priorities[PRIO_NULL]);

	pl_initialize_lists(prio_list);
	g_tcbs[0].tid = 0;
	g_tcbs[0].prio = PRIO_NULL;
	g_tcbs[0].next = NULL;
	g_tcbs[0].mbx = NULL;

	for(int i = 1; i < MAX_TASKS; ++i){
		g_tcbs[i].tid = i;
		g_tcbs[i].next = NULL;
		g_tcbs[i].state = DORMANT;
		g_tcbs[i].mbx = NULL;

	}

//	prio_list[PRIO_NULL] = &(g_tcbs[0]);
//	pl_insert(prio_list, &(g_tcbs[0]), &tree, priorities + PRIO_NULL);

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    p_tcb->ptask	= NULL;
    g_num_active_tasks++;
    gp_current_task = p_tcb;

    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
    	TCB *p_tcb;
    	task_t tid;
    	if(p_taskinfo->ptask == &kcd_task){
    		tid = TID_KCD;
    		--i;
    		--num_tasks;
    		p_tcb = &g_tcbs[TID_KCD];
    	} else {
    		tid = i + 1;
            p_tcb = &g_tcbs[i+1];
    	}

        p_tcb->prio = p_taskinfo->prio;
        p_tcb->priv = p_taskinfo->priv;
        //*********DONE************
		//check if the task is a user task and if it is create its user stack and add it to the taks_info buffer
		//if the returned address is NULL don't run k_tsk_create_new
        //*************************
        p_tcb->ptask = p_taskinfo->ptask;
        if(p_taskinfo->priv == 0){
        	if(p_taskinfo->prio == PRIO_NULL || p_taskinfo->u_stack_size < 0x200
        			|| (p_taskinfo->u_stack_size & ~0x07) != p_taskinfo->u_stack_size || p_taskinfo->ptask == NULL){
				return RTX_ERR;
			}
            U32* u_stack = k_alloc_p_stack(p_taskinfo->u_stack_size);
            p_taskinfo->u_stack_hi = (U32)u_stack;
            p_tcb->u_stack_size = p_taskinfo->u_stack_size;
            if (u_stack != NULL && k_tsk_create_new(p_taskinfo, p_tcb, tid) == RTX_OK) {
            	g_num_active_tasks++;
            	pl_insert(prio_list, p_tcb, &tree, priorities + p_taskinfo->prio);
            }
        } else {
			if (k_tsk_create_new(p_taskinfo, p_tcb, tid) == RTX_OK) {
				g_num_active_tasks++;
				pl_insert(prio_list, p_tcb, &tree, priorities + p_taskinfo->prio);
			}
        }
        p_taskinfo++;
    }
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb ->tid = tid;
    p_tcb->state = READY;

    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) = (U32) p_taskinfo->u_stack_hi;
        p_tcb->usp = (U32*)p_taskinfo->u_stack_hi;
//        p_tcb->usp = sp;

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        if(p_tcb_old->state != DORMANT && p_tcb_old->state != BLK_MSG){
            p_tcb_old->state = READY;	// change state of the to-be-switched-out tcb
        }
        k_tsk_switch(p_tcb_old);            // switch stacks
    }

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
    return run_new_wrapper(TRUE);
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */
    RTX_TASK_INFO info;
    // *********DONE***********
    // validate priority, and stack size? <- DONE
    // create task's user stack and add it to the taks_info buffer
    // if the returned address is NULL don't run k_tsk_create_new
    //*************************
    // TODO: If stack_size not 8 byte aligned return RTX_ERR - done, need to confirm if correct
    if(g_num_active_tasks >= MAX_TASKS || prio == PRIO_NULL || stack_size < 0x200
    		|| (stack_size & ~0x07) != stack_size || task == NULL || task_entry == NULL || prio == PRIO_RT){
    	return RTX_ERR;
    }
    U32* u_stack = k_alloc_p_stack(stack_size);

    if(u_stack == NULL){
    	return RTX_ERR;
    }

    task_t tid = (current_tid + 1)%MAX_TASKS;
    U16 count = 0;

    while((g_tcbs[tid].state != DORMANT && count < MAX_TASKS) || tid == TID_UART_IRQ){
    	tid = (tid + 1)%MAX_TASKS;

    	++count;
    }
    current_tid = tid;

    info.ptask = task_entry;
    info.k_stack_size = K_STACK_SIZE;
    info.u_stack_size = stack_size;
    info.tid = tid;
    info.prio = prio;
    info.state = READY;
    info.priv = 0;
    info.u_stack_hi = (U32)u_stack;

    *task = tid;

    g_tcbs[tid].prio = prio;
    g_tcbs[tid].priv = 0;
    g_tcbs[tid].u_stack_size = stack_size;
    g_tcbs[tid].ptask = task_entry;

    if(k_tsk_create_new(&info, &(g_tcbs[tid]), tid) == RTX_ERR){
    	return RTX_ERR;
    }
    g_num_active_tasks++;
    pl_insert(prio_list, &(g_tcbs[tid]), &tree, priorities + prio);

    run_new_wrapper(FALSE);

    /* DONE */
    //add tcb to ready list
    //call k_task_run_new
    return RTX_OK;

}

void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */

  /* DONE */
  //deallocate user stack
  //set state to dormant
  //run new task and make sure that the state of the exiting task is still dormant.

    if(gp_current_task->priv == 0){
    	k_dealloc_p_stack(gp_current_task->usp, gp_current_task->u_stack_size);
    }

    // deallocate mailbox if it exists
    if(gp_current_task->mbx != NULL) {
    	k_mem_dealloc_with_tid(gp_current_task->mbx, 256);
    }

    g_num_active_tasks--;
    gp_current_task->state = DORMANT;
    run_new_wrapper(FALSE);

    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    if(g_tcbs[task_id].state == DORMANT || prio == PRIO_NULL || prio == PRIO_RT ||
    		task_id == 0 || (g_tcbs[task_id].priv == 1 && gp_current_task->priv == 0) || task_id >= MAX_TASKS){
    	return RTX_ERR;
    }
    if(task_id == gp_current_task->tid){
    	gp_current_task->prio = prio;
    	run_new_wrapper(TRUE);
    } else {
    	if(g_tcbs[task_id].state == READY){
    		pl_remove(prio_list, g_tcbs + task_id, &tree, priorities + g_tcbs[task_id].prio);
    	}
		g_tcbs[task_id].prio = prio;
    	if(g_tcbs[task_id].state == READY){
    		pl_insert(prio_list, g_tcbs + task_id, &tree, priorities + g_tcbs[task_id].prio);
    	}

		run_new_wrapper(FALSE);
    }


    return RTX_OK;    
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get_info: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL || g_tcbs[task_id].state == DORMANT || task_id >= MAX_TASKS) {
        return RTX_ERR;
    }
    /* The code fills the buffer with some fake task information. 
       You should fill the buffer with correct information    */
    buffer->tid = task_id;
    buffer->prio = g_tcbs[task_id].prio;
    buffer->state = g_tcbs[task_id].state;
    buffer->priv = g_tcbs[task_id].priv;
    buffer->ptask = g_tcbs[task_id].ptask;
    buffer->k_stack_size = K_STACK_SIZE;
    buffer->u_stack_size = g_tcbs[task_id].priv == 0 ? g_tcbs[task_id].u_stack_size : 0;

    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    printf("k_tsk_get_tid: entering...\n\r");
#endif /* DEBUG_0 */ 

    return gp_current_task->tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

int run_new_wrapper(BOOL strict){
	if(!strict && gp_current_task->state != DORMANT && gp_current_task->state != BLK_MSG){
		rb_tree_node* temp_node = rb_min(tree.root, &tree);
		if (temp_node == tree.nil) {
			return RTX_OK;
		}
		U8 min_prio = temp_node->priority;
		if(gp_current_task->prio <= min_prio){
			return RTX_OK;
		}
	}
	return k_tsk_run_new();
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}


/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
