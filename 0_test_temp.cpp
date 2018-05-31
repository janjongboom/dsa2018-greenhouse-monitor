#include "select_program.h"

#if PROGRAM == TEST_TEMP

#include "mbed.h"
#include "DHT.h"

DHT sensor(D7, SEN51035P); // Temperature sensor
AnalogIn moist(A2);

int main() {
    int err;
    printf("\r\nDHT Test program");
    printf("\r\n******************\r\n");
    wait(1); // wait 1 second for device stable status
    while (1) {
        err = sensor.readData();
        if (err == 0) {
            printf("Temperature is %4.2f C \r\n",sensor.ReadTemperature(CELCIUS));
            printf("Temperature is %4.2f F \r\n",sensor.ReadTemperature(FARENHEIT));
            printf("Temperature is %4.2f K \r\n",sensor.ReadTemperature(KELVIN));
            printf("Humidity is %4.2f \r\n",sensor.ReadHumidity());
            printf("Dew point is %4.2f  \r\n",sensor.CalcdewPoint(sensor.ReadTemperature(CELCIUS), sensor.ReadHumidity()));
            printf("Dew point (fast) is %4.2f  \r\n",sensor.CalcdewPointFast(sensor.ReadTemperature(CELCIUS), sensor.ReadHumidity()));
        } else {
            printf("\r\nErr %i \n",err);
        }
        printf("Moisture is %.3f\n", moist.read());

        wait(5);
    }
}


#endif