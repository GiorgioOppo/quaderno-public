#include "blecentral.h"

#include <QTimer>

#import <CoreBluetooth/CoreBluetooth.h>

namespace {
constexpr int connectionTimeoutMs = 30000;
constexpr qsizetype chunkLimit = 244;
}

@interface QuadernoBleCentralDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@property(nonatomic, assign) BleCentralPrivate *transport;
@end

class BleCentralPrivate
{
public:
    explicit BleCentralPrivate(BleCentral &owner) : m_owner(owner)
    {
        m_deadline.setSingleShot(true);
        QObject::connect(&m_deadline, &QTimer::timeout, &m_owner, [this] {
            fail(BleCentral::tr("Paper Pro non trovato o collegamento Bluetooth scaduto."));
        });
    }

    ~BleCentralPrivate() { stop(); }

    void start()
    {
        if (m_manager)
            return;

        m_serviceUuid = [CBUUID UUIDWithString:@"31cb599d-0664-4b7b-a1c8-d854da913c40"];
        m_writeUuid = [CBUUID UUIDWithString:@"31cb599d-0664-4b7b-a1c8-d854da913c41"];
        m_receiveUuid = [CBUUID UUIDWithString:@"31cb599d-0664-4b7b-a1c8-d854da913c42"];
        m_delegate = [[QuadernoBleCentralDelegate alloc] init];
        m_delegate.transport = this;
        m_manager = [[CBCentralManager alloc] initWithDelegate:m_delegate
                                                       queue:dispatch_get_main_queue()
                                                     options:@{ CBCentralManagerOptionShowPowerAlertKey: @NO }];
        m_deadline.start(connectionTimeoutMs);
        emit m_owner.statusChanged(BleCentral::tr("Attendo che il Bluetooth del Mac sia disponibile…"));
    }

    void stop()
    {
        m_deadline.stop();
        m_delegate.transport = nullptr;
        m_manager.delegate = nil;
        m_peripheral.delegate = nil;
        if (m_manager.state == CBManagerStatePoweredOn) {
            if (m_manager.isScanning)
                [m_manager stopScan];
            if (m_peripheral && m_peripheral.state != CBPeripheralStateDisconnected)
                [m_manager cancelPeripheralConnection:m_peripheral];
        }
        m_receiveCharacteristic = nil;
        m_writeCharacteristic = nil;
        m_peripheral = nil;
        m_manager = nil;
        m_delegate = nil;
        m_ready = false;
        m_writePending = false;
    }

    qsizetype maximumChunkSize() const
    {
        if (!m_ready)
            return 0;
        return qMin(chunkLimit, qsizetype([m_peripheral maximumWriteValueLengthForType:CBCharacteristicWriteWithResponse]));
    }

    bool writeChunk(const QByteArray &chunk)
    {
        if (m_writePending || chunk.isEmpty() || chunk.size() > maximumChunkSize())
            return false;
        m_writePending = true;
        NSData *value = [NSData dataWithBytes:chunk.constData() length:NSUInteger(chunk.size())];
        [m_peripheral writeValue:value forCharacteristic:m_writeCharacteristic type:CBCharacteristicWriteWithResponse];
        return true;
    }

    void updateState(CBCentralManager *manager)
    {
        if (manager != m_manager)
            return;
        switch (manager.state) {
        case CBManagerStateUnknown:
            return;
        case CBManagerStateResetting:
            fail(BleCentral::tr("Il Bluetooth del Mac si sta riavviando. Riprova tra poco."));
            return;
        case CBManagerStateUnsupported:
            fail(BleCentral::tr("Il Mac non supporta Bluetooth Low Energy."));
            return;
        case CBManagerStateUnauthorized:
            fail(BleCentral::tr("Consenti a Quaderno di usare il Bluetooth in Impostazioni di Sistema → Privacy e sicurezza."));
            return;
        case CBManagerStatePoweredOff:
            fail(BleCentral::tr("Attiva il Bluetooth sul Mac e riprova."));
            return;
        case CBManagerStatePoweredOn:
            if (!m_peripheral && !manager.isScanning) {
                [manager scanForPeripheralsWithServices:@[m_serviceUuid] options:nil];
                emit m_owner.statusChanged(BleCentral::tr("Cerco Quaderno sul Paper Pro…"));
            }
            return;
        }
    }

    void discover(CBCentralManager *manager, CBPeripheral *peripheral)
    {
        if (manager != m_manager || m_peripheral)
            return;
        m_peripheral = peripheral;
        m_peripheral.delegate = m_delegate;
        [manager stopScan];
        [manager connectPeripheral:peripheral options:nil];
        emit m_owner.statusChanged(BleCentral::tr("Collegamento Bluetooth al Paper Pro…"));
    }

    void connect(CBCentralManager *manager, CBPeripheral *peripheral)
    {
        if (manager == m_manager && peripheral == m_peripheral)
            [peripheral discoverServices:@[m_serviceUuid]];
    }

    void disconnect(CBCentralManager *manager, CBPeripheral *peripheral)
    {
        if (manager == m_manager && peripheral == m_peripheral)
            fail(m_ready ? BleCentral::tr("Collegamento Bluetooth al Paper Pro interrotto.")
                         : BleCentral::tr("Impossibile stabilire il collegamento Bluetooth con il Paper Pro."));
    }

    void discoverServices(CBPeripheral *peripheral, NSError *error)
    {
        if (peripheral != m_peripheral)
            return;
        if (!error) {
            for (CBService *service in peripheral.services) {
                if ([service.UUID isEqual:m_serviceUuid]) {
                    [peripheral discoverCharacteristics:@[m_writeUuid, m_receiveUuid] forService:service];
                    return;
                }
            }
        }
        fail(BleCentral::tr("Il servizio Bluetooth di Quaderno non è disponibile sul Paper Pro."));
    }

