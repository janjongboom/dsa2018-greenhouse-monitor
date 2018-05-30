#include "select_program.h"

#if PROGRAM == AIR_QUALITY

#include "lorawan_network.h"
#include "dust_sensor.h"

extern EventQueue ev_queue;

static uint32_t DEV_ADDR   =      0x26011965;
static uint8_t NWK_S_KEY[] =      { 0x3E, 0xF0, 0x3E, 0xD6, 0xA7, 0x48, 0xE1, 0x53, 0x31, 0x95, 0xAC, 0xB3, 0x0B, 0x82, 0x47, 0xEE };
static uint8_t APP_S_KEY[] =      { 0xA9, 0x62, 0xE6, 0x06, 0xC9, 0x48, 0xEE, 0x3A, 0xD3, 0xF2, 0x54, 0x63, 0xC0, 0xEC, 0x11, 0xD4 };

DustSensor dust(D1);

float dust_concentration = 0.0f;
bool dust_updated = false;

void dust_sensor_cb(int lpo, float ratio, float concentration) {
    dust_concentration = concentration;
    dust_updated = true;
}

void check_for_updated_dust() {
    if (dust_updated) {
        dust_updated = false;
        printf("Measured concentration = %.2f pcs/0.01cf\n", dust_concentration);

        CayenneLPP payload(50);
        payload.addAnalogInput(1, dust_concentration / 100.0f); // save space

        lorawan_send(&payload);

        printf("Measuring dust...\n");
        dust.measure(&dust_sensor_cb);
    }
}

int main() {
    printf("=========================================\n");
    printf("      DSA 2018 Air Quality Sensor        \n");
    printf("=========================================\n");

    lorawan_setup(DEV_ADDR, NWK_S_KEY, APP_S_KEY);

    printf("Measuring dust...\n");
    dust.measure(&dust_sensor_cb);

    ev_queue.call_every(3000, &check_for_updated_dust);

    ev_queue.dispatch_forever();
}


#endif
