#ifndef __FLOW_H__
#define __FLOW_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <Protos/rpc.pb.h>

int flow_meter_ping(const struct device *dev, k_timeout_t timeout);
int flow_meter_fetch_record(const struct device *dev, Record *record, k_timeout_t timeout);

#endif // __FLOW_H__