    void discoverCharacteristics(CBPeripheral *peripheral, CBService *service, NSError *error)
    {
        if (peripheral != m_peripheral || ![service.UUID isEqual:m_serviceUuid])
            return;
        if (!error) {
            for (CBCharacteristic *characteristic in service.characteristics) {
                if ([characteristic.UUID isEqual:m_writeUuid] && (characteristic.properties & CBCharacteristicPropertyWrite))
                    m_writeCharacteristic = characteristic;
                if ([characteristic.UUID isEqual:m_receiveUuid]
                    && (characteristic.properties & (CBCharacteristicPropertyNotify | CBCharacteristicPropertyIndicate)))
                    m_receiveCharacteristic = characteristic;
            }
            if (m_writeCharacteristic && m_receiveCharacteristic) {
                [peripheral setNotifyValue:YES forCharacteristic:m_receiveCharacteristic];
                return;
            }
        }
        fail(BleCentral::tr("Il servizio Bluetooth del Paper Pro è incompatibile con questa versione di Quaderno."));
    }

    void updateNotification(CBPeripheral *peripheral, CBCharacteristic *characteristic, NSError *error)
    {
        if (peripheral != m_peripheral || characteristic != m_receiveCharacteristic)
            return;
        if (error || !characteristic.isNotifying) {
            fail(BleCentral::tr("Impossibile ricevere dati Bluetooth dal Paper Pro."));
            return;
        }
        if (m_ready)
            return;
        m_deadline.stop();
        m_ready = true;
        if (maximumChunkSize() == 0) {
            fail(BleCentral::tr("Il Paper Pro non accetta dati sul collegamento Bluetooth."));
            return;
        }
        emit m_owner.ready();
    }

    void receive(CBPeripheral *peripheral, CBCharacteristic *characteristic, NSError *error)
    {
        if (!m_ready || peripheral != m_peripheral || characteristic != m_receiveCharacteristic)
            return;
        if (error) {
            fail(BleCentral::tr("Errore durante la ricezione di dati Bluetooth dal Paper Pro."));
            return;
        }
        NSData *value = characteristic.value;
        if (value.length)
            emit m_owner.received(QByteArray(static_cast<const char *>(value.bytes), qsizetype(value.length)));
    }

    void writeCompleted(CBPeripheral *peripheral, CBCharacteristic *characteristic, NSError *error)
    {
        if (!m_writePending || peripheral != m_peripheral || characteristic != m_writeCharacteristic)
            return;
        m_writePending = false;
        if (error) {
            fail(BleCentral::tr("Errore durante l’invio di dati Bluetooth al Paper Pro."));
            return;
        }
        emit m_owner.writable();
    }

    void invalidateServices(CBPeripheral *peripheral, NSArray<CBService *> *services)
    {
        if (peripheral != m_peripheral)
            return;
        for (CBService *service in services) {
            if ([service.UUID isEqual:m_serviceUuid]) {
                fail(BleCentral::tr("Il servizio Bluetooth di Quaderno sul Paper Pro è cambiato."));
                return;
            }
        }
    }

private:
    void fail(const QString &message)
    {
        stop();
        emit m_owner.disconnected(message);
    }

    BleCentral &m_owner;
    QTimer m_deadline;
    QuadernoBleCentralDelegate *m_delegate = nil;
    CBCentralManager *m_manager = nil;
    CBPeripheral *m_peripheral = nil;
    CBCharacteristic *m_receiveCharacteristic = nil;
    CBCharacteristic *m_writeCharacteristic = nil;
    CBUUID *m_serviceUuid = nil;
    CBUUID *m_receiveUuid = nil;
    CBUUID *m_writeUuid = nil;
    bool m_ready = false;
    bool m_writePending = false;
};

@implementation QuadernoBleCentralDelegate
- (void)centralManagerDidUpdateState:(CBCentralManager *)manager
{
    if (auto *transport = self.transport)
        transport->updateState(manager);
}

- (void)centralManager:(CBCentralManager *)manager didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary<NSString *, id> *)advertisement RSSI:(NSNumber *)rssi
{
    if (auto *transport = self.transport)
        transport->discover(manager, peripheral);
}

- (void)centralManager:(CBCentralManager *)manager didConnectPeripheral:(CBPeripheral *)peripheral
{
    if (auto *transport = self.transport)
        transport->connect(manager, peripheral);
}

- (void)centralManager:(CBCentralManager *)manager didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->disconnect(manager, peripheral);
}

- (void)centralManager:(CBCentralManager *)manager didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->disconnect(manager, peripheral);
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->discoverServices(peripheral, error);
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->discoverCharacteristics(peripheral, service, error);
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic
              error:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->updateNotification(peripheral, characteristic, error);
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->receive(peripheral, characteristic, error);
}

- (void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error
{
    if (auto *transport = self.transport)
        transport->writeCompleted(peripheral, characteristic, error);
}

- (void)peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)services
{
    if (auto *transport = self.transport)
        transport->invalidateServices(peripheral, services);
}
@end

BleCentral::BleCentral(QObject *parent)
    : QObject(parent), m_private(std::make_unique<BleCentralPrivate>(*this))
{
}

BleCentral::~BleCentral() = default;

void BleCentral::start()
{
    m_private->start();
}

void BleCentral::stop()
{
    m_private->stop();
}

qsizetype BleCentral::maximumChunkSize() const
{
    return m_private->maximumChunkSize();
}

bool BleCentral::writeChunk(const QByteArray &chunk)
{
    return m_private->writeChunk(chunk);
}
