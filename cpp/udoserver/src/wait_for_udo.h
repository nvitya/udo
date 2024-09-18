/*
 * wait_for_udo.h
 *
 *  Created on: Aug 25, 2024
 *      Author: vitya
 */

#ifndef DLCORE_WAIT_FOR_UDO_H_
#define DLCORE_WAIT_FOR_UDO_H_

void prepare_udoip_wait(int afd);
bool wait_for_udoip_ms(int atimeout_ms);

#endif /* DLCORE_WAIT_FOR_UDO_H_ */
