#ifndef _LORAWAN_NETWORK_H_
#define _LORAWAN_NETWORK_H_

#include "mbed.h"
#include "mbed_trace.h"
#include "mbed_events.h"
#include "LoRaWANInterface.h"
#include "SX1276_LoRaRadio.h"
#include "CayenneLPP.h"
#include "lora_radio_helper.h"

#define MBED_CONF_LORA_APP_PORT     15

// EventQueue is required to dispatch events around
EventQueue ev_queue;

// Constructing Mbed LoRaWANInterface and passing it down the radio object.
static LoRaWANInterface lorawan(radio);

// Application specific callbacks
static lorawan_app_callbacks_t callbacks;

// LoRaWAN stack event handler
static void lora_event_handler(lorawan_event_t event);

static void lorawan_setup(uint32_t devAddr, uint8_t nwkSKey[16], uint8_t appSKey[16], Callback<void(lorawan_event_t)> lw_event_handler) {
    if (devAddr == 0x0) {
        printf("Set your LoRaWAN credentials first!\n");
        return;
    }

    // Enable trace output for this demo, so we can see what the LoRaWAN stack does
    mbed_trace_init();

    if (lorawan.initialize(&ev_queue) != LORAWAN_STATUS_OK) {
        printf("[LNWK][ERR ] LoRa initialization failed!\n");
        return;
    }

    // prepare application callbacks
    callbacks.events = lw_event_handler;
    lorawan.add_app_callbacks(&callbacks);

    // Disable adaptive data rating
    if (lorawan.disable_adaptive_datarate() != LORAWAN_STATUS_OK) {
        printf("[LNWK][ERR ] disable_adaptive_datarate failed!\n");
        return;
    }

    lorawan.set_datarate(0); // SF12BW125

    lorawan_connect_t connect_params;
    connect_params.connect_type = LORAWAN_CONNECTION_ABP;
    // connect_params.connection_u.otaa.dev_eui = DEV_EUI;
    // connect_params.connection_u.otaa.app_eui = APP_EUI;
    // connect_params.connection_u.otaa.app_key = APP_KEY;
    // connect_params.connection_u.otaa.nb_trials = 3;

    connect_params.connection_u.abp.dev_addr = devAddr;
    connect_params.connection_u.abp.nwk_skey = nwkSKey;
    connect_params.connection_u.abp.app_skey = appSKey;

    lorawan_status_t retcode = lorawan.connect(connect_params);

    if (retcode == LORAWAN_STATUS_OK ||
        retcode == LORAWAN_STATUS_CONNECT_IN_PROGRESS) {
    } else {
        printf("[LNWK][ERR ] Connection error, code = %d\n", retcode);
        return;
    }

    printf("[LNWK][INFO] Connection - In Progress...\n");
}

// This is called from RX_DONE, so whenever a message came in
static void receive_message()
{
    uint8_t rx_buffer[50] = { 0 };
    int16_t retcode;
    retcode = lorawan.receive(MBED_CONF_LORA_APP_PORT, rx_buffer,
                              sizeof(rx_buffer),
                              MSG_UNCONFIRMED_FLAG);

    if (retcode < 0) {
        printf("[LNWK][WARN] receive() - Error code %d\n", retcode);
        return;
    }

    printf("[LNWK][INFO] Data received on port %d (length %d): ", MBED_CONF_LORA_APP_PORT, retcode);

    for (uint8_t i = 0; i < retcode; i++) {
        printf("%02x ", rx_buffer[i]);
    }
    printf("\n");
}

// Event handler
bool lorawan_send(CayenneLPP *payload) {
    int16_t retcode = lorawan.send(MBED_CONF_LORA_APP_PORT, payload->getBuffer(), payload->getSize(), MSG_UNCONFIRMED_FLAG);

    // for some reason send() ret\urns -1... I cannot find out why, the stack returns the right number. I feel that this is some weird Emscripten quirk
    if (retcode < 0) {
        retcode == LORAWAN_STATUS_WOULD_BLOCK ? printf("[LNWK][WARN] send - duty cycle violation\n")
                : printf("[LNWK][WARN] send() - Error code %d\n", retcode);
        return false;
    }

    printf("[LNWK][INFO] %d bytes scheduled for transmission\n", retcode);
    return true;
}

#endif // _LORAWAN_NETWORK_H_
