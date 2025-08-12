/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "mailbox.h"
#include "tcb_storage.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

extern TCB * gp_current_task;
extern TCB g_tcbs[MAX_TASKS];

extern rb_tree_node 	priorities[PRIO_NULL + 2];
extern rb_tree			tree;
extern TCB 			*prio_list[PRIO_NULL + 1];


int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */
    // error check
    if (gp_current_task->mbx != NULL || size < MIN_MBX_SIZE)
    	return RTX_ERR;

    gp_current_task->mbx = k_mem_alloc_with_tid(size, 256);

    // could not allocate memory for mailbox
    if (gp_current_task->mbx == NULL)
    	return RTX_ERR;

    init_mailbox(gp_current_task->mbx, size);

    return RTX_OK;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
    // error checking
    if (receiver_tid >= MAX_TASKS || g_tcbs[receiver_tid].state == DORMANT || buf == NULL || ((RTX_MSG_HDR *)buf)->length < MIN_MSG_SIZE ) {
    	return RTX_ERR;
    }

    int ret = push_message(receiver_tid, buf);

    if (ret == RTX_ERR)
    	return RTX_ERR;

    if (g_tcbs[receiver_tid].state == BLK_MSG) {
    	g_tcbs[receiver_tid].state = READY;
    	pl_insert(prio_list, g_tcbs + receiver_tid, &tree, priorities + g_tcbs[receiver_tid].prio);
    }

    if(gp_current_task->tid != TID_UART_IRQ)
    	run_new_wrapper(TRUE);

    return RTX_OK;
}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */

    //////
    ///// ADD CHECKS FOR BLK_MSG IN THE FUNCTIONS IN K_TASK!!!!
    ///// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    /////
    /////
    if(buf == NULL || gp_current_task->mbx == NULL)
    	return RTX_ERR;

    if(empty((mailbox *)(gp_current_task->mbx))){
    	gp_current_task->state = BLK_MSG;
    	run_new_wrapper(FALSE);
    }

    void *msg = pop_message();

    if(((RTX_MSG_HDR *)msg)->length > len){
    	k_mem_dealloc_with_tid(msg, 256);
    	return RTX_ERR;
    }

    copy_data((U8 *)buf, (U8*)msg, sizeof(RTX_MSG_HDR));
    copy_data((U8 *)buf + sizeof(RTX_MSG_HDR), (U8*)msg + sizeof(RTX_MSG_HDR) + sizeof(task_t), ((RTX_MSG_HDR *)msg)->length - sizeof(RTX_MSG_HDR));

    if(sender_tid != NULL){
    	*sender_tid = *((task_t *)((U8*)msg + sizeof(RTX_MSG_HDR)));
    }

    k_mem_dealloc_with_tid(msg, 256);

    return RTX_OK;
}

int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    return 0;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
