#include "SensorManager.h"

SensorManager::SensorManager() {
    // Constructor
}

void SensorManager::i2cBusReset() {
    pinMode(SDA_PIN, OUTPUT_OPEN_DRAIN);
    pinMode(SCL_PIN, OUTPUT_OPEN_DRAIN);

    digitalWrite(SDA_PIN, HIGH);
    digitalWrite(SCL_PIN, HIGH);
    delay(10);

    for (int i = 0; i < 9; i++) {
        digitalWrite(SCL_PIN, LOW);
        delayMicroseconds(10);
        digitalWrite(SCL_PIN, HIGH);
        delayMicroseconds(10);
    }

    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SDA_PIN, HIGH);
    delay(10);
}

void SensorManager::i2cScan() {
    Serial.println("\nI2C scan start");
    byte count = 0;

    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("Found I2C device at 0x");
            Serial.println(addr, HEX);
            count++;
        }
    }

    if (count == 0) Serial.println("No I2C devices found");
    Serial.println("I2C scan done\n");
}

bool SensorManager::begin() {
    Serial.println("SensorManager: Initializing...");

    // ADC setup
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // allows ~0–3.3V range
    pinMode(BAT_ADC, INPUT);

    // I2C bus reset
    i2cBusReset();

    // Initialize I2C
    Wire.begin(SDA_PIN, SCL_PIN, 100000);
    delay(100);

    // Scan I2C bus
    i2cScan();

    // Initialize BH1750 light sensor
    if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
        Serial.println("SensorManager: BH1750 light sensor OK");
    } else {
        Serial.println("SensorManager: BH1750 light sensor FAILED");
        return false;
    }

    // Initialize SHT31 temperature/humidity sensor
    if (sht30.begin(0x44)) {
        Serial.println("SensorManager: SHT31 temp/humidity sensor OK");
    } else {
        Serial.println("SensorManager: SHT31 temp/humidity sensor FAILED");
        return false;
    }

    sensorsInitialized = true;
    Serial.println("SensorManager: All sensors initialized");
    return true;
}

float SensorManager::readLight() {
    if (!sensorsInitialized) return -1.0;
    
    float lux = lightMeter.readLightLevel();
    if (lux < 0) {
        Serial.println("SensorManager: Light sensor read error");
        return -1.0;
    }
    return lux;
}

float SensorManager::readTemperature() {
    if (!sensorsInitialized) return NAN;
    
    float temp = sht30.readTemperature();
    if (isnan(temp)) {
        Serial.println("SensorManager: Temperature read error");
        return NAN;
    }
    return temp;
}

float SensorManager::readHumidity() {
    if (!sensorsInitialized) return NAN;
    
    float hum = sht30.readHumidity();
    if (isnan(hum)) {
        Serial.println("SensorManager: Humidity read error");
        return NAN;
    }
    return hum;
}

float SensorManager::readBatteryVoltage() {
    uint16_t raw = analogRead(BAT_ADC);
    
    // ESP32-C3 ADC is 12-bit (0–4095)
    float v_adc = (raw / 4095.0f) * 3.3f;
    float v_bat = v_adc * ADC_FACTOR;
    
    Serial.print("SensorManager: Battery ADC raw=");
    Serial.print(raw);
    Serial.print("  Voltage=");
    Serial.print(v_bat, 2);
    Serial.println(" V");
    
    return v_bat;
}

SensorData SensorManager::readAllSensors() {
    SensorData data;
    data.isValid = false;

    if (!sensorsInitialized) {
        Serial.println("SensorManager: Sensors not initialized");
        return data;
    }

    Serial.println("SensorManager: Reading all sensors...");

    // Read each sensor
    data.light = readLight();
    data.temperature = readTemperature();
    data.humidity = readHumidity();
    data.battery = readBatteryVoltage();

    // Mark as valid only if critical sensors (light, temp, hum) are valid
    data.isValid = (data.light >= 0) && !isnan(data.temperature) && !isnan(data.humidity);

    if (data.isValid) {
        Serial.print("SensorManager: All sensors OK - Light=");
        Serial.print(data.light);
        Serial.print("lx  Temp=");
        Serial.print(data.temperature);
        Serial.print("°C  Humidity=");
        Serial.print(data.humidity);
        Serial.print("%  Battery=");
        Serial.print(data.battery, 2);
        Serial.println("V");
    } else {
        Serial.println("SensorManager: Some sensors failed to read");
    }

    return data;
}
