/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/watchdog.h>

#include <zephyr/logging/log.h>

#include <drivers/misc/lmp91000/lmp91000.h>
#include <drivers/misc/pam/pam.h>
#include <drivers/sensor/flow.h>
#include <lib/rpc/rpc.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>

#include <stdio.h>
#include <math.h>
#include <string.h>

// #include <pb_encode.h>
// #include <pb_decode.h>
#include <Protos/v1/rpc.h>
#include "Protos/sample.pb.h"
#include "Protos/rpc.pb.h"

#define R_TH_TOP 	(6300)
#define R_0			(1439)
#define T_0			(85+273.15)
#define R_1			(22520)
#define T_1			(5+273.15)
#define BETA		(3424.98)	// ln(R1 / R0) / [(1 / T1) - (1 / T0)]
#define V_REF_V		(2.5)
#define V_REF_MV	(2500)

#define R_O2_SHUNT	(47)
#define O2_GAIN		(50)
#define O2_SLOPE	(0.000004785) 	// (100 uA - 0 uA) / (20.9 % - 0 %) -- Assume linear between zero and 20.9 % concentration
#define O2_FACTOR	(88.936)		// 1 / (R_O2_SHUNT * O2_GAIN * O2_SLOPE)

#define H2_SENSITIVITY	(25)				// 10 to 40 nA/ppm in 400 ppm H2 at 23 C
#define H2_TIA_GAIN		(120000)			// R_TIA
#define H2_GAIN			(120)				// mV / nA // V / A = nV / nA ->  1e6 mV / nA = mv / (nA 1e-6)
#define H2_FACTOR_PPM	(0.000333333)		// mV * [nA / mV] * [ppm / nA] = ppm = mV * (1 / H2_GAIN) * (1 / H2_SENSITIVITY)


#define ADC_MUX_COUNT	(4)

LOG_MODULE_REGISTER(logging_main);

typedef struct obs_state {
	int16_t adc_counts[ADC_MUX_COUNT];
} obs_state_t;

typedef struct calc_state {
	int32_t adc_mv_vals[ADC_MUX_COUNT];

	float h2_temp_c;
	float h2_conc_ppm;
	float o2_temp_c;
	float o2_conc_;
} calc_state_t;


static struct obs_state state = { 0 };
static struct calc_state c_state = { 0 };

const static struct device *ads = DEVICE_DT_GET(DT_NODELABEL(asdf));
const static struct device *lmp = DEVICE_DT_GET(DT_NODELABEL(lmp_afe));
const static struct device *pwr = DEVICE_DT_GET(DT_NODELABEL(ina));
const static struct device *axetris = DEVICE_DT_GET(DT_NODELABEL(axetris));
const static struct device *pwm = DEVICE_DT_GET(DT_NODELABEL(ledc0));
// const struct device *hih = DEVICE_DT_GET(DT_NODELABEL(hih));
// const static struct device *pam = DEVICE_DT_GET(DT_NODELABEL(pam));

const static struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
// const static struct device *uart2 = DEVICE_DT_GET(DT_NODELABEL(uart2));
const static struct device *wdt0_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));


// const static struct device *i2c_rpc = DEVICE_DT_GET(DT_NODELABEL(i2c_rpc));
const static struct device *flow = DEVICE_DT_GET(DT_NODELABEL(flow));

const static struct device *pam = DEVICE_DT_GET(DT_NODELABEL(pam));



Sample samples[] = {
	{
		.has_species = true,
		.species = SPECIE_METHANE_CONCENTRATION,
		.has_value = true,
		.value = 2.0057,
		.has_units = true,
		.units = UNIT_PPM,
	},
	{
		.has_species = true,
		.species = SPECIE_PRESSURE,
		.has_value = true,
		.value = 0,
		.has_units = true,
		.units = UNIT_PASCAL,
	},
	{
		.has_species = true,
		.species = SPECIE_TEMPERATURE,
		.has_value = true,
		.value = 0,
		.has_units = true,
		.units = UNIT_CELSIUS,
	},
	{
		.has_species = true,
		.species = SPECIE_PRESSURE,
		.has_value = true,
		.value = 0,
		.has_units = true,
		.units = UNIT_PASCAL,
	},
	{
		.has_species = true,
		.species = SPECIE_TEMPERATURE,
		.has_value = true,
		.value = 0,
		.has_units = true,
		.units = UNIT_CELSIUS,
	},
	{
		.has_species = true,
		.species = SPECIE_H2_CONCENTRATION,
		.has_value = true,
		.value = 2.0057,
		.has_units = true,
		.units = UNIT_PPM,
	},
	{
		.has_species = true,
		.species = SPECIE_TEMPERATURE,
		.has_value = true,
		.value = 0,
		.has_units = true,
		.units = UNIT_CELSIUS,
	},
	{
		.has_species = true,
		.species = SPECIE_O2_CONCENTRATION,
		.has_value = true,
		.value = 2.0057,
		.has_units = true,
		.units = UNIT_PPM,
	},
	{
		.has_species = true,
		.species = SPECIE_TEMPERATURE,
		.has_value = true,
		.value = 0,
		.has_units = true,
		.units = UNIT_CELSIUS,
	},
};

