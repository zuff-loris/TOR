#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

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

osMessageQueueId_t queue_buffer_id;
osStatus_t retCodeS;
struct queueMsg_t queueMsgS;	
myToken* token_ptr = 0;

const osMessageQueueAttr_t queue_buffer_attr = {
	.name = "BUFFER_SEND "  	
};

//////////////////////////////////////////////////////////////////////////////////
/// \brief Send message on the message queue
/// \param theId The id corresponding to the class we want to send a message
//////////////////////////////////////////////////////////////////////////////////
void SendMessageS(osMessageQueueId_t theId)
{
	retCodeS = osMessageQueuePut(
		theId,
		&queueMsgS,
		osPriorityNormal,
		osWaitForever);
	CheckRetCode(retCodeS,__LINE__,__FILE__,CONTINUE);
}
bool updateStation()
{
	for(int i = 0; i <= 15; i++)
	{
		if(token_ptr->station[i] != gTokenInterface.station_list[i])
		{
			return true;
		}
	}
	return false;
}

void MacSender(void *argument)
{
	queue_buffer_id = osMessageQueueNew(8,sizeof(struct queueMsg_t),&queue_buffer_attr); 	
	//struct newDataFrame myDataFrame;
	uint8_t nbMsg = 0;
	myDataFrame* data_ptr = 0;
	bool newToken = false;

	for (;;){
		//Get the message on the queue
		retCodeS = osMessageQueueGet( 	
			queue_macS_id,
			&queueMsgS,
			NULL,
			osWaitForever); 
		
		//Test if the message is succesfully read
		if(retCodeS == osOK){
			switch(queueMsgS.type){
				//New token must be created
				case NEW_TOKEN:
					newToken = true;
					token_ptr = osMemoryPoolAlloc(memPool,osWaitForever);
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
					SendMessageS(queue_phyS_id);					
					break;
					
				case START:
					gTokenInterface.connected = true;
					
					break;
				case STOP:
					gTokenInterface.connected = false;
				
					break;
				case DATA_IND:
					data_ptr->stx = STX;
					data_ptr->control = ((((MYADDRESS << 3) + queueMsgS.sapi) << 8) + (queueMsgS.addr << 3) + queueMsgS.sapi) & 0x7F7F;
					data_ptr->data = queueMsgS.anyPtr;
					data_ptr->length = strlen(data_ptr->data);
					if(queueMsgS.addr == 15){
						//broadcast message
						data_ptr->status = ((data_ptr->control + (int)data_ptr->data + data_ptr->length) << 2) + 3;
						data_ptr->status = (data_ptr->status & 0xFC) | (0x3 & 0x3);
					}else{
						//message to single station
						data_ptr->status = (data_ptr->control + (int)data_ptr->data + data_ptr->length) << 2;
					}
					data_ptr->etx = ETX;
					queueMsgS.anyPtr = data_ptr;
					queueMsgS.type = DATA_IND;
					
					SendMessageS(queue_buffer_id);					
					break;
				case TOKEN:
					token_ptr = queueMsgS.anyPtr;
					if(updateStation())
					{
						SendMessageS(queue_lcd_id);
					}
					
					//Send all messages on the queue when we have the token
					do{
						retCodeS = osMessageQueueGet( 	
							queue_buffer_id,
							&queueMsgS,
							NULL,
							20); 		
					}while(retCodeS == osOK);	
					if(retCodeS == osOK)
					{
						queueMsgS.type = TO_PHY;
						SendMessageS(queue_phyS_id);	
						nbMsg++;
					}
					
					if(nbMsg == 0)				//if no message has been sent, we can release the token
					{
						queueMsgS.anyPtr = token_ptr;
						queueMsgS.type = TO_PHY;
						SendMessageS(queue_phyS_id);
					}
												
					break;
				case DATABACK:
					data_ptr = queueMsgS.anyPtr;

					if(data_ptr->status&0x02 == 1)			//is READ bit set?
					{
							if(data_ptr->status&0x01 == 1)	//is ACK bit set?
						{
							nbMsg--;
							if(nbMsg == 0)									//all my messages has been received? (via databack)
							{
								queueMsgS.anyPtr = token_ptr;
								queueMsgS.type = TO_PHY;
								SendMessageS(queue_phyS_id);
								newToken = false;
							}
							
						} 
						else 
						{
							data_ptr->status = data_ptr->status-2;
							queueMsgS.anyPtr = data_ptr;
							queueMsgS.type = TO_PHY;
							SendMessageS(queue_phyS_id);
						}

						} else 
						{
							queueMsgS.type = MAC_ERROR;
							SendMessageS(queue_lcd_id);
						}
					break;
				default:
					break;
			}	
		}
	}
}
