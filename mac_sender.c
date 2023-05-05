#include "stm32f7xx_hal.h"

#include <stdio.h>
#include <string.h>

#include "main.h"

//Token Structure
typedef struct newToken
{
	uint8_t startByte;
	uint8_t station[15];
} myToken;

//Create new queue to store multiple message while we can't send them
osMessageQueueId_t queue_buffer_id;


const osMessageQueueAttr_t queue_buffer_attr = {
	.name = "BUFFER_SEND "  	
};

//////////////////////////////////////////////////////////////////////////////////
/// \brief MacSender Thread: run the sender algorithm
//////////////////////////////////////////////////////////////////////////////////
void MacSender(void *argument)
{
	queue_buffer_id = osMessageQueueNew(8,sizeof(struct queueMsg_t),&queue_buffer_attr); 	
	uint8_t length = 0;											//Length of the data
	uint8_t * msg_ptr = 0;									//Pointer to the message
	uint8_t crcCalculate = 0;								//Store the calculate checksum
	struct queueMsg_t queueMsgS;						//Structure containing frame information
	osStatus_t retCode;											//Status of the message queue
	myToken* token_ptr = 0;									//Pointer to token structure
	token_ptr = osMemoryPoolAlloc(memPool,osWaitForever);		//Allocate memory to store the token

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
				//--------------------------------------------------------------------------
				// NEW TOKEN MUST BE CREATED		
				//--------------------------------------------------------------------------	
				case NEW_TOKEN:
					token_ptr->startByte = 0xFF;
					for (int i = 0; i <= 15; i++){		//Fill the station list with 0 and my station with TIME and CHAT SAPI
						if(i == MYADDRESS){
							token_ptr->station[i] = (1 << TIME_SAPI) | (1 << CHAT_SAPI);
						}else{
							token_ptr->station[i] = 0;
						}
					}
					queueMsgS.anyPtr = token_ptr;
					queueMsgS.type = TO_PHY;
					retCode = osMessageQueuePut(			//Send the token freshly created to the physical layer
						queue_phyS_id,
						&queueMsgS,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);					
					break;
					
				//--------------------------------------------------------------------------
				// STARTING THE STATION
				//--------------------------------------------------------------------------	
				case START:
					gTokenInterface.connected = true;
					
					break;
				//--------------------------------------------------------------------------
				// STOPPING THE STATION
				//--------------------------------------------------------------------------
				case STOP:
					gTokenInterface.connected = false;
				
					break;
				//--------------------------------------------------------------------------
				// DATA RECEIVED FROM UPPER LAYER
				//--------------------------------------------------------------------------
				case DATA_IND:
					msg_ptr = osMemoryPoolAlloc(memPool,osWaitForever);							//Allocate memory to store the frame
				
					msg_ptr[0] = ((MYADDRESS << 3) + queueMsgS.sapi) & 0x7F;				//Creating the frame with source
					msg_ptr[1] = ((queueMsgS.addr << 3) + queueMsgS.sapi) & 0x7F;		//Address
					msg_ptr[2] = strlen(queueMsgS.anyPtr);													//Length
					memcpy(&msg_ptr[3],queueMsgS.anyPtr,strlen(queueMsgS.anyPtr));	//Data
					crcCalculate = msg_ptr[0] + msg_ptr[1] + msg_ptr[2];						//Calculate CRC
				
					for(int i = 0; i < msg_ptr[2]; i++)
					{
						crcCalculate += msg_ptr[3+i];																	//Add data to CRC
					}
					
					msg_ptr[3+msg_ptr[2]] = (crcCalculate <<2);											//Put CRC in Status
					
					if(queueMsgS.addr == 15)
					{																																//Broadcast message? => READ = 1, ACK = 1	
						msg_ptr[3+msg_ptr[2]] += 3;
					}

					retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);						//Free memory of the message received
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				  queueMsgS.anyPtr = msg_ptr;
					queueMsgS.type = DATA_IND;
				
					retCode = osMessageQueuePut(																		//Store the frame in the buffer queue to send it when we have the token
						queue_buffer_id,
						&queueMsgS,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
					break;
				//--------------------------------------------------------------------------
				// TOKEN ENTERING THE STATION
				//--------------------------------------------------------------------------
				case TOKEN:
					token_ptr = queueMsgS.anyPtr;
				
					for(int i = 0; i <= 15; i++)																		//Check if station list has changed?
					{
						if(i == MYADDRESS && gTokenInterface.connected == true)				//My station is connected?
						{
							token_ptr->station[i] = (1 << TIME_SAPI) | (1 << CHAT_SAPI);	//Update station list
						} 
						else if(i == MYADDRESS)																				//if not connected
						{
							token_ptr->station[i] = (1 << TIME_SAPI);				
						}
						if(token_ptr->station[i] != gTokenInterface.station_list[i])	//Station status changed?
						{
							gTokenInterface.station_list[i] = token_ptr->station[i];		//Send the new station list to the LCD
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
					
					retCode = osMessageQueueGet( 																		//Get message from our buffer queue
						queue_buffer_id,
						&queueMsgS,
						NULL,
						0); 		
					if(retCode == osOK)																							//A message has been found?
					{
						
						queueMsgS.type = TO_PHY;																			//Send the message to the physical layer
						retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsgS,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
					} 
										
					break;
				//--------------------------------------------------------------------------
				// A MESSAGE FROM US IS RETURNING BACK
				//--------------------------------------------------------------------------
				case DATABACK:
					msg_ptr = queueMsgS.anyPtr;
					length = msg_ptr[2];

					if(((msg_ptr[3+length] & 0x02)>>1) == 1)												//Is READ bit set?
					{
						if(msg_ptr[3+length]&0x01 == 1)																//Is ACK bit set?
						{	
							//--------------------------------------------------------------------------
							// READ = 1 & ACK = 1 => MESSAGE RECEIVED BY THE DESTINATION AND CRC IS OK
							//--------------------------------------------------------------------------
							retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);				//Free memory of the received message
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							queueMsgS.anyPtr = token_ptr;																//Release Token because our message has been properly read
							queueMsgS.type = TO_PHY;
							retCode = osMessageQueuePut(
								queue_phyS_id,
								&queueMsgS,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						} 
						else 	
						{
							//--------------------------------------------------------------------------
							// READ = 1 & ACK = 0 => MESSAGE RECEIVED BY THE DESTINATION AND CRC IS BAD
							//--------------------------------------------------------------------------
							retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);				
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
							
							msg_ptr[3+msg_ptr[2]] -= 2;																	//Clear READ bit
							queueMsgS.anyPtr = msg_ptr;
							queueMsgS.type = TO_PHY;
							retCode = osMessageQueuePut(																//=> Send message back
								queue_phyS_id,
								&queueMsgS,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						}
					}
					else 	
					{
						//--------------------------------------------------------------------------
						// READ = 0 => MESSAGE NOT RECEIVED BY THE DESTINATION (MAC ERROR)
						//--------------------------------------------------------------------------
						retCode = osMemoryPoolFree(memPool,queueMsgS.anyPtr);					//Free memory of the received message
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
						char* errorMsg = osMemoryPoolAlloc(memPool,osWaitForever);		//Allocate memory to store the MAC ERROR message
						errorMsg = "OH NON CEST FAUX \0"; 
						queueMsgS.anyPtr = errorMsg;
						 
						queueMsgS.type = MAC_ERROR;																		//Send the MAC ERROR to the LCD to display the error message
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
