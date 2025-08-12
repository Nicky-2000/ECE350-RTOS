#include "mailbox.h"
#include "k_inc.h"
#include "k_mem.h"

extern TCB *gp_current_task;
extern TCB g_tcbs[MAX_TASKS];

int init_mailbox(void* mbx_ptr, U32 size) {
	// set the mailbox fields for the currently run task
	U8 *tmp_mbx = (U8 *)mbx_ptr;

	for(int i = 0; i < size; ++i){
		tmp_mbx[i] = 0;
	}

	mailbox *mbx_header = (mailbox *)mbx_ptr;
	mbx_header->size = size;
	mbx_header->first_header = NULL;

	return RTX_OK;
}

int push_message(task_t receiver_tid, const void* buf) {
	TCB receiver = g_tcbs[receiver_tid];
	// check to see if receiver has mailbox?
	if (receiver.mbx == NULL)
		return RTX_ERR;

	mailbox *recv_mbx = (mailbox *)receiver.mbx;

	// case 1: there is nothing in the list
	if (recv_mbx->first_header == NULL) {
		// check that the mailbox is big enough for the message
		// we store 8 bytes in the header of the mailbox and we need to store the sending tid (not taken into account by the length in the buffer
		if ((recv_mbx->size - sizeof(mailbox)) < ((RTX_MSG_HDR*)buf)->length + sizeof(task_t))
			return RTX_ERR;

		recv_mbx->first_header = (RTX_MSG_HDR *)((U8 *)receiver.mbx + sizeof(mailbox));


		//		recv_mbx->first_header->length = ((RTX_MSG_HDR*)buf)->length;
		//		recv_mbx->first_header->type = ((RTX_MSG_HDR*)buf)->type;
		copy_data((U8 *)(recv_mbx->first_header), (U8*)buf, sizeof(RTX_MSG_HDR));

//		task_t *tmp = (U8 *)recv_mbx->first_header + sizeof(RTX_MSG_HDR);
//		*(tmp) = gp_current_task->tid;
		copy_data((U8 *)recv_mbx->first_header + sizeof(RTX_MSG_HDR), (U8*)&(gp_current_task->tid), sizeof(task_t));

		U8* dest = (U8*)(recv_mbx->first_header) + sizeof(RTX_MSG_HDR) + sizeof(task_t);
		U8* src = (U8*)(buf) + sizeof(RTX_MSG_HDR);
		U32 size = ((RTX_MSG_HDR*)buf)->length - sizeof(RTX_MSG_HDR);

		copy_data(dest, src, size);
		return RTX_OK;
	}
	// case 2: stuff already in the the list
	// Calculate the size that we have used up..
	U32 count = 0;
	RTX_MSG_HDR* p_traverse = (RTX_MSG_HDR *)recv_mbx->first_header;
	RTX_MSG_HDR* header_buf = (RTX_MSG_HDR*)k_mem_alloc_with_tid(sizeof(RTX_MSG_HDR), 256);
	RTX_MSG_HDR* last_msg = NULL;
	while (p_traverse !=  NULL && count < (recv_mbx->size - sizeof(mailbox))) {
		// get the header of this node in proper for so we can use the ->length
		get_header((U8*)(p_traverse), (U8*)header_buf, receiver.mbx);
		if(header_buf->length == 0){
			p_traverse = NULL;
		} else {
			count += header_buf->length + sizeof(task_t);
			// since all data within mailbox that isn't in use will be 0, the pointer returned from get_next
			last_msg = p_traverse;
			p_traverse = get_next_message(p_traverse, header_buf, receiver.mbx);
		}
	}

	U32 space_remaining = recv_mbx->size - count - sizeof(mailbox);
	if (space_remaining < (((RTX_MSG_HDR*)buf)->length + sizeof(task_t))){
		k_mem_dealloc_with_tid(header_buf, 256);
		return RTX_ERR;
	}

	get_header((U8*)(last_msg), (U8*)header_buf, receiver.mbx);
	void *start_of_msg = (void *)((U8 *)last_msg + header_buf->length + sizeof(task_t));
	void *end_of_msg = (void *)((U8 *)start_of_msg + ((RTX_MSG_HDR*)buf)->length + sizeof(task_t));
	U32 end_addr = (U32)recv_mbx + recv_mbx->size;

	if((U32)start_of_msg > end_addr){
		start_of_msg = (void *)((U8*)recv_mbx + sizeof(mailbox) + ((U32)start_of_msg - end_addr));
		end_of_msg = (void *)((U8 *)start_of_msg + ((RTX_MSG_HDR*)buf)->length + sizeof(task_t));
	}

	k_mem_dealloc_with_tid(header_buf, 256);

	// copy the actual message data to a temporary buffer
	void *tmp_msg = k_mem_alloc_with_tid(((RTX_MSG_HDR*)buf)->length + sizeof(task_t), 256);
	copy_data((U8 *)tmp_msg, (U8 *)buf, sizeof(RTX_MSG_HDR)); // copy header in
	task_t *tmp_tid = (U8 *)tmp_msg + sizeof(RTX_MSG_HDR);
	*tmp_tid = gp_current_task->tid; // add tid of the sender
	copy_data((U8 *)tmp_tid + sizeof(task_t), (U8 *)buf + sizeof(RTX_MSG_HDR), ((RTX_MSG_HDR*)buf)->length - sizeof(RTX_MSG_HDR));

	// This is the case where the whole thing should fit in without needing to wrap around
	if((U32)end_of_msg < end_addr){
		copy_data((U8 *)start_of_msg, (U8 *)tmp_msg, ((RTX_MSG_HDR*)buf)->length + sizeof(task_t)); // copy header in
	} else {
		// we need the amount to copy at the start
		U32 amount_left = (U32)end_of_msg - end_addr;
		U32 amount_before_wrap = ((RTX_MSG_HDR*)buf)->length + sizeof(task_t) - amount_left;
		copy_data((U8 *)start_of_msg, (U8 *)tmp_msg, amount_before_wrap);
		copy_data((U8 *)recv_mbx + sizeof(mailbox), (U8*)tmp_msg + amount_before_wrap, amount_left);
	}
	k_mem_dealloc_with_tid(tmp_msg, 256);
	// This is maybe done.
	return RTX_OK;
}

