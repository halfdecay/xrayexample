#ifndef EMITTER_H
#define EMITTER_H

#include <QSerialPort>
#include <QTimer>

class Emitter : public QObject
{
    Q_OBJECT
public:
    explicit Emitter(QObject *parent = 0);
    // Connection Test Function.
    bool isConnected() const;

public slots:
    // Slot for setting parameters.
    void setFeatures(int voltage, int current, int workTime, int coolTime);
    // X-ray inclusion slot.
    void turnOnXRay();
    // X-ray shutdown slot.
    void turnOffXRay();

private:
    enum class Command: quint8; // Announcement of the transfer
    // Connection function.
    void connectToEmitter();
    // The most important function that implements the exchange protocol with the emitter.
    QByteArray writeAndRead(Command com, quint8 y1 = 0, quint8 y2 = 0, quint8 *m = nullptr);
    // Emitter command: Emitter status.
    bool commandS();
    // Emitter command: Set parameters.
    void commandP(quint16 volt, quint16 curr, quint16 workTime, quint16 coolTime);
    // Emitter command: Switch on the emitter.
    void commandN();
    // Emitter command: Switch off the emitter.
    void commandF();

    const quint8 STARTBYTE = 0x40;  // Beginning of the parcel.
    const quint8 DEV = 0;  // Emitter ID.

    // Available commands sent to the radiator. It is sent by the third byte in the parcel.
    enum class Command : quint8
    {
        S = 0x53, // Command request status.
        P = 0x50, // Command for setting parameters.
        N = 0x4e, // Command to turn on the X-ray.
        F = 0x46  // X-ray shutdown command.
    };

    // Emitter messages.
    enum class MessageCommandS : quint8
    {
        OK = 0x00, // All Oк.
        XRAY_ON = 0x01, // Emission included.
        XRAY_STARTING = 0x02, // Emitter output to the mode.
        XRAY_TRAIN = 0x03, // There is training.
        COOLING = 0x04 // Cooling.
    };

    // Emitter errors.
    enum class ErrorCommandS : quint8
    {
        NO_ERROR = 0x00, // Without errors
        XRAY_CONTROL_ERROR = 0x01, // X-ray tube control error.
        MODE_ERROR = 0x02, // Error setting the specified mode.
        VOLTAGE_ERROR = 0x03, // Exceeding the voltage threshold.
        CURRENT_ERROR = 0x04, // Exceed the current threshold.
        PROTECTIVE_BOX_ERROR = 0x05, // The protective box is open.
        LOW_SUPPLY_VOLTAGE = 0x06, // Low supply voltage.
        DISCONNECTION = 0x07, // No confirmation of connection (more than 1 s)
        OVERHEAT = 0x08 // Overheat.
    };

    QSerialPort *m_pSerialPort;
    bool m_isConnected;

    QTimer *m_pTimerCheckConnection;
};

#endif // EMITTER_H
