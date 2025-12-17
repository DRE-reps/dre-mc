/*
 * converters.h
 *
 *  Created on: Dec 7, 2025
 *      Author: vbolbat
 */

#ifndef INC_CONVERTERS_H_
#define INC_CONVERTERS_H_

/* took from https://eddy-em.livejournal.com/352482.html */
char* float2str(float x, uint8_t prec);
uint8_t uint_to_str(uint8_t* buf,const uint32_t buf_size, uint32_t value_to_convert);

#endif /* INC_CONVERTERS_H_ */
