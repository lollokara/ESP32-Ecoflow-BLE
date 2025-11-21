#ifndef PB_UTILS_H
#define PB_UTILS_H

#include <pb_decode.h>
#include <vector>

bool pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool pb_decode_to_vector(pb_istream_t *stream, const pb_field_t *field, void **arg);

#endif // PB_UTILS_H
