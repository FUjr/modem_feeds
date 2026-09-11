#include "tom_response.h"

#include <json-c/json.h>
#include <string.h>

int tom_sms_send_succeeded(const char *output)
{
    struct json_object *root = NULL;
    struct json_object *status = NULL;
    int succeeded = 0;

    if (!output || !*output)
        return 0;
    root = json_tokener_parse(output);
    if (root && json_object_is_type(root, json_type_object) &&
        json_object_object_get_ex(root, "status", &status) &&
        json_object_is_type(status, json_type_string) &&
        !strcmp(json_object_get_string(status), "success"))
        succeeded = 1;
    if (root)
        json_object_put(root);
    return succeeded;
}
