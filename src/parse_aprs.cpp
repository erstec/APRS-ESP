#include <Arduino.h>
#include <math.h>
#include <parse_aprs.h>
#include <string.h>
#include <pbuf.h>

uint16_t ParseAPRS::passCode(char *theCall)
{
	char rootCall[10]; // need to copy call to remove ssid from parse
	char *p1 = rootCall;

	while ((*theCall != '-') && (*theCall != 0))
		*p1++ = toupper(*theCall++);
	*p1 = 0;

	short hash = 0x73e2; // Initialize with the key value
	short i = 0;
	short len = strlen(rootCall);
	char *ptr = rootCall;

	while (i < len)
	{						   // Loop through the string two bytes at a time
		hash ^= (*ptr++) << 8; // xor high byte with accumulated hash
		hash ^= (*ptr++);	   // xor low byte with accumulated hash
		i += 2;
	}
	return hash & 0x7fff; // mask off the high bit so number is always positiv
}
