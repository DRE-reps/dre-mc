/*
 * converters.c
 *
 *  Created on: Dec 7, 2025
 *      Author: vbolbat
 */
#include "logger.h"
#include "main.h"
//took from https://eddy-em.livejournal.com/352482.html
static const float pwr10[] = {1., 10., 100., 1000., 10000.};
static const float rounds[] = {0.5, 0.05, 0.005, 0.0005, 0.00005};
#define P10L  (sizeof(pwr10)/sizeof(uint32_t) - 1)
char* float2str(float x, uint8_t prec){
#ifdef __LOGGING__
	save_log(FLOAT2STR);
#endif
    if(prec > P10L) prec = P10L;
    static char str[16] = {0}; // -117.5494E-36\0 - 14 symbols max!
    char *s = str + 14; // go to end of buffer
    uint8_t minus = 0;
    if(x < 0){
        x = -x;
        minus = 1;
    }
    int pow = 0; // xxxEpow
    // now convert float to 1.xxxE3y
    while(x > 1000.f){
        x /= 1000.f;
        pow += 3;
    }
    if(x > 0) while(x < 1.){
        x *= 1000.f;
        pow -= 3;
    }
    // print Eyy
    if(pow){
        uint8_t m = 0;
        if(pow < 0){pow = -pow; m = 1;}
        while(pow){
            register int p10 = pow/10;
            *s-- = '0' + (pow - 10*p10);
            pow = p10;
        }
        if(m) *s-- = '-';
        *s-- = 'E';
    }
    // now our number is in [1, 1000]
    uint32_t units;
    if(prec){
        units = (uint32_t) x;
        uint32_t decimals = (uint32_t)((x-units+rounds[prec])*pwr10[prec]);
        // print decimals
        while(prec){
            register int d10 = decimals / 10;
            *s-- = '0' + (decimals - 10*d10);
            decimals = d10;
            --prec;
        }
        // decimal point
        *s-- = '.';
    }else{ // without decimal part
        units = (uint32_t) (x + 0.5);
    }
    // print main units
    if(units == 0) *s-- = '0';
    else while(units){
        register uint32_t u10 = units / 10;
        *s-- = '0' + (units - 10*u10);
        units = u10;
    }
    if(minus) *s-- = '-';
    return s+1;
}

uint8_t uint_to_str(uint8_t* buf,const uint32_t buf_size, uint32_t value_to_convert)
{
	//чистим buff
	for (int i = 0; i < buf_size; i++)
	{
		buf[i] = 0;
	}
	uint32_t counter = 0;
	uint32_t var = value_to_convert;
	while (1) //считаем количество цифр
	{
		var /= 10;
		counter++;
		if (var == 0)
		{
			break;
		}
	}
	if (counter >= buf_size) //В конце обязательно должен остаться 0 (\0)
	{
		return 1; //RESP_ERR, buf_size not enough to return valid value
	}
	for (int i = counter - 1;i>=0;i--)
	{
		buf[i] = value_to_convert%10 + '0';
		value_to_convert /= 10;
	}
	return 0; //RESP_OK , buf_size enough to return valid value
}