void copy_data(U8* dest, U8* src, U32 size) {
	for (int i = 0; i < size; ++i) {
		dest[i] = src[i];
	}
}

// This function will *in theory* always be able to extract a header from the mailbox (even if it is wrapped around).

void get_header(U8* src, U8* dest, mailbox *mbx) {
	U32 end_addr = (U32)mbx + mbx->size;
	U32 i_but_for_the_top = 0;
	for (int i = 0; i < 8; ++i) {
		// in this case we have not reached the end of the mailbox address space
		if ((U32)(src + i) < end_addr) {
			dest[i] = src[i];
		}
		// if the header had to be wrapped around. then some of the next bytes are at the top of the mailbox address space
		else {
			dest[i] = *((U8*)mbx + i_but_for_the_top + sizeof(mailbox));
			i_but_for_the_top += 1;
		}
	}
}

void *pop_message() {

	mailbox *mbx = (mailbox *)gp_current_task->mbx;
	if (mbx->first_header == NULL) {
		return NULL; // NO MAILBOX EXISTS
	}

	RTX_MSG_HDR header_to_pop;
	get_header( (U8*)(mbx->first_header), (U8*)&header_to_pop, mbx);

	U32 buffer_size = header_to_pop.length + sizeof(task_t);

	void *buffer = k_mem_alloc_with_tid(buffer_size, 256);

	void *start_of_msg = (void *)((U8 *)mbx->first_header);
	void *end_of_msg = (void *)((U8 *)start_of_msg + header_to_pop.length + sizeof(task_t));
	U32 end_addr = (U32)mbx + mbx->size;

	// This is the case where the whole thing should fit in without needing to wrap around
	if((U32)end_of_msg <= end_addr){
		copy_data((U8 *)buffer, (U8 *)start_of_msg, header_to_pop.length + sizeof(task_t)); // copy header in
		for(int i = 0; i < header_to_pop.length + sizeof(task_t); ++i){
			((U8*)start_of_msg)[i] = 0;
		}
		// exact fit
		if ((U32)end_of_msg == end_addr) { // NOT SURE IF THIS IS RIGHT!!!
			RTX_MSG_HDR expected_header;
			get_header((U8 *)mbx + sizeof(mailbox) , (U8 *)&expected_header, mbx);
			if (expected_header.length == 0) {
				mbx->first_header = NULL;
			}
			else {
				mbx->first_header = (RTX_MSG_HDR*)((U8 *)mbx + sizeof(mailbox));
			}
		} else {
			RTX_MSG_HDR expected_header;
			get_header((U8 *)end_of_msg , (U8 *)&expected_header, mbx);
			if(expected_header.length == 0) {
				mbx->first_header = NULL;
			} else {
				mbx->first_header = (RTX_MSG_HDR*)end_of_msg;
			}
		}
		// need statement for not exact fit

	} else {
		// we need the amount to copy at the start
		U32 amount_left = (U32)end_of_msg - end_addr;
		U32 amount_before_wrap = header_to_pop.length + sizeof(task_t) - amount_left;
		copy_data((U8 *)buffer, (U8 *)start_of_msg, amount_before_wrap);
		copy_data((U8*)buffer + amount_before_wrap, (U8 *)mbx + sizeof(mailbox), amount_left);
		for(int i = 0; i < amount_before_wrap; ++i){
			((U8*)start_of_msg)[i] = 0;
		}
		for(int i = 0; i < amount_left; ++i){
			((U8 *)mbx + sizeof(mailbox))[i] = 0;
		}

		RTX_MSG_HDR expected_header;
		get_header((U8 *)mbx + sizeof(mailbox) + amount_left , (U8 *)&expected_header, mbx);
		if(expected_header.length == 0) {
			mbx->first_header = NULL;
		} else {
			mbx->first_header = (void *)((U8 *)mbx + sizeof(mailbox) + amount_left);
		}
	}

	return buffer;
}

RTX_MSG_HDR *get_next_message(RTX_MSG_HDR *current_msg, RTX_MSG_HDR *current_header, mailbox *mbx){
	RTX_MSG_HDR *next_msg_nowrap = (RTX_MSG_HDR *)((U8 *)current_msg + current_header->length + sizeof(task_t));
	U32 end_addr = (U32)mbx + mbx->size;
	if((U32)next_msg_nowrap < end_addr){
		return next_msg_nowrap;
	}

	U32 amount_left = (U32)next_msg_nowrap - end_addr;

	RTX_MSG_HDR *real_next_msg = (RTX_MSG_HDR *)((U8 *)mbx + amount_left + sizeof(mailbox));

	return real_next_msg;
}

BOOL empty(mailbox * mbx) {
	if(mbx->first_header == NULL){
		return TRUE;
	}
	return FALSE;
}

