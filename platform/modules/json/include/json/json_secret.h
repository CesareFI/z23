/* Copyright 2026 Rhett Creighton - Apache License 2.0 */

#ifndef ZCL_JSON_SECRET_H
#define ZCL_JSON_SECRET_H

#include "json/json.h"
#include "base/cleanse.h"

#include <string.h>

/* Overwrite every owned string value and object key in place without changing
 * the tree shape. Call immediately before json_free() for secret-bearing
 * documents whose storage has reached its last use. */
static inline void json_cleanse_strings(struct json_value *value)
{
    if (!value)
        return;
    if (value->type == JSON_STR && value->val.s)
        memory_cleanse(value->val.s, strlen(value->val.s) + 1);
    for (size_t i = 0; i < value->num_children; i++) {
        json_cleanse_strings(&value->children[i]);
        if (value->keys && value->keys[i])
            memory_cleanse(value->keys[i], strlen(value->keys[i]) + 1);
    }
}

#endif /* ZCL_JSON_SECRET_H */
