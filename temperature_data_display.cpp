#include "temperature_data_display.h"
#include "ui_temperature_data_display.h"

Temperature_Data_Display::Temperature_Data_Display(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Temperature_Data_Display), port_Settings(new SettingsDialog), port(new QSerialPort), chart(new QChart)
{
    ui->setupUi(this);

    connect(ui->actionConnect, SIGNAL(triggered()), this, SLOT(openSerialPort()));
    connect(ui->actionDisconnect, SIGNAL(triggered()), this, SLOT(closeSerialPort()));
    connect(ui->actionPort_Settings, &QAction::triggered, port_Settings, &SettingsDialog::show);
    connect(port, &QSerialPort::readyRead, this, &Temperature_Data_Display::grabData);
    startTime.setMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch());
    series = new QLineSeries();
    series->append(QDateTime::currentMSecsSinceEpoch(), 0);
    series->setName("Temperature");
    x_Axis = new QDateTimeAxis();
    x_Axis->setFormat("h:mm:ss");
    x_Axis->setLabelsAngle(70);
    x_Axis->setTitleText("TimeStamp");
    x_Axis->setRange(startTime, QDateTime::currentDateTime().addSecs(1000));
    x_Axis->setTickCount(10);
    y_Axis = new QValueAxis;
    y_Axis->setTitleText("Temp in C");
    y_Axis->setRange(0, 100);
    chart->setTitle("Temperature Vs Time");
    //chart->addAxis(x_Axis,Qt::AlignBottom);
    //chart->addAxis(y_Axis,Qt::AlignLeft);
    //chart->addSeries(series);
    //series->attachAxis(x_Axis);
    //series->attachAxis(y_Axis);
    chart->addSeries(series);
    //chartView = new QChartView(chart);
    //chartView->setParent(ui->graphFrame);
    ui->graphView->setChart(chart);
    ui->graphView->chart()->setAxisX(x_Axis, series);
    ui->graphView->chart()->setAxisY(y_Axis, series);
}

Temperature_Data_Display::~Temperature_Data_Display()
{
    delete ui;
}

void Temperature_Data_Display::grabData()
{
    char data[2];
    uint16_t numberValue;
    if(port->bytesAvailable() == 2 )
    {
        port->read(data, 2);
        numberValue = (data[0] << 8) | data[1]; //Don't care
        //Here we push it to our graph
        qDebug() << "The value we got is " << numberValue/128;
        series->append(QDateTime::currentMSecsSinceEpoch(), numberValue/128);
        //chart->removeSeries(series);
        //chart->addSeries(series);
        //ui->graphView->chart()->setAxisX(x_Axis, series);
        //ui->graphView->chart()->setAxisY(y_Axis, series);
        x_Axis->setRange(startTime, QDateTime::currentDateTime().addSecs(1000));
        //chartView->repaint();
    }
    else
        port->readAll(); //Clear all data
}

void Temperature_Data_Display::openSerialPort()
{
    //commThread->connectPort();

    const SettingsDialog::Settings p = port_Settings->settings();
    port->setPortName(p.name);
    port->setBaudRate(p.baudRate);
    port->setDataBits(p.dataBits);
    port->setParity(p.parity);
    port->setStopBits(p.stopBits);
    port->setFlowControl(p.flowControl);
    if (port->open(QIODevice::ReadWrite)) {
        QMessageBox box;
        box.setText(tr("Connected to %1 : %2, %3, %4, %5, %6")
                          .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                          .arg(p.stringParity).arg(p.stringStopBits).arg(p.stringFlowControl));
        box.exec();
        ui->status->setText((tr("Connected to %1 : %2, %3, %4, %5, %6")
                                  .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                                  .arg(p.stringParity).arg(p.stringStopBits).arg(p.stringFlowControl)));
        port->clear();
    } else {
        QMessageBox::critical(this, tr("Error"), port->errorString());
    }
}

void Temperature_Data_Display::closeSerialPort()
{
    //commThread->disconnectPort();

    if (port->isOpen())
        port->close();
    QMessageBox box;
    box.setText(tr("Disconnected"));
    box.exec();
}
