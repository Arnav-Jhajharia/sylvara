#pragma once

#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_SHT31.h>

// I2C Pins
#define SDA_PIN  4
#define SCL_PIN  5

// ADC Pins
#define BAT_ADC    1        // GPIO1 - battery voltage
#define ADC_FACTOR 1.1793   // voltage divider correction

// Sensor data structure
struct SensorData {
    float light;        // Lux
    float temperature;  // °C
    float humidity;     // %
    float battery;      // Volts
    bool isValid;       // True if all sensors read successfully
};

class SensorManager {
public:
    SensorManager();
    
    // Initialize I2C and sensors
    bool begin();
    
    // Read all sensors and return data struct
    SensorData readAllSensors();
    
    // Individual sensor read functions
    float readLight();
    float readTemperature();
    float readHumidity();
    float readBatteryVoltage();
    
    // Utility
    void i2cScan();

private:
    BH1750 lightMeter;
    Adafruit_SHT31 sht30;
    
    void i2cBusReset();
    bool sensorsInitialized = false;
};
