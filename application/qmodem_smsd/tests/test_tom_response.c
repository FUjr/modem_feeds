#include "tom_response.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(tom_sms_send_succeeded("{\"status\":\"success\"}"));
    assert(!tom_sms_send_succeeded("{\"status\":\"failed\",\"reason\":\"comm_error\"}"));
    assert(!tom_sms_send_succeeded("{\"status\":\"failed\",\"reason\":\"send_sms_failed\"}"));
    assert(!tom_sms_send_succeeded("not json"));
    assert(!tom_sms_send_succeeded(""));
    assert(!tom_sms_send_succeeded(NULL));
    puts("PASS: tom_modem SMS response status validation");
    return 0;
}
