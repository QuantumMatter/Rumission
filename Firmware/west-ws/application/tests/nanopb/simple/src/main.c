#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <pb_encode.h>
#include <pb_decode.h>

#include <Protos/sample.pb.h>
#include <Protos/rpc.pb.h>

ZTEST(nanopb_tests, test_nanopb_simple)
{
    // Location location[] = {
    //     { true, LOCATION_MAIN, true, "MAIN-001" }
    // };

    // Sample sample[] = {
    //     { true, SPECIE_METHANE_CONCENTRATION, true, UNIT_PPM, true, 1.997f, ARRAY_SIZE(location), location },
    //     { true, SPECIE_CO2_CONCENTRATION, true, UNIT_PPM, true, 205.0f, ARRAY_SIZE(location), location },
    // };

    Message message = {
        .has_m_id = true, .m_id = 1,
        .has_rpc_id = true, .rpc_id = 2,
        .has_target = true, .target = 3,
        .which_data = Message_request_tag,
        .data.request = {
            .has_func = true, .func = FUNC_PING,
        //     .which_args = Request_record_tag,
        //     .args.record = {
        //         .has_timestamp_us = true, .timestamp_us = 0,
        //         .samples_count = ARRAY_SIZE(sample), .samples = sample
        //     }
        }
    };

    printk("Struct Size: %lld\n", sizeof(message));

    pb_ostream_t size_stream = { 0 };
    pb_encode(&size_stream, Message_fields, &message);
    printk("Wire size: % d\n", size_stream.bytes_written);

    uint8_t *buffer = k_calloc(1, size_stream.bytes_written);
    pb_ostream_t ostream = pb_ostream_from_buffer(buffer, size_stream.bytes_written);
    pb_encode(&ostream, Message_fields, &message);

    for (size_t i = 0; i < ostream.bytes_written; ++i) {
        printk("%02x ", buffer[i]);
    }
    printk("\n");

    printk("m_id: %d\n", message.m_id);
    printk("rpc_id: %d\n", message.rpc_id);
    printk("target: %d\n", message.target);

    Message received = Message_init_zero;
    pb_istream_t istream = pb_istream_from_buffer(buffer, ostream.bytes_written);
    pb_decode(&istream, Message_fields, &received);

    zassert_equal(message.has_m_id, received.has_m_id);
    zassert_equal(message.m_id, received.m_id);
    zassert_equal(message.has_rpc_id, received.has_rpc_id);
    zassert_equal(message.rpc_id, received.rpc_id);
    zassert_equal(message.has_target, received.has_target);
    zassert_equal(message.target, received.target);
    zassert_equal(message.which_data, received.which_data);
    zassert_equal(message.data.request.has_func, received.data.request.has_func);
    zassert_equal(message.data.request.func, received.data.request.func);
    zassert_equal(message.data.request.which_args, received.data.request.which_args);
    zassert_equal(message.data.request.args.record.has_timestamp_us, received.data.request.args.record.has_timestamp_us);
    zassert_equal(message.data.request.args.record.timestamp_us, received.data.request.args.record.timestamp_us);
    zassert_equal(message.data.request.args.record.samples_count, received.data.request.args.record.samples_count);
    
    for (uint8_t sample_idx = 0; sample_idx < message.data.request.args.record.samples_count; sample_idx++)
    {
        Sample *message_sample = &message.data.request.args.record.samples[sample_idx];
        Sample *recv_sample = &received.data.request.args.record.samples[sample_idx];

        zassert_equal(message_sample->has_species, recv_sample->has_species);
        zassert_equal(message_sample->species, recv_sample->species);
        zassert_equal(message_sample->has_units, recv_sample->has_units);
        zassert_equal(message_sample->units, recv_sample->units);
        zassert_equal(message_sample->has_value, recv_sample->has_value);
        zassert_equal(message_sample->value, recv_sample->value);
        zassert_equal(message_sample->location_count, recv_sample->location_count);

        for (uint8_t loc_idx = 0; loc_idx < message_sample->location_count; loc_idx++)
        {
            Location *message_location = &message_sample->location[loc_idx];
            Location *recv_location = &recv_sample->location[loc_idx];

            zassert_equal(message_location->has_type, recv_location->has_type);
            zassert_equal(message_location->type, recv_location->type);
            zassert_equal(message_location->has_name, recv_location->has_name);
            zassert_not_equal(message_location->name, recv_location->name);
            zassert_mem_equal(message_location->name, recv_location->name, sizeof(message_location->name));
        }
    }

    k_free(buffer);
    pb_release(Message_fields, &received);   
}

ZTEST_SUITE(nanopb_tests, NULL, NULL, NULL, NULL, NULL);
