#include "RPC.h"

#include <pb_encode.h>
#include <pb_decode.h>

bool pb_copy(const pb_msgdesc_t *fields, const void *src, void *dest)
{
    uint8_t buffer[2048];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    pb_encode(&ostream, fields, src);

    pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    return pb_decode(&istream, fields, dest);
}