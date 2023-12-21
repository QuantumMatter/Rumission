#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <lib/rpc/rpc.h>
#include <Protos/v1/rpc.h>
#include <drivers/misc/pam/pam.h>

#include <errno.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define PAM_TARGET_ID   (2)
#define FLOW_TARGET_ID  (3)

const static struct device *uart_socket_1 = DEVICE_DT_GET(DT_NODELABEL(uart_socket_1));
const static struct device *uart_socket_2 = DEVICE_DT_GET(DT_NODELABEL(uart_socket_2));

const static struct device *pam = DEVICE_DT_GET(DT_NODELABEL(pam));
const static struct device *flow = DEVICE_DT_GET(DT_NODELABEL(flow));

static struct rpc rpc = {0};

K_MSGQ_DEFINE(flow_meter_records, sizeof(Record), 4, 4);

uint32_t rpc_ping(const InOut *arg, InOut *result)
{
    printk("test_func called!\n");

    uint32_t arg_int = arg->value.integer;
    uint32_t *result_int = &result->value.integer;

    *result_int = arg_int + 1;

    return 0;
}

/** 
 * @param arg       Data specified from the caller; cleaned up by the RPC lib immidately afterwards
 * @param result    Data to be returned to the caller; cleaned up by RPC after transmitting
 */
uint32_t rpc_post_record(const InOut *arg, InOut *result)
{
    printk("rpc_post_record called\n");

    if (arg->which_value != InOut_record_tag) return -EINVAL;

    // Put the argument into the message queue to be retrieved
    // from the main thread later
    Record record;
    pb_copy(Record_fields, &arg->value.record, &record);
    k_msgq_put(&flow_meter_records, &record, K_NO_WAIT);

    result->which_value = InOut_integer_tag;
    result->value.integer = 0;

    return 0;
}

uint32_t pam_ping(const struct device *dev, uint32_t arg_int, uint32_t *result, k_timeout_t timeout)
{
    InOut arg = {
        .which_value = InOut_integer_tag,
        .value.integer = arg_int,
    };
    InOut result_obj = {
        .which_value = InOut_integer_tag,
    };
    int rc = rpc_call(&rpc, PAM_TARGET_ID, FUNC_PING, &arg, &result_obj, timeout);
    if (rc == 0)
    {
        *result = result_obj.value.integer;
    }
    else
    {
        *result = 0;
    }
    return rc;
}

uint32_t pam_post_record(const struct device *dev, const Record *record, k_timeout_t timeout)
{
    InOut arg = {
        .which_value = InOut_record_tag,
    };
    pb_copy(Record_fields, record, &arg.value.record);

    InOut result = {
        .which_value = InOut_integer_tag,
    };

    return rpc_call(&rpc, PAM_TARGET_ID, FUNC_POST_RECORD, &arg, &result, timeout);
}

bool rpc_pam_read(struct rpc *rpc, Message *message)
{
    return pam_msgq_get(pam, message, K_FOREVER) == 0;
}

bool rpc_pam_write(struct rpc *rpc, Message *message)
{
    printk("Sending message to PAM:\n");
    message_print(message, 0);
    printk("---\n\n");
    return pam_write(pam, message);
}

bool rpc_flow_write(struct rpc *rpc, Message *message)
{
    printk("Sending message to Flow Meter:\n");
    message_print(message, 0);
    printk("---\n\n");
    return pam_write(flow, message);
}

int main(void)
{
    // k_sleep(K_SECONDS(15));

    if (!device_is_ready(uart_socket_1))
    {
        printk("UART is NOT ready\n");
        return -ENODEV;
    }

    if (!device_is_ready(pam))
    {
        printk("PAM is NOT ready\n");
        return -ENODEV;
    }

    rpc_register_function(&rpc, FUNC_PING, rpc_ping, InOut_integer_tag);
    rpc_register_function(&rpc, FUNC_POST_RECORD, rpc_post_record, InOut_record_tag);
    rpc_register_target(&rpc, PAM_TARGET_ID, rpc_pam_read, rpc_pam_write);
    rpc_register_target(&rpc, FLOW_TARGET_ID, rpc_pam_read, rpc_flow_write);

    Record record = { 0 };
    while (true)
    {

        if (k_msgq_get(&flow_meter_records, &record, K_FOREVER) == 0)
        {
            printk("-----Processing record\n");

            record.samples_count += 2;
            record.samples = realloc(record.samples, record.samples_count * sizeof(Sample));

            Sample *sample = &record.samples[record.samples_count - 2];
            sample->has_species = true;
            sample->species = SPECIE_METHANE_CONCENTRATION;
            sample->has_value = true;
            sample->value = 1.99f;
            sample->has_units = false;
            sample->location_count = 0;

            sample = &record.samples[record.samples_count - 1];
            sample->has_species = true;
            sample->species = SPECIE_H2_CONCENTRATION;
            sample->has_value = true;
            sample->value = 0.015f;
            sample->has_units = false;
            sample->location_count = 0;

            if (pam_post_record(pam, &record, K_SECONDS(1)) != 0)
            {
                printk("Error posting record to PAM\n");
            }
        } 
        k_cpu_idle();
    }
    

    return 0;
}

void flow_rx(void*, void*, void*);
K_THREAD_DEFINE(flow_tid, 4096, flow_rx, NULL, NULL, NULL, 5, 0, 0);

void flow_rx(void *p1, void *p2, void *p3)
{
    // Use the poll API to wait for the first message from any device
    const struct device *rpc_devs[] = {
        pam,
        flow,
    };

    struct k_poll_event events[ARRAY_SIZE(rpc_devs)];
    for (uint8_t rpc_dev_idx = 0; rpc_dev_idx < ARRAY_SIZE(rpc_devs); rpc_dev_idx++)
    {
        pam_msgq_poll_init(rpc_devs[rpc_dev_idx], &events[rpc_dev_idx]);
    }

    while (true)
    {
        int rc = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

        if (rc == 0)
        {
            printk("Poll event triggered\n");

            Message message = { 0 };
            for (uint8_t rpc_dev_idx = 0; rpc_dev_idx < ARRAY_SIZE(rpc_devs); rpc_dev_idx++)
            {
                const struct device *rpc_dev = rpc_devs[rpc_dev_idx];
                if (pam_msgq_get(rpc_dev, &message, K_NO_WAIT) == 0)
                {
                    printk("Received message from %s:\n", rpc_dev->name);
                    message_print(&message, 0);
                    printk("---\n\n");
                    rpc_post(&rpc, rpc_dev_idx + 2, &message);
                }
            }
        }

    }
}