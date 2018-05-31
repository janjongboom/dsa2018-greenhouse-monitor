#include "select_program.h"

#if PROGRAM == AIR_QUALITY

#include "lorawan_network.h"
#include "dust_sensor.h"

extern EventQueue ev_queue;

static uint32_t DEV_ADDR   =      0x0;
static uint8_t NWK_S_KEY[] =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
static uint8_t APP_S_KEY[] =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

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
