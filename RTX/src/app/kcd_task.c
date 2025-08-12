#include "rtx.h"
#include "k_inc.h"
#include "Serial.h"
#include "printf.h"

/* The KCD Task Template File */

extern TCB g_tcbs[MAX_TASKS];

void invalid(){
	SER_PutStr(1,"Invalid Command");
}

void cant_process(){
	SER_PutStr(1,"Command cannot be processed");
}

void kcd_task(void)
{
	//buffer including overhead
	RTX_MSG_HDR *msg = mem_alloc(sizeof(RTX_MSG_HDR) + 64);
	size_t msg_hdr_len = sizeof(RTX_MSG_HDR);
	task_t commands[62]; //A-Z + a-z + 0-9 registered commands

	for(U8 i = 0; i < 62; ++i){
		commands[i] = MAX_TASKS;
	}

	// this is going to be used to to store the putty input
	U8 command_string[64];
	U8 count = 0;

	task_t sender_tid = 0;

	mbx_create(KCD_MBX_SIZE);

	while (1){
		if (recv_msg(&sender_tid, msg, msg_hdr_len + 1) == RTX_OK){
			if (msg->type == KCD_REG){
				//command registration
				printf("Herer\r\n");
				//more than one char after %
				if (msg->length - msg_hdr_len > 1){
					continue;
				}

				//first char after header
				//(is the % included in the msg? - would increment lines 17, 22 and 27)
				U8 cmd = (U8)(*((U8*)msg + msg_hdr_len));

				if (cmd >= 48 && cmd <= 57){
					commands[cmd - 48] = sender_tid; //0-9
				} else if (cmd >= 65 && cmd <= 90){
					commands[cmd - 55] = sender_tid; //A-Z
				} else if (cmd >= 97 && cmd <= 122){
					commands[cmd - 61] = sender_tid; //a-z
				} else {
					printf("Should hit here once (cmd=%d)\r\n", cmd);
					continue;
				}

//				send_msg(TID_KCD, (void *)msg);
			} else if (msg->type == KEY_IN) {
				//TODO: terminal keyboard input (putty input keys)
				if(sender_tid != TID_UART_IRQ){
					continue;
				}
				U8 cmd = (U8)(*((U8*)msg + msg_hdr_len));
				task_t send_to;
				//if it's enter
				if(cmd == '\r'){
					// do something important
					if(command_string[0] != '%' || count > 64){
						count = 0;
						invalid();
						continue;
					}

					if (command_string[1] >= 48 && command_string[1] <= 57){
						send_to = commands[command_string[1] - 48]; //0-9
					} else if (command_string[1] >= 65 && command_string[1] <= 90){
						send_to = commands[command_string[1] - 55]; //A-Z
					} else if (command_string[1] >= 97 && command_string[1] <= 122){
						send_to = commands[command_string[1] - 61]; //a-z
					} else {
						cant_process();
						count = 0;
						continue;
					}

					if(send_to >= MAX_TASKS ||  g_tcbs[send_to].state == DORMANT){
						cant_process();
						count = 0;
						continue;
					}

					((RTX_MSG_HDR *)msg)->length = count - 1 + msg_hdr_len;
					((RTX_MSG_HDR *)msg)->type = KCD_CMD;

					copy_data((U8*)msg + msg_hdr_len, command_string + 1, count - 1);
					if(send_msg(send_to, msg) == RTX_ERR){
						cant_process();
					}
					count = 0;
					continue;
				}

				if(count < 64){
					command_string[count] = cmd;
					++count;
				} else {
					count = 65;
				}
				// count the number of times you have put stuff into the command array
				// increment it everytime we get a key_in. Upon hearing enter we reset counter to 0
				// we throw out input if the

			}
			//break case?
		}
	}
}
