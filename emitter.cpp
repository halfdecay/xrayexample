#include "emitter.h"

#include <QDebug>
#include <QThread>

Emitter::Emitter(QObject *parent) : QObject(parent)
{
    // Инициализация последовательного порта.
    m_pSerialPort = new QSerialPort(this);
    m_pSerialPort->setPortName("COM3");
    // Скорость передачи данных. 19200 бит/с.
    m_pSerialPort->setBaudRate(QSerialPort::Baud19200);
    m_pSerialPort->setDataBits(QSerialPort::Data8);
    m_pSerialPort->setParity(QSerialPort::NoParity);
    m_pSerialPort->setStopBits(QSerialPort::OneStop);
    m_pSerialPort->setFlowControl(QSerialPort::NoFlowControl);

    /* In this radiator with the included X-ray, it is necessary, at least once a second,
     send a command to the status request to confirm that the connection is not disconnected.
     Otherwise, the x-ray will be turned off. To do this, create a timer with an interval of 1 second. */
    m_pTimerCheckConnection = new QTimer(this);
    m_pTimerCheckConnection->setInterval(1000);

    /* After a time of 1 s, the status request command is called.
     This is where the lambda function is used, not to create a slot.
     It would be possible to make commandS private slot, but in this case at connect
     through the old form of recording (on SIGNAL, SLOT)
     This slot would be accessible to external objects. */
    connect(m_pTimerCheckConnection, &QTimer::timeout, [this](){
        commandS();
    });

    // We connect the serial port.
    connectToEmitter();
}

bool Emitter::isConnected() const
{
    return m_isConnected;
}

void Emitter::setFeatures(int voltage, int current, int workTime, int coolTime)
{
    if (isConnected())
    {
        commandP(voltage, current, workTime, coolTime);
    }
}

void Emitter::turnOnXRay()
{
    if (isConnected())
    {
        commandN();
    }
}

void Emitter::turnOffXRay()
{
    if (isConnected())
    {
        commandF();
    }
}

void Emitter::connectToEmitter()
{
    if (m_pSerialPort->open(QSerialPort::ReadWrite))
    {
        // We are convinced that the radiator of the X-ray is connected to the serial port.
        m_isConnected = commandS();
        if (m_isConnected)
        {
            qDebug() << "The radiograph of the X-ray is connected.";
        }
        else
        {
            qDebug() << "A different device is connected to the serial port of the radiator of the X-ray!";
        }
    }
    else
    {
        qDebug() << "The serial port is not connected. ";
         m_isConnected = false;
    }
}

QByteArray Emitter::writeAndRead(Command com, quint8 y1, quint8 y2, quint8 *m)
{
    quint16 y = y2 * 256 + y1;

    // Data sent to the serial port.
    QByteArray sentData;
    sentData.resize(6 + y);
    sentData[0] = STARTBYTE;
    sentData[1] = DEV;

    // Checksum = the sum of all bytes, starting with the byte of the command.
    quint8 crc = 0;
    crc += sentData[2] = static_cast<quint8>(com);
    crc += sentData[3] = y1;
    crc += sentData[4] = y2;

    // If there are data bytes, then they are also included in the checksum.
    if (y && m)
    {
        for (int i = 0; i < y; ++i)
        {
            crc += sentData[5 + i] = m[i];
        }
    }

    // The last byte is the checksum.
    sentData[5 + y] = crc;

    // Write to the serial port and wait for up to 100 ms until the recording is done.
    m_pSerialPort->write(sentData);
    m_pSerialPort->waitForBytesWritten(100);

    // We go to sleep, waiting for the microcontroller of the radiator to process the data and answer.
    this->thread()->msleep(50);
    // We read the data from the emitter.
    m_pSerialPort->waitForReadyRead(50);
    return m_pSerialPort->readAll();
}

bool Emitter::commandS() // Command request status.
{
    // We send the emitter.
    QByteArray receivedData = writeAndRead(Command::S);

    /* Must come exactly 8 bytes.
     The answer should be: @ dev com 2 0 s e CRC.
     Otherwise the answer did not come from the radiator. */
    if (receivedData.size() == 8)
    {
        quint8 crc = 0;
        for (int i = 1; i < receivedData.size() - 1; ++i)
        {
            crc += receivedData[i];
        }
        if (crc != static_cast<quint8>(receivedData[receivedData.size() - 1]))
            qDebug() << "CRC does not match!" << crc << static_cast<quint8>(receivedData[receivedData.size() - 1]);

        // If the condition is correct, then our radiographer of the X-ray replied.
        if (receivedData[0] == STARTBYTE &&
            static_cast<Command>(receivedData.at(2)) == Command::S &&
            receivedData[3] == 2 &&
            receivedData[4] == 0)
        {
            MessageCommandS message = static_cast<MessageCommandS>(receivedData.at(5));
            switch (message)
            {
            case MessageCommandS::OK:
                qDebug() << "Everything is fine.";
                break;
            case MessageCommandS::XRAY_ON:
                qDebug() << "Radiation is on.";
                break;
            case MessageCommandS::XRAY_STARTING:
                qDebug() << "Output of radiation to the mode.";
                break;
            case MessageCommandS::XRAY_TRAIN:
                qDebug() << "There is training.";
                break;
            case MessageCommandS::COOLING:
                qDebug() << "Cooling.";
                break;
            }
            ErrorCommandS error = static_cast<ErrorCommandS>(receivedData.at(6));
            switch (error)
            {
            case ErrorCommandS::NO_ERROR:
                qDebug() << "No mistakes";
                break;
            case ErrorCommandS::XRAY_CONTROL_ERROR:
                qDebug() << "X-ray tube control error.";
                break;
            case ErrorCommandS::MODE_ERROR:
                qDebug() << "It is not possible to set the preset mode on the handset.";
                break;
            case ErrorCommandS::VOLTAGE_ERROR:
                qDebug() << "The voltage threshold is exceeded.";
                break;
            case ErrorCommandS::CURRENT_ERROR:
                qDebug() << "The current threshold is exceeded.";
                break;
            case ErrorCommandS::PROTECTIVE_BOX_ERROR:
                qDebug() << "The protective box is open.";
                break;
            case ErrorCommandS::LOW_SUPPLY_VOLTAGE:
                qDebug() << "Low supply voltage.";
                break;
            case ErrorCommandS::DISCONNECTION:
                qDebug() << "Lack of communication with the host (more than 1s).";
                break;
            case ErrorCommandS::OVERHEAT:
                qDebug() << "Overheat.";
                break;
            }
            // Return true, because the answer came from the radiator.
            return true;
        }
    }
    else
    {
        qDebug() << "Error. The answer does not meet expectations.";
    }
    return false;
}