static Sample *methane_sample = &samples[0];
static Sample *pressure_a_sample = &samples[1];
static Sample *temp_a_sample = &samples[2];
static Sample *pressure_b_sample = &samples[3];
static Sample *temp_b_sample = &samples[4];
static Sample *h2_sample = &samples[5];
static Sample *h2_temp_sample = &samples[6];
static Sample *o2_sample = &samples[7];
static Sample *o2_temp_sample = &samples[8];

Record record = {
	.has_timestamp_us = true,
	.timestamp_us = 0,

	.samples_count = ARRAY_SIZE(samples),
	.samples = samples,
};


// Checks if all of our devices in the
// device tree are ready to use
int check_peripherals(const struct device *devices[], uint8_t num_devices)
{
	for (uint8_t i = 0; i <num_devices; i++)
	{
		if (!device_is_ready(devices[i]))
		{
			printk("%s is not ready!\n", devices[i]->name);
			return -i;
		}
	}

	return 0;
}

#ifdef CONFIG_LMP91000

int configure_afe(const struct device *lmp)
{
	int rc;

	rc = lmp91000_lock(lmp, false);									if (rc != 0) printk("LMP Failed to unlock! %d\n", rc);				// Let's us update register settings
	rc = lmp91000_set_tia_gain(lmp, TIA_GAIN_120K);					if (rc != 0) printk("LMP Failed to set gain! %d\n", rc);			// Set to the highest gain, because we have a SMALL signal
	rc = lmp91000_set_rload(lmp, RLOAD_100);						if (rc != 0) printk("LMP Failed to set load! %d\n", rc);			// Essentially acts a filter; has the slowest response time
	rc = lmp91000_set_ref_source(lmp, false);						if (rc != 0) printk("LMP Failed to set ref source! %d\n", rc);		// Uses external 2.5 V reference; then int-z is 1.25 V
	rc = lmp91000_set_bias_sign(lmp, BIAS_POSITIVE);				if (rc != 0) printk("LMP Failed to set bias sign! %d\n", rc);		// Positive bias voltage - Not required?
	rc = lmp91000_set_op_mode(lmp, OP_MODE_3_LEAD_AMPEROMETRIC);	if (rc != 0) printk("LMP Failed to set mode! %d\n", rc);			// 3 lead amperometric mode
	rc = lmp91000_lock(lmp, true);									if (rc != 0) printk("LMP Failed to unlock! %d\n", rc);				// Prevent accidental register changes

	return rc;
}

#endif // CONFIG_LMP91000

// #ifdef CONFIG_TI_ADS1115

int read_ads_values(const struct device *ads, int16_t *result_arr)
{
	int err;
	struct adc_sequence sequence = {
		// .buffer
		// .buffer_size

		.channels = BIT(0),
		.resolution = 15,
	};

	struct adc_channel_cfg ads_channel_cfg = {
		.gain = ADC_GAIN_1,
		.reference = ADC_REF_INTERNAL,
		.channel_id = 0,
		.differential = false,
		// .input_positive = mux_idx
	};

	for (uint8_t mux_idx = 0; mux_idx < ADC_MUX_COUNT; mux_idx++)
	{
		sequence.buffer = &state.adc_counts[mux_idx];
		sequence.buffer_size = sizeof(state.adc_counts[mux_idx]);

		ads_channel_cfg.input_positive = mux_idx;

		err = adc_channel_setup(ads, &ads_channel_cfg);
		if (err != 0) {
			continue;
		}

		err = adc_read(ads, &sequence);
		if (err != 0) {
			continue;
		}
	}

	return 0;
}

// #endif // CONFIG_TI_ADS1115

