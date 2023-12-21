#ifndef INCLUDE_LMP91000_H
#define INCLUDE_LMP91000_H

#include <zephyr/device.h>

enum lmp91000_tia_gain {
    TIA_GAIN_EXT =  0b000,
    TIA_GAIN_2K75 = 0b001,
    TIA_GAIN_3K5 =  0b010,
    TIA_GAIN_7K =   0b011,
    TIA_GAIN_14K =  0b100,
    TIA_GAIN_35K =  0b101,
    TIA_GAIN_120K = 0b110,
    TIA_GAIN_350K = 0b111
};

enum lmp91000_rload {
    RLOAD_10 =  0b00,
    RLOAD_33 =  0b01,
    RLOAD_50 =  0b10,
    RLOAD_100 = 0b11
};

enum lmp91000_int_z {
    INT_Z_20 =      0b00,
    INT_Z_50 =      0b01,
    INT_Z_67 =      0b10,
    INT_Z_BYPASS =  0b11
};

enum lmp91000_bias_sign {
    BIAS_NEGATIVE = 0b0,
    BIAS_POSITIVE = 0b1
};

enum lmp91000_bias {
    BIAS_0 =  0b0000,
    BIAS_1 =  0b0001,
    BIAS_2 =  0b0010,
    BIAS_4 =  0b0011,
    BIAS_6 =  0b0100,
    BIAS_8 =  0b0101,
    BIAS_10 = 0b0110,
    BIAS_12 = 0b0111,
    BIAS_14 = 0b1000,
    BIAS_16 = 0b1001,
    BIAS_18 = 0b1010,
    BIAS_20 = 0b1011,
    BIAS_22 = 0b1100,
    BIAS_24 = 0b1101,
};

enum lmp91000_op_mode {
    OP_MODE_DEEP_SLEEP =            0b000,
    OP_MODE_2_LEAD_GALVANIC =       0b001,
    OP_MODE_STANDBY =               0b010,
    OP_MODE_3_LEAD_AMPEROMETRIC =   0b011,
    OP_MODE_TEMP_TIA_OFF =          0b100,
    OP_MODE_TEMP_TIA_ON =           0b101,
};
struct lmp91000_status {
    uint8_t status: 1;
    uint8_t reserved: 7;
} __packed;

struct lmp91000_lock {
    uint8_t lock: 1;
    uint8_t reserved: 7;
} __packed;

struct lmp91000_tiacn {
    uint8_t rload: 2;
    uint8_t tia_gain: 3;
    uint8_t reserved: 3;
} __packed;

struct lmp91000_refcn {
    uint8_t bias: 4;
    uint8_t bias_sign: 1;
    uint8_t int_z: 2;
    uint8_t ref_source: 1;
} __packed;

struct lmp91000_modecn {
    uint8_t op_mode: 3;
    uint8_t reserved: 4;
    uint8_t fet_short: 1;
} __packed;

int lmp91000_lock(const struct device *dev, bool lock);
int lmp91000_set_tia_gain(const struct device *dev, enum lmp91000_tia_gain gain);
int lmp91000_set_rload(const struct device *dev, enum lmp91000_rload rload);
int lmp91000_set_ref_source(const struct device *dev, bool ref_source_ext);
int lmp91000_set_int_z(const struct device *dev, enum lmp91000_int_z int_z);
int lmp91000_set_bias_sign(const struct device *dev, enum lmp91000_bias_sign bias_sign);
int lmp91000_set_op_mode(const struct device *dev, enum lmp91000_op_mode op_mode);

#endif // INCLUDE_LMP91000_H