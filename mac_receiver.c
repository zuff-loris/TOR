#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

osStatus_t retCode;
osMessageQueueId_t messageId;
struct queueMsg_t queueMsg;

//////////////////////////////////////////////////////////////////////////////////
/// \brief Send message on the message queue
/// \param theId The id corresponding to the class we want to send a message
//////////////////////////////////////////////////////////////////////////////////
void SendMessage(osMessageQueueId_t theId){
	retCode = osMessageQueuePut(
		theId,
		&queueMsg,
		osPriorityNormal,
		osWaitForever);
	CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
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
	for(;;){
		//Get the message on the queue
		retCode = osMessageQueueGet(
			queue_macR_id,
			&queueMsg,
			NULL,
			osWaitForever);
		
		//Test if the message is succesfully read
		if(retCode == osOK){
			msg_ptr = queueMsg.anyPtr;
			
			if(msg_ptr[1] == TOKEN_TAG)			//token frame received
			{				
				queueMsg.type = TOKEN;				//send token to MAC Sender
				SendMessage(queue_macS_id);
			} 
			else if(msg_ptr[1]>>3 == gTokenInterface.myAddress ||
								msg_ptr[1]>>3 == BROADCAST_ADDRESS)							//destination is my address or broadcast frame
			{		
				controlCS //send to mac sender that checksum is incorrect
				queueMsg.type = DATA_IND;				
				if(msg_ptr[1]&0x03 == CHAT_SAPI)		//SAPI number is for chat
				{
					SendMessage(queue_chatR_id);
				} else if(msg_ptr[1]&0x03 == TIME_SAPI)
				{
					SendMessage(queue_timeR_id);
				}
				
			} 
			else if(msg_ptr[0]>>3 == gTokenInterface.myAddress)			//source is my address
			{		
				queueMsg.type = DATABACK;			//send databack to MAC Sender
				SendMessage(queue_macS_id);
			}
			else 														//message not meant for us, send back to physical layer
			{
				queueMsg.type = TO_PHY;
				SendMessage(queue_phyS_id);
			}
		}
	}
}
