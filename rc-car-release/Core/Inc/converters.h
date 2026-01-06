/*
 * converters.h
 *
 *  Created on: Dec 7, 2025
 *      Author: vbolbat
 */

#ifndef INC_CONVERTERS_H_
#define INC_CONVERTERS_H_

/* took from https://eddy-em.livejournal.com/352482.html */
/* returned str contains \0 */
char* float2str(float x, uint8_t prec); /* prec -> how much numbers the string will contain after . */
/* returned str contains \0 */
uint8_t uint_to_str(uint8_t* buf,const uint32_t buf_size, uint32_t value_to_convert);

#endif /* INC_CONVERTERS_H_ */
