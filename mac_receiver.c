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
	length = data[2];
	
	calculateCS = data[0] + data[1] + data[2];
	for(int i = 0; i < length; i++)
	{
		calculateCS += data[3+i];
	}
	calculateCS = calculateCS << 2;
	if(calculateCS == (data[length+3]&0xFC))
	{
		return true;
	} 
	else
	{
		return false;
	}
}
//////////////////////////////////////////////////////////////////////////////////
/// \brief MacReceiver Thread: run the receiver algorithm
//////////////////////////////////////////////////////////////////////////////////
void MacReceiver(void *argument)
{
	uint8_t * data_ptr;											//Pointer to the data
	uint8_t * msg;													//Pointer to the data frame
	uint8_t length = 0;											//Length of the data
	osStatus_t retCode;											//Status of the message queue
	struct queueMsg_t queueMsgR;						//Structure containing frame information
	
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
			//--------------------------------------------------------------------------
			// DESTINATION ADDRESS IS MY ADDRESS OR BROADCAST		
			//--------------------------------------------------------------------------	
			else if(msg[1]>>3 == gTokenInterface.myAddress ||		
					msg[1]>>3 == BROADCAST_ADDRESS)					
			{		
				if(controlCS(msg))		//control checksum
				{
					//----------------------------------------------------------------------------
					// MEMORY ALLOCATION				
					//----------------------------------------------------------------------------
					data_ptr = osMemoryPoolAlloc(memPool,osWaitForever);
					memcpy(data_ptr,&msg[3],msg[2]);
					queueMsgR.addr = msg[0] >> 3;
					
					if(msg[0]>>3 != gTokenInterface.myAddress)	//Source is not my address?
					{
						msg[3+msg[2]] = msg[3+msg[2]] | 0x3;			//Set READ and ACK
						queueMsgR.type = TO_PHY;
						queueMsgR.anyPtr = msg;
						retCode = osMessageQueuePut(							//Send back on physical layer
							queue_phyS_id,
							&queueMsgR,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					} 
					else																				//Source is my address ?
					{
						msg[3+msg[2]] = msg[3+msg[2]] | 0x3;			//Set READ and ACK
						queueMsgR.type = DATABACK;								//Send databack to mac_sender
						queueMsgR.anyPtr = msg;
						retCode = osMessageQueuePut(
							queue_macS_id,
							&queueMsgR,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}

					data_ptr[msg[2]] = '\0';										//Add \0 to the data to mark the end of the message
					queueMsgR.anyPtr = data_ptr;

					queueMsgR.type = DATA_IND;				
					if((msg[1]&0x03) == CHAT_SAPI)							//SAPI is for chat?
					{
						retCode = osMessageQueuePut(							//Send data to chat_receiver
							queue_chatR_id,		
							&queueMsgR,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					} 
					else if((msg[1]&0x03) == TIME_SAPI)					//SAPI is for time?
					{
						retCode = osMessageQueuePut(							//Send data to time_receiver
							queue_timeR_id,
							&queueMsgR,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}
				} 
				else																					//Checksum is incorrect?
				{
					length = msg[2];
					msg[length+3] = (msg[length+3] & 0xFC) | (0x2 & 0x3);		//Set READ and clear ACK
					queueMsgR.type = TO_PHY;
					retCode = osMessageQueuePut(								//Send back to physical layer
						queue_phyS_id,
						&queueMsgR,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				}
			}
			//--------------------------------------------------------------------------
			// SOURCE ADDRESS IS MY ADDRESS	
			//--------------------------------------------------------------------------	
			else if(msg[0]>>3 == gTokenInterface.myAddress)	
			{
				queueMsgR.type = DATABACK;										//Send databack to mac_sender
				retCode = osMessageQueuePut(
					queue_macS_id,
					&queueMsgR,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
			}
			//--------------------------------------------------------------------------
			// NEITHER SOURCE OR DESTINATION IS M ADDRESS => Not meant for us
			//--------------------------------------------------------------------------	
			else																						
			{
				queueMsgR.type = TO_PHY;
				retCode = osMessageQueuePut(									//Send back to physical layer
					queue_phyS_id,
					&queueMsgR,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
			}
		}	
	}
}
