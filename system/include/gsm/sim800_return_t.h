/*
 * sim800_return_t.h
 *
 *  Created on: May 25, 2022
 *      Author: mateusz
 */

#ifndef INCLUDE_GSM_SIM800_RETURN_T_H_
#define INCLUDE_GSM_SIM800_RETURN_T_H_

typedef enum sim800_return_t {

	SIM800_OK					= 0,
	SIM800_RX_TERMINATED		= 1,
	SIM800_WRONG_STATE_TO_RX		= 100,
	SIM800_WRONG_STATE_TO_TX		= 101,
	SIM800_WRONG_STATE_TO_CONNECT	= 102,
	SIM800_ADDRESS_AND_PORT_TO_LONG	= 103,
	SIM800_CONNECTING_FAILED		= 104,
	SIM800_RECEIVING_TIMEOUT		= 105,
	SIM800_SEND_FAILED				= 106,

	SIM800_UNSET					= -1

} sim800_return_t;

#endif /* INCLUDE_GSM_SIM800_RETURN_T_H_ */
