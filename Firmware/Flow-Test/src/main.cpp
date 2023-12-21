#include <Arduino.h>

#include "USB.h"

#include "ICM_20948.h"
#include "SparkFun_SDP3x_Arduino_Library.h"
#include "Adafruit_NeoPixel.h"

#include <RPC.h>
#include <RPCI2C.h>

// #define SERIAL_PORT USBSerial

#define WIRE_PORT Wire

#define AD0_VAL 1

RPCController rpc(0);
RPCI2C rpc_io(&WIRE_PORT);

ICM_20948_I2C imu;
SDP3X sdp_a;
SDP3X sdp_b;

Adafruit_NeoPixel pixels(1, 13, NEO_GRB + NEO_KHZ800);

float accX, accY, accZ, gyrX, gyrY, gyrZ, magX, magY, magZ, tempC, tempF, roll, pitch, heading;
float pressA, pressB, tempAC, tempAF, tempBC, tempBF;

void I2C_OnReceive(int len)
{

}

void I2C_OnRequest()
{
	String message = 
		String(accX) + ","
		+ String(accY) + ","
		+ String(accZ) + ","
		+ String(gyrX) + ","
		+ String(gyrY) + ","
		+ String(gyrZ) + ","
		+ String(magX) + ","
		+ String(magY) + ","
		+ String(magZ) + ","
		+ String(tempC) + ","
		+ String(tempF) + ","
		+ String(pressA) + ","
		+ String(pressB) + ","
		+ String(tempAC) + ","
		+ String(tempBC);

	// SERIAL_PORT.printf("--\n%s\n--\n", message.c_str());

	uint8_t message_bytes[128] = { 0 };
	memcpy(message_bytes, message.c_str(), message.length());

	Wire1.write(message_bytes, sizeof(message_bytes));
}

void setup()
{
	// SERIAL_PORT.begin(115200);

	pixels.begin();
	pixels.setPixelColor(0, pixels.Color(0, 0, 255));
	pixels.show();

	WIRE_PORT.setPins(15, 14);
	WIRE_PORT.begin();
	WIRE_PORT.setClock(400000);

	// Wire1.onReceive(I2C_OnReceive);
	// Wire1.onRequest(I2C_OnRequest);
	// Wire1.begin(0x11, 37, 36, 100000);	

	WIRE_PORT.onReceive([](int numBytes) { rpc_io.onReceive(numBytes); });
	WIRE_PORT.onRequest([]() { rpc_io.onRequest(); });

	// pinMode(36, INPUT);
	// pinMode(37, OUTPUT);

	// while (true)
	// {
	// 	SERIAL_PORT.println("HIGH");
	// 	// digitalWrite(36, HIGH);
	// 	digitalWrite(37, HIGH);
	// 	delay(10);

	// 	SERIAL_PORT.println("LOW");
	// 	// digitalWrite(36, LOW);
	// 	digitalWrite(37, LOW);
	// 	delay(10);
	// }

	delay(1000);

	int nDevices = 0;
  for(int address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
 
    // if (error == 0)
    // {
    //   SERIAL_PORT.print("I2C device found at address 0x");
    //   if (address<16)
    //     SERIAL_PORT.print("0");
    //   SERIAL_PORT.print(address,HEX);
    //   SERIAL_PORT.println("  !");
 
    //   nDevices++;
    // }
    // else
    // {
    //   SERIAL_PORT.print("Unknown error at address 0x");
    //   if (address<16)
    //     SERIAL_PORT.print("0");
    //   SERIAL_PORT.print(address,HEX);
	// SERIAL_PORT.print(" ");
	// SERIAL_PORT.println(error);
    // }    
  }

//   imu.enableDebugging(SERIAL_PORT);


	bool initialized = false;
	// while (!initialized)
	for (uint8_t attempt = 0; (attempt < 5) && !initialized; attempt++	)
	{
		imu.begin(WIRE_PORT, AD0_VAL);
		// SERIAL_PORT.print(F("Initialization of the sensor returned: "));
		// SERIAL_PORT.println(imu.statusString());
		if (imu.status != ICM_20948_Stat_Ok)
		{
			// SERIAL_PORT.println("Trying again...");
			delay(500);
		}
		else
		{
			initialized = true;
		}
	}

	sdp_a.stopContinuousMeasurement(0x21);
	sdp_b.stopContinuousMeasurement(0x22);

	if (!sdp_a.begin(0x21)) {
		// SERIAL_PORT.println("SDP3x A not found");
		while (1);
	}

	if (!sdp_b.begin(0x22)) {
		// SERIAL_PORT.println("SDP3x B not found");
		while (1);
	}

	sdp_a.startContinuousMeasurement(true, true);
	sdp_b.startContinuousMeasurement(true, true);

	accX = 0;
	accY = 0;
	accZ = 0;
	gyrX = 0;
	gyrY = 0;
	gyrZ = 0;
	magX = 0;
	magY = 0;
	magZ = 0;
	tempC = 0;
	tempF = 0;
	roll = 0;
	pitch = 0;
	heading = 0;
	pressA = 0;
	pressB = 0;
	tempAC = 0;
	tempAF = 0;
	tempBC = 0;
	tempBF = 0;
}

