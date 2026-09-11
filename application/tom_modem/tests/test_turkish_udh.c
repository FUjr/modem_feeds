#include <assert.h>
#include <string.h>

#include "../../libqmodem-sms/src/pdu.c"

int main(void)
{
	struct udh_info info;
	unsigned char udh[] = {
		0x40, 0x08, 0x00, 0x03, 0x7a, 0x02, 0x01,
		0x24, 0x01, 0x01
	};
	assert(parse_udh(udh, sizeof(udh), 1, 1, &info) == 0);
	assert(info.concat_ref == 0x7a && info.concat_total == 2 && info.concat_part == 1);
	assert(info.turkish_locking && !info.turkish_single);
	assert(gsm7_locking_to_latin1(0x0b) == 0xd0);
	assert(gsm7bits_to_latin1[0x0b] != gsm7_locking_to_latin1(0x0b));
	assert(parse_udh(udh, 5, 1, 1, &info) < 0);
	return 0;
}
