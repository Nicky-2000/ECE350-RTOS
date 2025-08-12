


#ifndef K_MBX_H_
#define K_MBX_H_
#include "common.h"


// the mailbox is going to be allocated data that will hold a circular queue of messages


// create function needs to have

// init mailbox function

// functions we need for this mailbox

/*------------------------
 *
 *
 *
 *
 *
 *
 *
 */

//typedef struct msg_node {
//	RTX_MSG_HDR* msg;
//	struct msg_node* next;
//
//
//} msg_node;
//
//
typedef struct mailbox {
	U32 size;
	RTX_MSG_HDR* first_header;


} mailbox;


int init_mailbox(void* mbx_ptr, U32 size);
// set all the values... using current running task set the tcb of current task to


int push_message(task_t receiver_tid, const void* buf);


void *pop_message(void);

void copy_data(U8* dest, U8* src, U32 size);
void get_header(U8* src, U8* dest, mailbox *mbx);
RTX_MSG_HDR *get_next_message(RTX_MSG_HDR *current_msg, RTX_MSG_HDR *current_header, mailbox *mbx);
BOOL empty(mailbox *mbx);
#endif /* ! K_MBX_H_ */
