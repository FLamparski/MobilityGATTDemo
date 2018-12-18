#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <functional>
#include <string>
#include <vector>

#ifndef _BLE_SERVICE_H
#define _BLE_SERVICE_H

class MyBLEServerCallbacks;
class RxCharacteristicCallbacks;

class AppBLEService {
    public:
    void start(std::string deviceName);
    void onClientConnected(std::function<void()> callback);
    void onClientDisconnected(std::function<void()> callback);
    void onDataReceived(std::function<void(std::string)> callback);
    void writeData(std::string value);

    protected:
    friend class MyBLEServerCallbacks;
    friend class RxCharacteristicCallbacks;
    std::function<void()> onClientConnectedCallback;
    std::function<void()> onClientDisconnectedCallback;
    std::function<void(std::string)> onDataReceivedCallback;
    bool hasQueuedMessages();
    std::string popMessage();
    void updateNumMessages();
    void notifyNumMessages();

    private:
    BLEServer* mServer;
    BLECharacteristic* mTxCharacteristic;
    BLECharacteristic* mNfCharacteristic;
    std::vector<std::string> txBuffer;
};

#endif
