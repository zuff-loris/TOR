#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

typedef struct newToken
{
	uint8_t startByte;
	uint8_t station[15];
} myToken;

osMessageQueueId_t queue_buffer_id;


const osMessageQueueAttr_t queue_buffer_attr = {
	.name = "BUFFER_SEND "  	
};

void MacSender(void *argument)
{
	queue_buffer_id = osMessageQueueNew(8,sizeof(struct queueMsg_t),&queue_buffer_attr); 	
	uint8_t length = 0;
	uint8_t * msg_ptr = 0;
	uint8_t crcCalculate = 0;
	struct queueMsg_t queueMsgS;	
	osStatus_t retCode;
	myToken* token_ptr = 0;
	token_ptr = osMemoryPoolAlloc(memPool,osWaitForever);

	for (;;){
		//Get the message on the queue
		retCode = osMessageQueueGet( 	
			queue_macS_id,
			&queueMsgS,
			NULL,
			osWaitForever); 
		
		//Test if the message is succesfully read
		if(retCode == osOK){
			switch(queueMsgS.type){
				//New token must be created
				case NEW_TOKEN:
					token_ptr->startByte = 0xFF;
					for (int i = 0; i <= 15; i++){
						if(i == MYADDRESS){
							token_ptr->station[i] = (1 << TIME_SAPI) | (1 << CHAT_SAPI);
						}else{
							token_ptr->station[i] = 0;
						}
					}
					queueMsgS.anyPtr = token_ptr;
					queueMsgS.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsgS,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);					
					break;
					
				case START:
					gTokenInterface.connected = true;
					
					break;
				case STOP:
					gTokenInterface.connected = false;
				
					break;
				case DATA_IND:
					msg_ptr = osMemoryPoolAlloc(memPool,osWaitForever);
				
					msg_ptr[0] = ((MYADDRESS << 3) + queueMsgS.sapi) & 0x7F;
					msg_ptr[1] = ((queueMsgS.addr << 3) + queueMsgS.sapi) & 0x7F;
					msg_ptr[2] = strlen(queueMsgS.anyPtr);
					memcpy(&msg_ptr[3],queueMsgS.anyPtr,strlen(queueMsgS.anyPtr));
					crcCalculate = msg_ptr[0] + msg_ptr[1] + msg_ptr[2];
				
					for(int i = 0; i < msg_ptr[2]; i++)
					{
						crcCalculate += msg_ptr[3+i];
					}
					
					msg_ptr[3+msg_ptr[2]] = (crcCalculate <<2);
					
					if(queueMsgS.addr == 15)
					{
						//broadcast message (READ = 1, ACK = 1)	
						msg_ptr[3+msg_ptr[2]] += 3;
					}

					retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				  queueMsgS.anyPtr = msg_ptr;
					queueMsgS.type = DATA_IND;
				
					retCode = osMessageQueuePut(
						queue_buffer_id,
						&queueMsgS,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
					break;
				case TOKEN:
					token_ptr = queueMsgS.anyPtr;
				
					for(int i = 0; i <= 15; i++)
					{
						if(i == MYADDRESS && gTokenInterface.connected == true)
						{
							token_ptr->station[i] = (1 << TIME_SAPI) | (1 << CHAT_SAPI);
						} 
						else if(i == MYADDRESS)
						{
							token_ptr->station[i] = (1 << TIME_SAPI);
						}
						if(token_ptr->station[i] != gTokenInterface.station_list[i])
						{
							gTokenInterface.station_list[i] = token_ptr->station[i];
							queueMsgS.type = TOKEN_LIST;
							retCode = osMessageQueuePut(
								queue_lcd_id,
								&queueMsgS,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							i = 16;
						}
					}
					
					retCode = osMessageQueueGet( 	
						queue_buffer_id,
						&queueMsgS,
						NULL,
						0); 		
					//CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					if(retCode == osOK)
					{
						queueMsgS.type = TO_PHY;
						retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsgS,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
					} 
					
					queueMsgS.anyPtr = token_ptr;
					queueMsgS.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsgS,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
										
					break;
				case DATABACK:
					msg_ptr = queueMsgS.anyPtr;
					length = msg_ptr[2];

					if(((msg_ptr[3+length] & 0x02)>>1) == 1)			//is READ bit set?
					{
						if(msg_ptr[3+length]&0x01 == 1)	//is ACK bit set?
						{	
							// Read = 1 & ACK = 1 => Send Token
							retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							queueMsgS.anyPtr = token_ptr;
							queueMsgS.type = TO_PHY;
							retCode = osMessageQueuePut(
								queue_phyS_id,
								&queueMsgS,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						} 
						else 	// Read = 1 & ACK = 0 => Send message back
						{
							retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							msg_ptr[3+msg_ptr[2]] -= 2;
							queueMsgS.anyPtr = msg_ptr;
							queueMsgS.type = TO_PHY;
							retCode = osMessageQueuePut(
								queue_phyS_id,
								&queueMsgS,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						}
					}
					else 		// Read = 0 => MAC ERROR
					{
						retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
						char* errorMsg = osMemoryPoolAlloc(memPool,osWaitForever);
						errorMsg = "OH NON CEST FAUX\0"; 
						queueMsgS.anyPtr = errorMsg;
						 
						queueMsgS.type = MAC_ERROR;
						retCode = osMessageQueuePut(
							queue_lcd_id,
							&queueMsgS,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}
					break;
				default:
					break;
			}	
		}
	}
}
