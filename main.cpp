#include "mbed.h"
#include "mbed_trace.h"
#include "mbed_events.h"
#include "LoRaWANInterface.h"
#include "SX1276_LoRaRadio.h"
#include "CayenneLPP.h"
#include "lora_radio_helper.h"
#include "DHT.h"

#define SEND_INTERVAL           10

// OTAA Credentials
static uint8_t DEV_EUI[] = { 0x00, 0x56, 0x82, 0x37, 0xD3, 0xDA, 0xF2, 0x10 };
static uint8_t APP_EUI[] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x00, 0xF4, 0x08 };
static uint8_t APP_KEY[] = { 0x8F, 0x18, 0x7B, 0x24, 0x47, 0x27, 0x35, 0x79, 0x8E, 0xBD, 0xAC, 0x8A, 0xBF, 0x35, 0x44, 0xAB };


// 0x6872E6DC7C50E0E102722FE942A4FB27

// The port we're sending and receiving on
#define MBED_CONF_LORA_APP_PORT     15

// Peripherals (LoRa radio, temperature sensor and button)
#ifdef TARGET_XDOT_L151CC
InterruptIn btn(GPIO1);
#else
InterruptIn btn(BUTTON1);
#endif

// EventQueue is required to dispatch events around
static EventQueue ev_queue;

// Constructing Mbed LoRaWANInterface and passing it down the radio object.
static LoRaWANInterface lorawan(radio);

// Application specific callbacks
static lorawan_app_callbacks_t callbacks;

// LoRaWAN stack event handler
static void lora_event_handler(lorawan_event_t event);

static DHT dht(PC_8, 22);
static AnalogIn moist(A3);

// Send a message over LoRaWAN
static void send_message() {
    CayenneLPP payload(50);

    int retryCount = 3;

    float temp = 0.0f;
    float humi = 0.0f;
    int r;

    while (--retryCount > 0) {
        r = dht.readData();
        if (r != ERROR_NONE) {
            printf("err=%d\n", r);
            wait_ms(2100);
            continue;
        }
        else {
            temp = dht.ReadTemperature(CELCIUS);
            humi = dht.ReadHumidity();
            break;
        }
    }

    if (r != ERROR_NONE) {
        printf("Could not read DHT data: %d\n", r);
    }
    else {
        payload.addTemperature(2, temp);
        payload.addRelativeHumidity(3, humi);
        printf("Temp=%f Humi=%f\n", temp, humi);
    }

    printf("Humi=%f\n", moist.read());

    payload.addAnalogInput(4, moist.read());

    printf("Sending %d bytes\n", payload.getSize());

    int16_t retcode = lorawan.send(MBED_CONF_LORA_APP_PORT, payload.getBuffer(), payload.getSize(), MSG_UNCONFIRMED_FLAG);

    // for some reason send() ret\urns -1... I cannot find out why, the stack returns the right number. I feel that this is some weird Emscripten quirk
    if (retcode < 0) {
        retcode == LORAWAN_STATUS_WOULD_BLOCK ? printf("send - duty cycle violation\n")
                : printf("send() - Error code %d\n", retcode);
        return;
    }

    printf("%d bytes scheduled for transmission\n", retcode);
}

int main() {
    if (DEV_EUI[0] == 0x0 && DEV_EUI[1] == 0x0 && DEV_EUI[2] == 0x0 && DEV_EUI[3] == 0x0 && DEV_EUI[4] == 0x0 && DEV_EUI[5] == 0x0 && DEV_EUI[6] == 0x0 && DEV_EUI[7] == 0x0) {
        printf("Set your LoRaWAN credentials first!\n");
        return -1;
    }

    printf("Welcome to DSA 2018, sending every %d seconds\n", SEND_INTERVAL);

    // Enable trace output for this demo, so we can see what the LoRaWAN stack does
    mbed_trace_init();

    if (lorawan.initialize(&ev_queue) != LORAWAN_STATUS_OK) {
        printf("LoRa initialization failed!\n");
        return -1;
    }

    // Fire a message when the button is pressed
    // btn.fall(ev_queue.event(&send_message));
    ev_queue.call_every(SEND_INTERVAL * 1000, callback(&send_message));

    // prepare application callbacks
    callbacks.events = mbed::callback(lora_event_handler);
    lorawan.add_app_callbacks(&callbacks);

    // Disable adaptive data rating
    if (lorawan.disable_adaptive_datarate() != LORAWAN_STATUS_OK) {
        printf("disable_adaptive_datarate failed!\n");
        return -1;
    }

    lorawan.set_datarate(3); // SF9BW125

    lorawan_connect_t connect_params;
    connect_params.connect_type = LORAWAN_CONNECTION_ABP;
    connect_params.connection_u.otaa.dev_eui = DEV_EUI;
    connect_params.connection_u.otaa.app_eui = APP_EUI;
    connect_params.connection_u.otaa.app_key = APP_KEY;
    connect_params.connection_u.otaa.nb_trials = 3;

    connect_params.connection_u.abp.dev_addr = 0x26011378;

    static uint8_t nwk[] = { 0x64, 0x55, 0x77, 0x88, 0xF2, 0x25, 0x76, 0xA9, 0x89, 0xE3, 0x1D, 0x08, 0x1F, 0x3D, 0xDF, 0x47 };
    static uint8_t app[] = { 0x0F, 0x60, 0xE1, 0x0F, 0x34, 0x8E, 0x60, 0x30, 0x29, 0xB2, 0xCD, 0xA2, 0x29, 0x05, 0xAA, 0xBF };

    connect_params.connection_u.abp.nwk_skey = nwk;
    connect_params.connection_u.abp.app_skey = app;

    lorawan_status_t retcode = lorawan.connect(connect_params);

    if (retcode == LORAWAN_STATUS_OK ||
        retcode == LORAWAN_STATUS_CONNECT_IN_PROGRESS) {
    } else {
        printf("Connection error, code = %d\n", retcode);
        return -1;
    }

    printf("Connection - In Progress ...\r\n");

    // make your event queue dispatching events forever
    ev_queue.dispatch_forever();

    return 0;
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
        printf("receive() - Error code %d\n", retcode);
        return;
    }

    printf("Data received on port %d (length %d): ", MBED_CONF_LORA_APP_PORT, retcode);

    for (uint8_t i = 0; i < retcode; i++) {
        printf("%02x ", rx_buffer[i]);
    }
    printf("\n");
}

// Event handler
static void lora_event_handler(lorawan_event_t event) {
    switch (event) {
        case CONNECTED:
            printf("Connection - Successful\n");
            break;
        case DISCONNECTED:
            ev_queue.break_dispatch();
            printf("Disconnected Successfully\n");
            break;
        case TX_DONE:
            printf("Message Sent to Network Server\n");
            break;
        case TX_TIMEOUT:
        case TX_ERROR:
        case TX_CRYPTO_ERROR:
        case TX_SCHEDULING_ERROR:
            printf("Transmission Error - EventCode = %d\n", event);
            break;
        case RX_DONE:
            printf("Received message from Network Server\n");
            receive_message();
            break;
        case RX_TIMEOUT:
        case RX_ERROR:
            printf("Error in reception - Code = %d\n", event);
            break;
        case JOIN_FAILURE:
            printf("OTAA Failed - Check Keys\n");
            break;
        default:
            MBED_ASSERT("Unknown Event");
    }
}