void loop_normal(void)
{
	int rc = 0;
	char fm_string[128] = { 0 };
	char pam_string[256] = { 0 };

	(void)read_ads_values(ads, state.adc_counts);

	// process observed state
	int32_t vref_mv = adc_ref_internal(ads);
	for (uint8_t mux_idx = 0; mux_idx < ADC_MUX_COUNT; mux_idx++)
	{
		c_state.adc_mv_vals[mux_idx] = state.adc_counts[mux_idx];
		adc_raw_to_millivolts(vref_mv, ADC_GAIN_1, 15, &c_state.adc_mv_vals[mux_idx]);
	}

	float v;
	v = (float) c_state.adc_mv_vals[2] / 1000;
	c_state.h2_temp_c = (BETA * T_0) / (T_0 * logf(((R_TH_TOP * v) / (V_REF_V - v)) / (R_0)) + BETA) - 273.15;
	h2_temp_sample->value = c_state.h2_temp_c;

	v = (float) c_state.adc_mv_vals[0] / 1000;
	c_state.o2_conc_ = v * O2_FACTOR;
	o2_sample->value = c_state.o2_conc_;

	v = (float) c_state.adc_mv_vals[1] / 1000;
	c_state.o2_temp_c = (BETA * T_0) / (T_0 * logf(((R_TH_TOP * v) / (V_REF_V - v)) / (R_0)) + BETA) - 273.15;
	o2_temp_sample->value = c_state.o2_temp_c;

	v = (float) c_state.adc_mv_vals[3];
	v -= 1250.0;	// Subtract the internal zero, which is the voltage at the non-inverting input of the TIA
	c_state.h2_conc_ppm	= v * H2_FACTOR_PPM;
	h2_sample->value = c_state.h2_conc_ppm;

	printk("Observed State: \n");
	printk("\tADC Counts:\n");
	for (uint8_t mux_idx = 0; mux_idx < ADC_MUX_COUNT; mux_idx++)
	{
		printk("\t\t[%d] %d\n", mux_idx, state.adc_counts[mux_idx]);
	}
	printk("Calc State: \n");
	printk("\tADC Values (mV): \n");
	for (uint8_t mux_idx = 0; mux_idx < ADC_MUX_COUNT; mux_idx++)
	{
		printk("\t\t[%d] %d\n", mux_idx, c_state.adc_mv_vals[mux_idx]);
	}
	printk("\tO2:\n");
	printk("\t\tConc (%%): %f\n", c_state.o2_conc_);
	printk("\t\tTemp (C): %f\n", c_state.o2_temp_c);
	printk("\tH2:\n");
	printk("\t\tConc (ppm): %f\n", c_state.h2_conc_ppm);
	printk("\t\tTemp (C): %f\n", c_state.h2_temp_c);

	struct sensor_value methane;
	rc = sensor_channel_get(axetris, 60, &methane);
	if (rc == 0)
	{
		printk("\tMethane:\n");
		printk("\t\tConc (ppm): %d.%06d\n", methane.val1, methane.val2);
	}

	// struct sensor_value humidity, humidity_temp;
	// rc = sensor_channel_get(hih, SENSOR_CHAN_HUMIDITY, &humidity);
	// if (rc == 0)
	// {
	// 	printk("\tHIH8120:\n");
	// 	printk("\t\tHumidity (%%): %f\n", sensor_value_to_double(&humidity));
	// }
	// else
	// {
	// 	LOG_ERR("Failed to get humidity! %d", rc);
	// }
	// rc = sensor_channel_get(hih, SENSOR_CHAN_AMBIENT_TEMP, &humidity_temp);
	// if (rc == 0)
	// {
	// 	printk("\t\tTemp (C): %f\n", sensor_value_to_double(&humidity_temp));
	// }
	// else
	// {
	// 	LOG_ERR("Failed to get humidity - temp! %d", rc);
	// }

	struct sensor_value v_bus, power, current;
	rc = sensor_sample_fetch(pwr);
	if (rc != 0) {
		LOG_ERR("Failed to fetch INA sample! %d", rc);
	} else {
		sensor_channel_get(pwr, SENSOR_CHAN_VOLTAGE, &v_bus);
		sensor_channel_get(pwr, SENSOR_CHAN_POWER, &power);
		sensor_channel_get(pwr, SENSOR_CHAN_CURRENT, &current);

		printk("Batteries:\n");
		printk("\tPack One:\n");
		printk("\t\tVoltage (V): %f\n", sensor_value_to_double(&v_bus));
		printk("\t\tCurrent (A): %f\n", sensor_value_to_double(&current));
		printk("\t\tPower (W): %f\n", sensor_value_to_double(&power));
	}

	// rc = i2c_read(i2c, fm_string, sizeof(fm_string), 0x11);
	// if (rc == 0)
	// {
	// 	printk("FM:\n");
	// 	printk("\t%s\n", fm_string);
	// }
	// else
	// {
	// 	LOG_ERR("Failed to read FM! %d", rc);
	// 	fm_string[0] = '\0';
	// }


	// uint16_t pam_str_len = sprintf(pam_string, "%d,%d,%d,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%s\n",
	// 	0,
	// 	state.adc_counts[0],
	// 	state.adc_counts[1],
	// 	state.adc_counts[2],
	// 	state.adc_counts[3],
	// 	c_state.o2_conc_,
	// 	c_state.o2_temp_c,
	// 	c_state.h2_conc_ppm,
	// 	c_state.h2_temp_c,
	// 	sensor_value_to_double(&methane),
	// 	sensor_value_to_double(&v_bus),
	// 	sensor_value_to_double(&current),
	// 	sensor_value_to_double(&power),
	// 	fm_string
	// );

	// pam_write_data(pam, pam_string, pam_str_len);
}

