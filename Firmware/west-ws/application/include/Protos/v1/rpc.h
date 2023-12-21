#ifndef __RMSN_PROTOS_RPC_H__
#define __RMSN_PROTOS_RPC_H__

#include <pb.h>
#include <Protos/rpc.pb.h>

bool pb_copy(const pb_msgdesc_t *fields, const void *src, void *dest);

void Sample_print(Sample *sample, uint8_t level);
void Record_print(Record *record, uint8_t level);
void InOut_print(InOut *inout, uint8_t level);
void request_print(Request *request, uint8_t level);
void response_print(Response *response, uint8_t level);
void message_print(Message *message, uint8_t level);

int message_validate(Message *message);

#endif // __RMSN_PROTOS_RPC_H__