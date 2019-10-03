#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>

#include "emitter.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

signals:
    // This signal will be sent by pressing the "Set" button to transfer the parameters to the emitter.
    void setFeaturesRequested(int voltage, int current, int workTime, int coolTime);

private:
    Ui::MainWindow *ui;

    // Emitter and a separate stream in which it will work.
    QThread *m_pThread;
    Emitter *m_pEmitter;
};

#endif // MAINWINDOW_H