// Command to set parameters.
void Emitter::commandP(quint16 volt, quint16 curr, quint16 workTime, quint16 coolTime)
{
    quint8 m[8];

    // 8 bytes of data, 4 words. The first low byte of the word, the second highest byte of the word.
    m[0] = volt - (volt/256) * 256;
    m[1] = volt/256;
    m[2] = curr - (curr/256) * 256;
    m[3] = curr/256;
    m[4] = workTime - (workTime/256) * 256;
    m[5] = workTime/256;
    m[6] = coolTime - (coolTime/256) * 256;
    m[7] = coolTime/256;
    QByteArray receivedData = writeAndRead(Command::P, 8, 0, m);

    /* Must come exactly 14 bytes.
     The answer should be: @ dev com 8 0 volt curr workTime coolTime CRC.
     volt, curr, workTime, coolTime - words in 2 bytes.
     Otherwise the answer did not come from the radiator. */
    if (receivedData.size() == 14)
    {
        quint8 crc = 0;
        for (int i = 1; i < receivedData.size() - 1; ++i)
        {
            crc += receivedData[i];
        }
        if (crc != static_cast<quint8>(receivedData[receivedData.size() - 1]))
            qDebug() << "CRC does not match!" << crc << static_cast<quint8>(receivedData[receivedData.size() - 1]);

        // The response from the radiator is the current parameter values.
        if (receivedData[0] == STARTBYTE &&
            static_cast<Command>(receivedData.at(2)) == Command::P &&
            receivedData[3] == 8 &&
            receivedData[4] == 0)
        {
            quint16 receivedVolt = receivedData[5] + receivedData[6] * 256;
            quint16 receivedCurr = receivedData[7] + receivedData[8] * 256;
            quint16 receivedWorkTime = receivedData[9] + receivedData[10] * 256;
            quint16 receivedCoolTime = receivedData[11] + receivedData[12] * 256;
            qDebug() << "Данные : " << receivedVolt << receivedCurr << receivedWorkTime << receivedCoolTime;
        }
    }
    else
    {
        qDebug() << "Error. The answer does not meet expectations.";
    }
}

// X-ray shutdown command.
void Emitter::commandN()
{
    QByteArray receivedData = writeAndRead(Command::N);

    /* It should come exactly 6 bytes.
     The answer should be: @ dev com 0 0 CRC.
     Otherwise the answer did not come from the radiator. */
    if (receivedData.size() == 6)
    {
        quint8 crc = 0;
        for (int i = 1; i < receivedData.size() - 1; ++i)
        {
            crc += receivedData[i];
        }
        if (crc != static_cast<quint8>(receivedData[receivedData.size() - 1]))
            qDebug() << "CRC does not match!" << crc << static_cast<quint8>(receivedData[receivedData.size() - 1]);

        // Turn on the connection expiration timer if the X-ray is on.
        if (receivedData[0] == STARTBYTE &&
            static_cast<Command>(receivedData.at(2)) == Command::N &&
            receivedData[3] == 0 &&
            receivedData[4] == 0)
        {
            qDebug() << "X-rays included.";
            m_pTimerCheckConnection->start();
        }
    }
    else
    {
        qDebug() << "Error. The answer does not meet expectations.";
    }
}

void Emitter::commandF() // Turn off the X-ray.
{
    QByteArray receivedData = writeAndRead(Command::F);

    /* It should come exactly 6 bytes.
     The answer should be: @ dev com 0 0 CRC.
     Otherwise the answer did not come from the radiator. */
    if (receivedData.size() == 6)
    {
        quint8 crc = 0;
        for (int i = 1; i < receivedData.size() - 1; ++i)
        {
            crc += receivedData[i];
        }
        if (crc != static_cast<quint8>(receivedData[receivedData.size() - 1]))
            qDebug() << "CRC does not match!" << crc << static_cast<quint8>(receivedData[receivedData.size() - 1]);

        // Turn off the connection confirmation timer.
        if (receivedData[0] == STARTBYTE &&
            static_cast<Command>(receivedData.at(2)) == Command::F &&
            receivedData[3] == 0 &&
            receivedData[4] == 0)
        {
            qDebug() << "The X-ray is turned off.";
            m_pTimerCheckConnection->stop();
        }
    }
    else
    {
        qDebug() << "Error. The answer does not meet expectations.";
    }
}
