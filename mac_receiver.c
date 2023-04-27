#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

osStatus_t retCodeR;
struct queueMsg_t queueMsgR;

//////////////////////////////////////////////////////////////////////////////////
/// \brief Send message on the message queue
/// \param theId The id corresponding to the class we want to send a message
//////////////////////////////////////////////////////////////////////////////////
void SendMessageR(osMessageQueueId_t theId){
	retCodeR = osMessageQueuePut(
		theId,
		&queueMsgR,
		osPriorityNormal,
		osWaitForever);
	CheckRetCode(retCodeR,__LINE__,__FILE__,CONTINUE);
}
//////////////////////////////////////////////////////////////////////////////////
/// \brief Control if the checksum received is correct
/// \param data Pointer to the data frame received
//////////////////////////////////////////////////////////////////////////////////
bool controlCS(uint8_t* data){
	uint8_t calculateCS = 0; 
	uint8_t length = 0;
	length = data[3];
	
	calculateCS = data[1] + data[2] + data[3];
	for(int i = 0; i < length; i++)
	{
		calculateCS += data[4+i];
	}
	calculateCS = calculateCS&0x3F;
	if(calculateCS == (data[length+4]>>2))
	{
		return true;
	} 
	else
	{
		return false;
	}
}
void MacReceiver(void *argument)
{
	uint8_t * msg_ptr;
	uint8_t * msg;
	size_t size;
	uint8_t length = 0;
	
	for(;;){
		//Get the message on the queue
		retCodeR = osMessageQueueGet(
			queue_macR_id,
			&queueMsgR,
			NULL,
			osWaitForever);
		
		//Test if the message is succesfully read
		if(retCodeR == osOK)
		{
			msg_ptr = queueMsgR.anyPtr;
			if(msg_ptr[0] == TOKEN_TAG)		
			{
				size = TOKENSIZE;						//size of token frame
			} 
			else
			{
				size = msg_ptr[3] + 6;			//size of message frame + 6 for ETX,Control,...)
			}
			//----------------------------------------------------------------------------
			// MEMORY ALLOCATION				
			//----------------------------------------------------------------------------
			/*msg = osMemoryPoolAlloc(memPool,osWaitForever);
			memcpy(msg,&msg_ptr[0],size);
			queueMsgR.anyPtr = msg;*/
						
			if(msg_ptr[0] == TOKEN_TAG)			//token frame received
			{		
				//--------------------------------------------------------------------------
				// TOKEN RECEIVED AND SEND TO MAC SENDER			
				//--------------------------------------------------------------------------			
				queueMsgR.type = TOKEN;				//send token to MAC Sender
				SendMessageR(queue_macS_id);
			} 
			else if(msg_ptr[0]>>3 == gTokenInterface.myAddress)				//source is my address
			{
				if(msg_ptr[1]>>3 == gTokenInterface.myAddress)
				{
					
				}
				queueMsgR.type = DATABACK;			//send databack to MAC Sender
				SendMessageR(queue_macS_id);
			} 
			else
			{
				if(msg_ptr[1]>>3 == gTokenInterface.myAddress ||		//destination is my address 
					msg_ptr[1]>>3 == BROADCAST_ADDRESS)					//or broadcast 
				{		
					if(controlCS(msg))		//control checksum
					{
						//checksum is correct, we can read the message
						length = msg_ptr[2];
						msg_ptr[length+3] = (msg_ptr[length+3] & 0xFC) | (0x3 & 0x3);		//set READ and ACK bits
						queueMsgR.type = DATA_IND;				
						if(msg_ptr[1]&0x03 == CHAT_SAPI)				//chat received
						{
							SendMessageR(queue_chatR_id);
						} 
						else if(msg_ptr[1]&0x03 == TIME_SAPI)		//time received
						{
							SendMessageR(queue_timeR_id);
						}
					} 
					else			//checksum is incorrect
					{
						length = msg_ptr[2];
						msg_ptr[length+3] = (msg_ptr[length+3] & 0xFC) | (0x2 & 0x3);		//set READ and clear ACK
						queueMsgR.type = TO_PHY;
						SendMessageR(queue_phyS_id);
					}
					
				} 
				else 				//message not ment for us, send back to physical layer
				{
					queueMsgR.type = TO_PHY;
					SendMessageR(queue_phyS_id);
				}
			}
		}
	}
}
