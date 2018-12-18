#include <Arduino.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <string>
#include <memory>
#include "ble_service.h"

// Custom service UUID for socket-like semantics. Keep in mind the MTU
// is about 600 bytes; this service will not send more than 512 in one
// message.
#define SERVICE_UUID           "19e1b670-3e07-43f2-8056-0c01c019cf78"

// Characteristic for receiving data from the app.
#define CHARACTERISTIC_UUID_RX "19e1b671-3e01-43f2-8056-0c01c019cf78"

// Characteristic for sending data to the app.
#define CHARACTERISTIC_UUID_TX "19e1b672-3e07-43f2-8056-0c01c019cf78"

// Characteristic notifying the app of how many messages are pending.
// The app can at any point READ this characteristic to get the
// length of the message queue. The app can ask the device to NOTIFY
// it, in which case the device will NOTIFY the app if there are NEW
// messages added to the queue.
#define CHARACTERISTIC_UUID_NF "19e1b673-3e07-43f2-8056-0c01c019cf78"

class MyBLEServerCallbacks : public BLEServerCallbacks {
    private:
    AppBLEService *mService;

    public:
    MyBLEServerCallbacks(AppBLEService *service) : mService(service) {}
    
    void onConnect(BLEServer *server) {
        if (mService->onClientConnectedCallback != nullptr) {
            mService->onClientConnectedCallback();
        }
    }

    void onDisconnect(BLEServer *server) {
        server->startAdvertising();
        if (mService->onClientDisconnectedCallback != nullptr) {
            mService->onClientDisconnectedCallback();
        }
    }
};

#define ATT_MTU 512
class RxCharacteristicCallbacks : public BLECharacteristicCallbacks {
    private:
    AppBLEService *mService;
    std::string curMsg;
    int pos;

    public:
    RxCharacteristicCallbacks(AppBLEService *service) :
        mService(service),
        curMsg(""),
        pos(0) {}

    void onWrite(BLECharacteristic* pCharacteristic) {
        // TODO: Buffer input until a terminal \r\n is found.
        if (mService->onDataReceivedCallback != nullptr) {
            mService->onDataReceivedCallback(pCharacteristic->getValue());
        }
    }

    void onRead(BLECharacteristic *pCharacteristic) {
        // This is called by the app. On each invocation it will return
        // the next chunk of the current message. If the current message has been
        // fully transmitted, moves on to the next one. If the messages are
        // exhausted, sends empty string.
        if (curMsg == "") {
            if (mService->hasQueuedMessages()) {
                curMsg = mService->popMessage();
                pos = 0;
                // Note, this could technically notify too but notifying
                // waits on a semaphore that's already taken by this method,
                // causing a deadlock.
                // TODO: Set an event flag so that the notification can be sent
                // on the next tick. (Do we want this?)
                mService->updateNumMessages();
            }
            else {
                pCharacteristic->setValue("");
                return;
            }
        }

        auto chunk = curMsg.substr(pos, ATT_MTU);
        if (chunk.size() < ATT_MTU) {
            curMsg = "";
        }
        else {
            pos += ATT_MTU;
        }

        pCharacteristic->setValue(chunk);
    }
};

void AppBLEService::start(std::string deviceName) {
    // Create the BLE Device
    BLEDevice::init(deviceName);

    // Create the BLE Server
    mServer = BLEDevice::createServer();
    mServer->setCallbacks(new MyBLEServerCallbacks(this));

    // Create the BLE Service
    BLEService *pService = mServer->createService(SERVICE_UUID);

    // Create characteristics for sending data to the client
    mTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, 
                                                       BLECharacteristic::PROPERTY_READ);
    mNfCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_NF,
                                                       BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    mNfCharacteristic->addDescriptor(new BLE2902());

    // Create characteristic for receiving data from the client
    BLECharacteristic *rxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

    auto characteristicCallbacks = new RxCharacteristicCallbacks(this);
    rxCharacteristic->setCallbacks(characteristicCallbacks);
    mTxCharacteristic->setCallbacks(characteristicCallbacks);

    // Start the service
    pService->start();

    // Start advertising
    auto advertisement = mServer->getAdvertising();
    auto advData = BLEAdvertisementData();
    advData.setCompleteServices(BLEUUID(SERVICE_UUID));
    advData.setName(deviceName);
    advertisement->setScanResponseData(advData);
    advertisement->start();
}

void AppBLEService::onClientConnected(std::function<void()> callback) {
    onClientConnectedCallback = callback;
}

void AppBLEService::onClientDisconnected(std::function<void()> callback) {
    onClientDisconnectedCallback = callback;
}

void AppBLEService::onDataReceived(std::function<void(std::string)> callback) {
    onDataReceivedCallback = callback;
}

#define MAX_MESSAGES 10
void AppBLEService::writeData(std::string data) {
    txBuffer.push_back(data);
    while (txBuffer.size() > MAX_MESSAGES) {
        txBuffer.erase(txBuffer.begin());
    }
    notifyNumMessages();
}

void AppBLEService::updateNumMessages() {
    int num = txBuffer.size();
    mNfCharacteristic->setValue(num);
}

void AppBLEService::notifyNumMessages() {
    updateNumMessages();
    mNfCharacteristic->notify();
}

bool AppBLEService::hasQueuedMessages() {
    return txBuffer.size() > 0;
}

std::string AppBLEService::popMessage() {
    if (!hasQueuedMessages()) {
        return "";
    }

    auto msg = txBuffer[0];
    txBuffer.erase(txBuffer.begin());
    return msg;
}