#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"



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
	uint8_t * data_ptr;
	uint8_t * msg;
	size_t size;
	uint8_t length = 0;
	osStatus_t retCode;
	struct queueMsg_t queueMsgR;
	
	for(;;){
		//Get the message on the queue
		retCode = osMessageQueueGet(
			queue_macR_id,
			&queueMsgR,
			NULL,
			osWaitForever);
		
		//Test if the message is succesfully read
		if(retCode == osOK)
		{
			msg = queueMsgR.anyPtr;
			if(msg[0] == TOKEN_TAG)		
			{
				size = TOKENSIZE;						//size of token frame
			} 
			else
			{
				size = msg[2] + 4;			//size of message frame + 4 for Control, Status...)
			}

			if(msg[0] == TOKEN_TAG)			//token frame received
			{		
				//--------------------------------------------------------------------------
				// TOKEN RECEIVED AND SEND TO MAC SENDER			
				//--------------------------------------------------------------------------			
				queueMsgR.type = TOKEN;				//send token to MAC Sender
				retCode = osMessageQueuePut(
					queue_macS_id,
					&queueMsgR,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
			} 
			else if(msg[1]>>3 == gTokenInterface.myAddress ||		//destination is my address 
					msg[1]>>3 == BROADCAST_ADDRESS)					//or broadcast 
			{		
				if(controlCS(msg))		//control checksum
				{
					//----------------------------------------------------------------------------
					// MEMORY ALLOCATION				
					//----------------------------------------------------------------------------
					data_ptr = osMemoryPoolAlloc(memPool,osWaitForever);
					memcpy(data_ptr,&msg[3],msg[2]);
					queueMsgR.anyPtr = data_ptr;

					queueMsgR.type = DATA_IND;				
					if(msg[1]&0x03 == CHAT_SAPI)				//chat received
					{
						retCode = osMessageQueuePut(
							queue_chatR_id,
							&queueMsgR,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					} 
					else if(msg[1]&0x03 == TIME_SAPI)		//time received
					{
						retCode = osMessageQueuePut(
							queue_timeR_id,
							&queueMsgR,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}
				} 
				else			//checksum is incorrect
				{
					length = msg[2];
					msg[length+3] = (msg[length+3] & 0xFC) | (0x2 & 0x3);		//set READ and clear ACK
					queueMsgR.type = TO_PHY;
					retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsgR,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				}
			}
			else if(msg[0]>>3 == gTokenInterface.myAddress)	//source is my address
			{
				queueMsgR.type = DATABACK;			//send databack to MAC Sender
				retCode = osMessageQueuePut(
					queue_macS_id,
					&queueMsgR,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
			}
			else			//message not ment for us, send back to physical layer
			{
				queueMsgR.type = TO_PHY;
				retCode = osMessageQueuePut(
					queue_phyS_id,
					&queueMsgR,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
			}
			}		//Message a nous meme: limité a 2 ou 3
	}
}
