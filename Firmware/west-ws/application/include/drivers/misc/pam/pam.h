#ifndef __PAM_H__
#define __PAM_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include "Protos/rpc.pb.h"

int pam_ping(const struct device *dev, k_timeout_t timeout);
int pam_post_record(const struct device *dev, Record *record, k_timeout_t timeout);

#endif // __PAM_H__