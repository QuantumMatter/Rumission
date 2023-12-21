#include <zephyr/kernel.h>

#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <Protos/v1/rpc.h>
#include "Protos/rpc.pb.h"
#include "Protos/sample.pb.h"

#include <errno.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rpc_help, LOG_LEVEL_ERR);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

// These functions are a quick and dirty way to copy one message to another
// without concern for the contents of the message. This is useful for
// messages that have a repeated field that may be allocated in a different
// memory space than the message itself.
// We'll pipe one pb_ostream_t to another pb_istream_t
// static bool pb_copy_callback(pb_ostream_t *ostream, const pb_byte_t *buf, size_t count)
// {
//     return pb_read(ostream->state, buf, count);
// }

bool pb_copy(const pb_msgdesc_t *fields, const void *src, void *dest)
{
    // pb_ostream_t ostream = {
    //     .callback = pb_copy_callback,
    //     .state = src,
    //     .max_size = 1024,
    //     .bytes_written = 0,
    // };

    // pb_istream_t istream = {
    //     .callback = pb_copy_callback,
    //     .state = ostream,
    //     .bytes_left = 1024,
    // };

    // return pb_decode(&istream, fields, dest);

    uint8_t buffer[512];
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&ostream, fields, src))
    {
        LOG_ERR("[pb_copy::encode] Could not encode message! %s", PB_GET_ERROR(&ostream));
        return false;
    }

    pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    if (!pb_decode(&istream, fields, dest))
    {
        LOG_ERR("[pb_copy::decode] Could not decode message! %s", PB_GET_ERROR(&istream));
        return false;
    }

    return true;
}


pb_size_t message_data_options[] = {
    Message_request_tag,
    Message_cancel_tag,
    Message_response_tag,
};

int Error_validate(Error *error)
{
    if (!error->has_code) return -Error_code_tag;
    if (error->has_message)
    {
        if (strlen(error->message) > 128) return -Error_message_tag;
    }

    return 0;
}

int InOut_validate(InOut *inout)
{
    if (inout->which_value == InOut_error_tag)
    {
        return Error_validate(&inout->value.error);
    }

    return 0;
}

int response_validate(Response *response)
{
    if (!response->has_type) return -Response_type_tag;
    if (!response->has_result) return -Response_result_tag;

    return InOut_validate(&response->result);
}

int request_validate(Request *request)
{
    if (!request->has_func) return -Request_func_tag;
    return 0;
}

static bool pb_message_checksum(pb_ostream_t *ostream, const uint8_t *buf, size_t count)
{
    uint32_t *checksum = ostream->state;
    for (size_t i = 0; i < count; i++)
    {
        *checksum += buf[i];
    }
    return true;
}

uint32_t message_checksum(Message *message)
{
    uint32_t original_checksum = message->checksum;
    message->checksum = 0;

    uint32_t calculated_checksum = 0;

    pb_ostream_t ostream = {
        .callback = pb_message_checksum,
        .state = &calculated_checksum,
        .max_size = 1024,
        .bytes_written = 0,
    };

    pb_encode(&ostream, Message_fields, message);
    message->checksum = original_checksum;
    return calculated_checksum;
}

int message_validate(Message *message)
{
    // Required fields
    if (message->version != 1) return -Message_version_tag;
    if (message->checksum != message_checksum(message)) return -Message_checksum_tag;

    if (!message->has_m_id) return -Message_m_id_tag;
    if (!message->has_rpc_id) return -Message_rpc_id_tag;
    if (!message->has_source) return -Message_source_tag;
    if (!message->has_target) return -Message_target_tag;

    // Optional fields that are required by this version
    bool data_is_valid = false;
    for (uint8_t i = 0; i < ARRAY_SIZE(message_data_options); i++)
    {
        if (message->which_data == message_data_options[i])
        {
            data_is_valid = true;
            break;
        }
    }
    if (!data_is_valid) return -message_data_options[0];

    if (message->which_data == Message_request_tag)
    {
        return request_validate(&message->data.request);
    }
    else if (message->which_data == Message_cancel_tag)
    {
        return 0;
    }
    else if (message->which_data == Message_response_tag)
    {
        return response_validate(&message->data.response);
    }
    else
    {
        return -message_data_options[0];
    }
}

