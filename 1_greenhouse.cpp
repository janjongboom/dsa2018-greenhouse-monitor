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
#include "standby.h"

#define     STANDBY_TIME_S          5 * 60


static uint32_t DEVADDR_1 = 0x26011055;
static uint8_t NWKSKEY_1[] = { 0xbe, 0x51, 0x5b, 0x4e, 0x57, 0xfe, 0x71, 0x2c, 0x81, 0x5c, 0x49, 0xb6, 0xfc, 0x4c, 0xac, 0x25 };
static uint8_t APPSKEY_1[] = { 0x27, 0xcb, 0xc4, 0xd0, 0x50, 0xd5, 0x6f, 0x88, 0xb5, 0x17, 0xd3, 0xfe, 0x1c, 0x0f, 0x91, 0xdf };

static uint32_t DEVADDR_10 = 0x26011c27;
static uint8_t NWKSKEY_10[] = { 0x9b, 0x2d, 0xa7, 0x5c, 0x7c, 0x10, 0x06, 0x3a, 0x4f, 0xe7, 0xc2, 0xae, 0x22, 0xd7, 0xc0, 0xb0 };
static uint8_t APPSKEY_10[] = { 0x41, 0xa5, 0x43, 0xd3, 0x3f, 0xc9, 0x13, 0xe9, 0x0c, 0xbd, 0x20, 0x09, 0x15, 0x7c, 0x70, 0x2a };

static uint32_t DEVADDR_2 = 0x2601164b;
static uint8_t NWKSKEY_2[] = { 0xe1, 0x9b, 0x11, 0x64, 0x92, 0x90, 0xa7, 0xf8, 0x1a, 0x12, 0x1c, 0x32, 0xab, 0x7e, 0x1d, 0x01 };
static uint8_t APPSKEY_2[] = { 0x53, 0x16, 0x4a, 0x8f, 0xf1, 0xcd, 0xea, 0x50, 0x0b, 0xa1, 0xcd, 0x8e, 0x5e, 0x4e, 0xcf, 0xfa };

static uint32_t DEVADDR_3 = 0x2601123c;
static uint8_t NWKSKEY_3[] = { 0x56, 0xe8, 0x05, 0x77, 0x19, 0x0c, 0x4d, 0x7a, 0x5a, 0x9c, 0x69, 0xcc, 0x54, 0xfa, 0x6b, 0x1b };
static uint8_t APPSKEY_3[] = { 0xc7, 0x51, 0xaa, 0x53, 0xbe, 0xbe, 0xdf, 0x93, 0x23, 0x93, 0x95, 0xf2, 0xaa, 0xa5, 0x32, 0x14 };

static uint32_t DEVADDR_4 = 0x26011337;
static uint8_t NWKSKEY_4[] = { 0xbc, 0x84, 0xba, 0xbd, 0xc4, 0x99, 0x8c, 0x1f, 0xaa, 0x88, 0xdc, 0x68, 0x8b, 0x63, 0xa9, 0xf0 };
static uint8_t APPSKEY_4[] = { 0xb4, 0x0b, 0xd6, 0xbb, 0xa1, 0x94, 0xcf, 0x6a, 0x26, 0x17, 0xf3, 0xdf, 0xc7, 0x5a, 0x79, 0x3c };

static uint32_t DEVADDR_5 = 0x260117b4;
static uint8_t NWKSKEY_5[] = { 0x25, 0x94, 0x10, 0xeb, 0x2a, 0xfe, 0xc4, 0xa7, 0x3b, 0x13, 0xd7, 0x1b, 0xce, 0x76, 0x88, 0xbb };
static uint8_t APPSKEY_5[] = { 0x4c, 0x23, 0x46, 0x16, 0x2f, 0x74, 0x59, 0x42, 0xf0, 0x68, 0x1a, 0x51, 0x5b, 0xe3, 0xbe, 0xb5 };

static uint32_t DEVADDR_6 = 0x26011bc9;
static uint8_t NWKSKEY_6[] = { 0x27, 0x2d, 0x51, 0x52, 0xba, 0x5b, 0x83, 0x7b, 0xd8, 0x22, 0xfa, 0x23, 0x02, 0x21, 0x4e, 0x0c };
static uint8_t APPSKEY_6[] = { 0xc1, 0xae, 0xb4, 0xb7, 0x94, 0x8a, 0x27, 0xf8, 0x40, 0xd8, 0x55, 0xe0, 0x0c, 0x2e, 0xdc, 0xe6 };

static uint32_t DEVADDR_7 = 0x26011389;
static uint8_t NWKSKEY_7[] = { 0xc4, 0x67, 0x5b, 0x19, 0x65, 0xd1, 0xb5, 0xab, 0xb0, 0x7a, 0x13, 0x88, 0x20, 0x71, 0xc5, 0xc0 };
static uint8_t APPSKEY_7[] = { 0xfd, 0xe5, 0x67, 0x0e, 0x1d, 0x56, 0x7d, 0x9b, 0xa2, 0xae, 0x2a, 0x90, 0x1f, 0x11, 0x26, 0xd7 };

static uint32_t DEVADDR_8 = 0x26011003;
static uint8_t NWKSKEY_8[] = { 0xc0, 0x50, 0x66, 0x53, 0x18, 0x15, 0x64, 0x66, 0x3f, 0x84, 0x99, 0x2b, 0x45, 0x60, 0x47, 0x22 };
static uint8_t APPSKEY_8[] = { 0x64, 0x6c, 0xf0, 0xa4, 0x9c, 0x6f, 0x30, 0xf4, 0x0c, 0x80, 0x3e, 0x4a, 0x50, 0xc3, 0x77, 0x53 };

static uint32_t DEVADDR_9 = 0x26011d1c;
static uint8_t NWKSKEY_9[] = { 0x52, 0xc1, 0xb1, 0x66, 0xae, 0xa8, 0xc1, 0xc5, 0x49, 0x3f, 0x69, 0xc6, 0x06, 0xbf, 0x5b, 0xf8 };
static uint8_t APPSKEY_9[] = { 0x03, 0xd8, 0xae, 0x6b, 0xf5, 0x79, 0x30, 0x4c, 0x07, 0x22, 0xb4, 0x33, 0x30, 0x29, 0x83, 0xef };



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

    // for some reason send() ret\urns -1... I cannot find out why, the stack returns the right number. I feel that this is some weird Emscripten quirk
    if (retcode < 0) {
        retcode == LORAWAN_STATUS_WOULD_BLOCK ? printf("send - duty cycle violation\n")
                : printf("send() - Error code %d\n", retcode);

        standby(STANDBY_TIME_S);
    }

    printf("%d bytes scheduled for transmission\n", retcode);
}

int main() {
    set_time(0);

    printf("=========================================\n");
    printf("      DSA 2018 Green House Monitor       \n");
    printf("=========================================\n");

    printf("Sending every %d seconds\n", STANDBY_TIME_S);

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

    lorawan.set_datarate(0); // SF12BW125

    lorawan_connect_t connect_params;
    connect_params.connect_type = LORAWAN_CONNECTION_ABP;

    connect_params.connection_u.abp.dev_addr = DEVADDR_1;
    connect_params.connection_u.abp.nwk_skey = NWKSKEY_1;
    connect_params.connection_u.abp.app_skey = APPSKEY_1;

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
            standby(STANDBY_TIME_S);
            break;
        case TX_TIMEOUT:
        case TX_ERROR:
        case TX_CRYPTO_ERROR:
        case TX_SCHEDULING_ERROR:
            printf("Transmission Error - EventCode = %d\n", event);
            standby(STANDBY_TIME_S);
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
