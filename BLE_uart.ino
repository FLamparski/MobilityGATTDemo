#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <sstream>
#include "ble_service.h"

#define MASK(x) (1 << x)

#define EVT_FLAG_CONNECTED MASK(0)
#define EVT_FLAG_DISCONNECTED MASK(1)
#define EVT_CMD_GET_SAMPLES MASK(2)
EventGroupHandle_t eventFlags = xEventGroupCreate();

// For UART-style service:
/*
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with "NOTIFY"
*/
// #define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
// #define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
// #define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
// #define CHARACTERISTIC_UUID_EXTRA "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"

// For notify-and-read style service:
/*
   The service advertises itself as 19e1b670-3e07-43f2-8056-0c01c019cf78.
   Characteristic at 19e1b671... receives data with WRITE
   Characteristic at 19e1b672... sends data with READ (upon request)
   Characteristic at 19e1b673... informs client there is new data on 19e1b672
*/


AppBLEService bleService;


void setup() {
    Serial.begin(115200);

    bleService.onClientConnected([] () {
        xEventGroupSetBits(eventFlags, EVT_FLAG_CONNECTED);
    });
    bleService.onClientDisconnected([] () {
        xEventGroupSetBits(eventFlags, EVT_FLAG_DISCONNECTED);
    });
    bleService.onDataReceived([] (std::string rxValue) {
        if (rxValue.length() > 0) {
            Serial.println("*********");
            Serial.printf("Length: %d\r\n", rxValue.length());
            Serial.print("Received Value:\r\n");
            Serial.print(rxValue.c_str());
            Serial.println();
            Serial.println("*********");
        }
        if (rxValue.find("APP_GET_SAMPLES") != std::string::npos) {
            xEventGroupSetBits(eventFlags, EVT_CMD_GET_SAMPLES);
        }
    });
    bleService.start("Mobility Mk 1");

    xTaskCreate(taskConnectionHandler, "connection-handler", 3 * 1024, nullptr, tskIDLE_PRIORITY, nullptr);
}

void transmitKeepalive(int timestamp) {
    std::ostringstream buffer;
    buffer << "{\"type\":\"DEVICE_ALIVE\",\"ts\":" << timestamp << "}\r\n";
    bleService.writeData(buffer.str());
}

void transmitSamples(int timestamp) {
    std::ostringstream buffer;
    buffer << "{\"type\":\"DEVICE_SAMPLES\",\"ts\":" << timestamp << "\",\"payload\":[";
    for (int i = 10; i >= 0; i--) {
        buffer << "{\"roundId\":" << i << ",\"device\":\"11:22:33:44:55:66\",\"type\":\"MOBILITY_DATA\",\"distanceDriven\":230.54}";
        if (i > 0) {
            buffer << ",";
        }
    }
    buffer << "]}\r\n";
    bleService.writeData(buffer.str());
}

#define NOT_CONNECTED 0
#define CONNECTED 1
#define KEEPALIVE_PERIOD 10
void taskConnectionHandler(void *params) {
    Serial.println("connection-handler started");
    int state = NOT_CONNECTED;
    int keepaliveTimer = KEEPALIVE_PERIOD;
    int timestampTimer = 0;
    while (true) {
        auto evt = xEventGroupWaitBits(
            eventFlags,
            EVT_FLAG_CONNECTED | EVT_FLAG_DISCONNECTED | EVT_CMD_GET_SAMPLES,
            pdTRUE,
            pdFALSE,
            100
        );
        switch (state) {
            case NOT_CONNECTED:
                if (evt & EVT_FLAG_CONNECTED) {
                    state = CONNECTED;
                    transmitSamples(1234);
                }
                break;
            case CONNECTED:
                if (evt & EVT_FLAG_DISCONNECTED) {
                    state = NOT_CONNECTED;
                    timestampTimer = 0;
                }
                else {
                    keepaliveTimer--;
                    if (keepaliveTimer == 0) {
                        keepaliveTimer = KEEPALIVE_PERIOD;
                        timestampTimer++;
                    }
                    if (evt & EVT_CMD_GET_SAMPLES) {
                        transmitSamples(timestampTimer);
                    }
                }
                break;
        }
    }
}

void loop() {
}