void loop_rpc_test(void)
{
	int rc = 0;
	Record flow_record = Record_init_zero;

	// if ((rc = flow_meter_ping(flow, K_MSEC(1000))) == 0)
	// {
	rc = flow_meter_fetch_record(flow, &flow_record, K_MSEC(1000));
	// }

	if (rc == 0) {
		Record_print(&flow_record, 0);

		if (flow_record.samples_count == 4)
		{
			pressure_a_sample->value = flow_record.samples[0].value;
			temp_a_sample->value = flow_record.samples[1].value;
			pressure_b_sample->value = flow_record.samples[2].value;
			temp_b_sample->value = flow_record.samples[3].value;
		}

		pb_release(Record_fields, &flow_record);
	}
	else
	{
		LOG_ERR("Failed to fetch record! %d", rc);
	}
}

void loop_pam_test(void)
{
	// if (pam_ping(pam, K_MSEC(2000)) == 0)
	{
		if (pam_post_record(pam, &record, K_MSEC(2000)) == 0)
		{
			printk("PAM posted record!\n");
		}
		else
		{
			printk("PAM failed to post record!\n");
		}
	}
	// else
	// {
	// 	printk("PAM is dead!\n");
	// }
}

static void wdt_callback(const struct device *wdt_dev, int channel_id) {}

int configure_watchdog(const struct device *wdt)
{
	int err;
	int wdt_channel_id;

	if (!device_is_ready(wdt)) {
		printk("Watchdog device %s is not ready!\n", wdt->name);
		return -1;
	}

	struct wdt_timeout_cfg cfg = {
		.flags = WDT_FLAG_RESET_SOC,

		.window.min = 0,
		.window.max = 15000,

		.callback = wdt_callback
	};

	wdt_channel_id = wdt_install_timeout(wdt, &cfg);

	if (wdt_channel_id == -ENOTSUP)
	{
		printk("Watchdog device %s does not support timeouts!\n", wdt->name);
		cfg.callback = NULL;
		wdt_channel_id = wdt_install_timeout(wdt, &cfg);
	}
	if (wdt_channel_id < 0)
	{
		printk("Watchdog device %s could not be installed!\n", wdt->name);
		return wdt_channel_id;
	}

	err = wdt_setup(wdt, 0);
	if (err != 0)
	{
		printk("Watchdog device %s could not be setup!\n", wdt->name);
		return err;
	}

	return wdt_channel_id;
}

int main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);

	// const struct device *devices_to_check[] = {
	// 	ads, lmp, pwr, axetris, pwm, i2c,
	// 	flow
	// };

	// if (check_peripherals(devices_to_check, ARRAY_SIZE(devices_to_check)) != 0)
	// {
	// 	printk("Failed to initialize peripherals!\n");
	// 	return 0;
	// }

	pwm_set_cycles(pwm, 0, k_us_to_ticks_ceil32(10000), k_us_to_ticks_ceil32(9000), 0);
	configure_afe(lmp);

	k_sleep(K_MSEC(5000));	

	int wdt_channel = configure_watchdog(wdt0_dev);
	if (wdt_channel < 0)
	{
		printk("Failed to configure watchdog!\n");
		return 0;
	}
	
	uint64_t loop_count = 0, loop_start, loop_duration;
	int rc = 0;
	while (true)
	{
		rc = 0;
		loop_count++;
		loop_start = k_uptime_get();

		wdt_feed(wdt0_dev, wdt_channel);

		// LOG_ERR("Iteration %"PRId64, loop_count);

		printk("Loop #%03lld\n", loop_count);

		struct sensor_value methane;
		rc = sensor_channel_get(axetris, 60, &methane);
		if (rc == 0)
		{
			printk("\tMethane:\n");
			printk("\t\tConc (ppm): %d.%06d\n", methane.val1, methane.val2);

			methane_sample->value = sensor_value_to_double(&methane);
		}

		loop_normal();
		loop_rpc_test();
		loop_pam_test();

		// sensor_sample_fetch(hih);
		// loop_duration = k_uptime_delta(&loop_start);
		// k_sleep(K_MSEC(2000 - loop_duration));

	}


	return 0;
}
