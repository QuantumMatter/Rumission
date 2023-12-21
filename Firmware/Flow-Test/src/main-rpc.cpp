#include <Arduino.h>
#include <Wire.h>

#include "USB.h"

#include <RPC.h>
#include <RPCI2C.h>

#include <SimpleTask.h>

#include "SparkFun_SDP3x_Arduino_Library.h"

// #define SERIAL_PORT USBSerial

#define I2C_CTRL_PORT       Wire
#define I2C_TARGET_PORT     Wire1
#define CTRL_SDA_PIN        (15)
#define CTRL_SCL_PIN        (14)
#define TARGET_ADDR         (0x11)
#define TARGET_SDA_PIN      (37)
#define TARGET_SCL_PIN      (36)

#define RPC_MAIN_ADDR       (0)
#define RPC_SELF_ADDR       (3)

#ifdef SERIAL_PORT
#define LOG(...) SERIAL_PORT.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

SDP3X sdp_a;
SDP3X sdp_b;

class Ping: public RPCHandler {
    public:
        Ping(const Message *message, const InOut *arg, InOut *result): RPCHandler(message, arg, result) {};
        ~Ping() {};

    public:
        int loop()
        {
            LOG("Ping: %d\n", this->_arg->value.integer);

            this->_result->which_value = InOut_integer_tag;
            this->_result->value.integer = this->_arg->value.integer + 1;
            return 0;
        };

        int cancel() { return 0; };
};

class GetRecord: public RPCHandler {
    public:
        GetRecord(const Message *message, const InOut *arg, InOut *result): RPCHandler(message, arg, result) {};
        ~GetRecord() {};

        int loop()
        {
            LOG("GetRecord\n");

            this->_result->which_value = InOut_record_tag;
            Record *r = &this->_result->value.record;

            r->has_timestamp_us = true;
            r->timestamp_us = millis();

            // LOG("Samples Array: %p\n", r->samples);

            Species species[] = {
                SPECIE_PRESSURE,
                SPECIE_TEMPERATURE,
                SPECIE_PRESSURE,
                SPECIE_TEMPERATURE,
            };

            float values[] = {
                22.222,
                33.333,
                0,
                0,
            };

            sdp_a.readMeasurement(&values[0], &values[1]);
            sdp_b.readMeasurement(&values[2], &values[3]);

            RPCUnits units[] = {
                UNIT_PASCAL,
                UNIT_CELSIUS,
                UNIT_PASCAL,
                UNIT_CELSIUS,
            };

            r->samples_count = 4;
            r->samples = (Sample*) malloc(sizeof(Sample) * r->samples_count);
            memset(r->samples, 0, sizeof(Sample) * r->samples_count);

            for (uint8_t i = 0; i < r->samples_count; i++)
            {
                r->samples[i].has_species = true;
                r->samples[i].species = species[i];
                r->samples[i].has_units = true;
                r->samples[i].units = units[i];
                r->samples[i].has_value = true;
                r->samples[i].value = values[i];
                r->samples[i].location_count = 0;
            }

            // LOG("GetRecord::loop\n");
            // LOG("Result ptr: %p\n", this->_result);
            // LOG("Record Ptr: %p\n", &this->_result->value.record);
            // LOG("Record Samples Ptr: %p\n", this->_result->value.record.samples);            

            return 0;
        }
        
        int cancel() { return 0; };
};

RPCHandler* RPCController::Dispatch(FUNCTION fun, const Message *message, const InOut *arg, InOut *result)
{
    switch (fun)
    {
        case FUNC_PING:
            return new Ping(message, arg, result);
        case FUNC_GET_RECORD:
            return new GetRecord(message, arg, result);
        default:
            return NULL;
    }
}

class PeriodicLogging : public SimpleTask {
    public:
        PeriodicLogging(int hz): SimpleTask(hz) {};
        ~PeriodicLogging() {};

        void run(int count)
        {
            LOG("Waiting: %d\r\n", count);
        };
};

RPCController rpc(RPC_SELF_ADDR);
RPCI2C rpc_io(&I2C_TARGET_PORT);
PeriodicLogging periodic_logging(1);

void onReceive(int numBytes)
{
    LOG("onReceive\r\n");
    rpc_io.onReceive(numBytes);
}

void onRequest()
{
    rpc_io.onRequest();
}

void setup()
{
    #ifdef SERIAL_PORT
    SERIAL_PORT.begin(115200);
    LOG("Hello, world!\r\n");
    #endif

    I2C_CTRL_PORT.setPins(CTRL_SDA_PIN, CTRL_SCL_PIN);
    I2C_CTRL_PORT.begin();
    I2C_CTRL_PORT.setClock(400000);
    
    sdp_a.stopContinuousMeasurement(0x21);
	sdp_b.stopContinuousMeasurement(0x22);

	if (!sdp_a.begin(0x21)) {
		LOG("SDP3x A not found");
		while (1);
	}

	if (!sdp_b.begin(0x22)) {
		LOG("SDP3x B not found");
		while (1);
	}

    sdp_a.startContinuousMeasurement(true, true);
	sdp_b.startContinuousMeasurement(true, true);

    RPCTarget *target = rpc.RegisterTarget(RPC_MAIN_ADDR, &rpc_io);

    #ifdef SERIAL_PORT
    target->enableDebug(&SERIAL_PORT);
    rpc_io.enableDebug(&SERIAL_PORT);
    #endif

    I2C_TARGET_PORT.onReceive(onReceive);
    I2C_TARGET_PORT.onRequest(onRequest);
    I2C_TARGET_PORT.begin(TARGET_ADDR, TARGET_SDA_PIN, TARGET_SCL_PIN, 100000);
}

void loop()
{
    rpc.loop();
    periodic_logging.loop();

    delay(50);
}