uint32_t loop_count = 0;

void loop()
{

	loop_count++;
	uint8_t g = ((4 * loop_count) & 0x300) ? 0x00 : 0xFF;
	uint8_t r = ((4 * loop_count) & 0x100) ? 0xFF : 0x00;
	uint8_t b = ((4 * loop_count) & 0x200) ? 0xFF : 0x00;
	uint8_t value = (127 + pixels.sine8(loop_count * 4 * 2)) / 4;
	pixels.setPixelColor(0, pixels.Color(r & value, g & value, b & value));
	pixels.show();

	if (imu.dataReady())
	{
		imu.getAGMT();
		accX = imu.accX();
		accY = imu.accY();
		accZ = imu.accZ();
		gyrX = imu.gyrX();
		gyrY = imu.gyrY();
		gyrZ = imu.gyrZ();
		magX = imu.magX();
		magY = imu.magY();
		magZ = imu.magZ();
		tempC = imu.temp();
	}

	sdp_a.readMeasurement(&pressA, &tempAC);
	sdp_b.readMeasurement(&pressB, &tempBC);
	
	tempF = tempC * 1.8f + 32.0f;
	tempAF = tempAC * 1.8f + 32.0f;
	tempBF = tempBC * 1.8f + 32.0f;

	float accel_mag = sqrtf(accX * accX + accY * accY + accZ * accZ);
	float accel_x_norm = accX / accel_mag;
	float accel_y_norm = accY / accel_mag;
	float accel_z_norm = accZ / accel_mag;
	roll = atan2(accel_y_norm, accel_z_norm) * 180.0f / M_PI;
	pitch = atan2(-accel_x_norm, sqrtf(accel_y_norm * accel_y_norm + accel_z_norm * accel_z_norm)) * 180.0f / M_PI;

	heading = atan2(magY, magX) * 180.0f / M_PI;
	
	// SERIAL_PORT.print("$");
	// SERIAL_PORT.printf("%+04.02f", accX);		// 0
	// SERIAL_PORT.printf(", %+04.02f", accY);		// 1
	// SERIAL_PORT.printf(", %+04.02f", accZ);		// 2
	// SERIAL_PORT.printf(", %+04.02f", gyrX);		// 3
	// SERIAL_PORT.printf(", %+04.02f", gyrY);		// 4
	// SERIAL_PORT.printf(", %+04.02f", gyrZ);		// 5
	// SERIAL_PORT.printf(", %+04.02f", magX);		// 6
	// SERIAL_PORT.printf(", %+04.02f", magY);		// 7
	// SERIAL_PORT.printf(", %+04.02f", magZ);		// 8
	// SERIAL_PORT.printf(", %+02.02f", tempC);	// 9
	// SERIAL_PORT.printf(", %+02.02f", tempF);	// 10
	// SERIAL_PORT.printf(", %+03.02f", roll);		// 11
	// SERIAL_PORT.printf(", %+03.02f", pitch);	// 12
	// SERIAL_PORT.printf(", %+03.02f", heading);	// 13
	// SERIAL_PORT.printf(", %+04.02f", pressA);	// 14
	// SERIAL_PORT.printf(", %+04.02f", pressB);	// 15
	// SERIAL_PORT.printf(", %+02.02f", tempAC);	// 16
	// SERIAL_PORT.printf(", %+02.02f", tempAF);	// 17
	// SERIAL_PORT.printf(", %+02.02f", tempBC);	// 18
	// SERIAL_PORT.printf(", %+02.02f", tempBF);	// 19
	// SERIAL_PORT.print(";");
	// SERIAL_PORT.println();

	delay(50);
}
