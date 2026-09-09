#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <qmodem-sms/pdu.h>

int main(void)
{
    unsigned char pdus[SMS_MAX_PARTS][SMS_MAX_PDU_LENGTH];
    int lengths[SMS_MAX_PARTS];
    char long_text[81];
    int parts;

    memset(long_text, 'A', 80);
    long_text[80] = '\0';
    parts = pdu_encode_ucs2_parts("18675170156", "test短信", pdus, lengths,
                                  SMS_MAX_PARTS, 42);
    assert(parts == 1);
    assert(pdus[0][1] == 0x11);
    assert(lengths[0] > 10);
    parts = pdu_encode_ucs2_parts("18675170156", long_text, pdus, lengths,
                                  SMS_MAX_PARTS, 42);
    assert(parts == 2);
    assert(pdus[0][1] == 0x51 && pdus[1][1] == 0x51);
    assert(memmem(pdus[0], lengths[0], "\x05\x00\x03\x2a\x02\x01", 6));
    assert(memmem(pdus[1], lengths[1], "\x05\x00\x03\x2a\x02\x02", 6));
    assert(pdu_encode_ucs2_parts("10086", "\xc0\xaf", pdus, lengths,
                                 SMS_MAX_PARTS, 1) < 0);
    puts("PASS UCS-2 single, multipart, and invalid UTF-8 encoding");
    return 0;
}