void indent(int level)
{
    for (int i = 0; i < level; i++)
    {
        printk("\t");
    }
}

void Sample_print(Sample *sample, uint8_t level)
{
    // k_sleep(K_MSEC(50));
    indent(level);    printk("Sample {\n");
    
    if (sample->has_species) { indent(level + 1);  printk("species: %d\n", sample->species); }
    if (sample->has_value)   { indent(level + 1);  printk("value: %f\n", sample->value); }
    if (sample->has_units)   { indent(level + 1);  printk("units: %d\n", sample->units); }

    indent(level);    printk("}\n");
}

void Record_print(Record *record, uint8_t level)
{
    // k_sleep(K_MSEC(50));
    indent(level);    printk("Record {\n");
    if (record->has_timestamp_us) { indent(level + 1);  printk("timestamp_us: %lld\n", record->timestamp_us); }

    indent(level + 1);  printk("samples: [\n");
    for (uint8_t i = 0; i < record->samples_count; i++)
    {
        Sample_print(&record->samples[i], level + 2);
    }
    indent(level + 1);  printk("]\n");

    indent(level);    printk("}\n");
}

void InOut_print(InOut *inout, uint8_t level)
{
    // k_sleep(K_MSEC(50));
    indent(level);    printk("InOut {\n");

    if (inout->which_value == InOut_error_tag)
    {
        indent(level + 1);  printk("error: {\n");
        if (inout->value.error.has_code)    { indent(level + 2);  printk("code: %d\n", inout->value.error.code); }
        if (inout->value.error.has_message) { indent(level + 2);  printk("message: %s\n", inout->value.error.message); }
        indent(level + 1);  printk("}\n");
    }
    else if (inout->which_value == InOut_integer_tag)
    {
        indent(level + 1);  printk("integer: %d\n", inout->value.integer);
    }
    else if (inout->which_value == InOut_record_tag)
    {
        Record_print(&inout->value.record, level + 1);
    }

    indent(level);    printk("}\n");
}

void request_print(Request *request, uint8_t level)
{
    // k_sleep(K_MSEC(50));
    indent(level);     printk("Request {\n");
    if (request->has_func) { indent(level + 1);  printk("func: %d\n", request->func); }
    if (request->has_args) InOut_print(&request->args, level + 1);
    indent(level);     printk("}\n");
}

void response_print(Response *response, uint8_t level)
{
    // k_sleep(K_MSEC(50));
    indent(level);     printk("Response {\n");
    if (response->has_type)   { indent(level + 1);  printk("type: %d\n", response->type); }
    if (response->has_result) InOut_print(&response->result, level + 1);
    indent(level);     printk("}\n");
}

void message_print(Message *message, uint8_t level)
{
    indent(level);      printk("Message {\n");
    indent(level + 1);  printk("version: %d\n", message->version);
    if (message->has_m_id)      { indent(level + 1);  printk("m_id: %d\n", message->m_id); }
    if (message->has_rpc_id)    { indent(level + 1);  printk("rpc_id: %d\n", message->rpc_id); }
    if (message->has_source)    { indent(level + 1);  printk("source: %d\n", message->source); }
    if (message->has_target)    { indent(level + 1);  printk("target: %d\n", message->target); }

    if (message->which_data == Message_request_tag) request_print(&message->data.request, level + 1);
    if (message->which_data == Message_cancel_tag)  { indent(level + 1);  printk("cancel: {}\n"); }
    if (message->which_data == Message_response_tag) response_print(&message->data.response, level + 1);

    indent(level+1);      printk("checksum: %d\n", message->checksum);

    indent(level);      printk("}\n");
}