#include "debug.h"
#include "stdio.h"
#include "main.h"

int _write(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    ITM_SendChar(*ptr++);
  }
  return len;
}

void print_float_simple(float value) {
    int integer_part = (int)value;
    int fractional_part = (int)((value - integer_part) * 100);
    printf("%d.%02d\n", integer_part, fractional_part);
}
