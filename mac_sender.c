#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"
osMessageQueueId_t queue_buffer_id;

const osMessageQueueAttr_t queue_buffer_attr = {
	.name = "BUFFER_SEND "  	
};

typedef struct newToken
{
	uint8_t startByte;
	uint8_t station[15];
} myToken;
typedef struct newDataFrame
{
	uint8_t	stx;
	uint16_t control;
	uint8_t length;
	char* data;
	uint8_t status;
	uint8_t etx;
} myDataFrame;

void MacSender(void *argument)
{
	queue_buffer_id = osMessageQueueNew(2,sizeof(struct queueMsg_t),&queue_buffer_attr); 	
	osStatus_t retCode;
	bool	alreadySent = false;
	struct queueMsg_t queueMsg;	
	//struct newDataFrame myDataFrame;
	myDataFrame* data_ptr = 0;
	myToken* token_ptr = 0;
	

	for (;;){
		retCode = osMessageQueueGet( 	
			queue_macS_id,
			&queueMsg,
			NULL,
			osWaitForever); 
		
		//Test if the message is succesfully read
		if(retCode == osOK){
			switch(queueMsg.type){
				//New token must be created
				case NEW_TOKEN:
					token_ptr = osMemoryPoolAlloc(memPool,osWaitForever);
					for (int i = 1; i <= 15; i++){
						if(i == MYADDRESS){
							token_ptr->station[i] = (1 << TIME_SAPI) | (1 << CHAT_SAPI);
						}else{
							token_ptr->station[i] = 0;
						}
					}
					token_ptr->startByte = 0xFF;
					queueMsg.anyPtr = token_ptr;
					queueMsg.type = TO_PHY;
					
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					
					break;
					
				case START:
					gTokenInterface.connected = true;
					
					break;
				case STOP:
					gTokenInterface.connected = false;
				
					break;
				case DATA_IND:
					alreadySent = false;
					data_ptr->stx = STX;
					data_ptr->control = ((((MYADDRESS << 3) + queueMsg.sapi) << 8) + (queueMsg.addr << 3) + queueMsg.sapi) & 0x7F7F;
					data_ptr->data = queueMsg.anyPtr;
					data_ptr->length = strlen(data_ptr->data);
					if(queueMsg.addr == 15){
						//broadcast message
						data_ptr->status = ((data_ptr->control + (int)data_ptr->data + data_ptr->length) << 2) + 3;
					}else{
						//message to single station
						data_ptr->status = (data_ptr->control + (int)data_ptr->data + data_ptr->length) << 2;
					}
					data_ptr->etx = ETX;
					queueMsg.anyPtr = data_ptr;
					queueMsg.type = DATA_IND;
					
					retCode = osMessageQueuePut(
						queue_buffer_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					
					
					break;
				case TOKEN:
					token_ptr = queueMsg.anyPtr;
					if(alreadySent == false){
						
						do{
							retCode = osMessageQueueGet( 	
								queue_buffer_id,
								&queueMsg,
								NULL,
								osWaitForever); 						
						}while(retCode == osOK);	
						
						/*queueMsg.anyPtr = data_ptr;
						queueMsg.type = TO_PHY;
						retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);*/
						alreadySent = true;
						queueMsg.anyPtr = token_ptr;
						queueMsg.type = TO_PHY;
						retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);
					}
								
					break;
				case DATABACK:
					data_ptr = queueMsg.anyPtr;

					if(data_ptr->status&0x02 == 1){
						if(data_ptr->status&0x01 == 1){
							queueMsg.anyPtr = token_ptr;
							queueMsg.type = TO_PHY;
							retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);
						} else {
							data_ptr->status = data_ptr->status-2;
							queueMsg.anyPtr = data_ptr;
							queueMsg.type = TO_PHY;
							retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);
						}
						alreadySent = false;
					} else {
						queueMsg.type = MAC_ERROR;
						retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					}
					break;
				default:
					break;
			}	
		}
		
	}
}
