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

// Currently not used
void transmitKeepalive(int timestamp) {
    std::ostringstream buffer;
    buffer << "{\"type\":\"DEVICE_ALIVE\",\"ts\":" << timestamp << "}\r\n";
    bleService.writeData(buffer.str());
}

// Simulates the device transmitting samples to the app
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
