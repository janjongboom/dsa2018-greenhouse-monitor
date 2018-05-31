#include "select_program.h"

#if PROGRAM == GREENHOUSE_MONITOR

#include "mbed.h"
#include "mbed_trace.h"
#include "mbed_events.h"
#include "LoRaWANInterface.h"
#include "SX1276_LoRaRadio.h"
#include "CayenneLPP.h"
#include "lora_radio_helper.h"
#include "DHT.h"

#define SEND_INTERVAL           20


static uint32_t DEV_ADDR   =      0x0;
static uint8_t NWK_S_KEY[] =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
static uint8_t APP_S_KEY[] =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

// The port we're sending and receiving on
#define MBED_CONF_LORA_APP_PORT     15

// Peripherals
static DHT dht(D7, SEN51035P);          // Temperature sensor
static AnalogIn moist(A2);              // Moisture sensor

// EventQueue is required to dispatch events around
static EventQueue ev_queue;

// Constructing Mbed LoRaWANInterface and passing it down the radio object.
static LoRaWANInterface lorawan(radio);

// Application specific callbacks
static lorawan_app_callbacks_t callbacks;

// LoRaWAN stack event handler
static void lora_event_handler(lorawan_event_t event);

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
            wait_ms(3000);
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

    uint8_t moisture_value = static_cast<uint8_t>(moist.read() * 255.0f);

    printf("Moist=%u\n", moisture_value);

    payload.addAnalogInput(4, static_cast<float>(moisture_value));

    printf("Sending %d bytes\n", payload.getSize());

    int16_t retcode = lorawan.send(MBED_CONF_LORA_APP_PORT, payload.getBuffer(), payload.getSize(), MSG_UNCONFIRMED_FLAG);

    // schedule next message
    ev_queue.call_in(SEND_INTERVAL * 1000, callback(&send_message));

    // for some reason send() ret\urns -1... I cannot find out why, the stack returns the right number. I feel that this is some weird Emscripten quirk
    if (retcode < 0) {
        retcode == LORAWAN_STATUS_WOULD_BLOCK ? printf("send - duty cycle violation\n")
                : printf("send() - Error code %d\n", retcode);
        return;
    }

    printf("%d bytes scheduled for transmission\n", retcode);
}

int main() {
    if (DEV_ADDR == 0x0) {
        printf("Set your LoRaWAN credentials first!\n");
        return -1;
    }

    printf("=========================================\n");
    printf("      DSA 2018 Green House Monitor       \n");
    printf("      Device: 0x%08x                         \n", DEV_ADDR);
    printf("=========================================\n");

    printf("Sending every %d seconds\n", SEND_INTERVAL);

    // Enable trace output for this demo, so we can see what the LoRaWAN stack does
    mbed_trace_init();

    if (lorawan.initialize(&ev_queue) != LORAWAN_STATUS_OK) {
        printf("LoRa initialization failed!\n");
        return -1;
    }

    // prepare application callbacks
    callbacks.events = mbed::callback(lora_event_handler);
    lorawan.add_app_callbacks(&callbacks);

    // Disable adaptive data rating
    if (lorawan.disable_adaptive_datarate() != LORAWAN_STATUS_OK) {
        printf("disable_adaptive_datarate failed!\n");
        return -1;
    }

    lorawan.set_datarate(2); // SF10BW125

    lorawan_connect_t connect_params;
    connect_params.connect_type = LORAWAN_CONNECTION_ABP;

    connect_params.connection_u.abp.dev_addr = DEV_ADDR;
    connect_params.connection_u.abp.nwk_skey = NWK_S_KEY;
    connect_params.connection_u.abp.app_skey = APP_S_KEY;

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
            ev_queue.call_in(1000, &send_message);
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

#endif
