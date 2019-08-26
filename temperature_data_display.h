/*
 * Author: Chance Reimer
 * Purpose: To use QSerialPort to read data from the FPGA board every second and display the results on a graph
 * Grabbed good info from teh Hands-On GUI Programming with C++ book, pg 97 starting
 * */

#ifndef TEMPERATURE_DATA_DISPLAY_H
#define TEMPERATURE_DATA_DISPLAY_H

#include <QMainWindow>
#include <QtCharts>
#include <QChartView>
#include <QSerialPort>

//Adding file from preexisting files on local directory
#include "settingsdialog.h" //Created by QT

using namespace QtCharts;
namespace Ui {
class Temperature_Data_Display;
}

class Temperature_Data_Display : public QMainWindow
{
    Q_OBJECT

public:
    explicit Temperature_Data_Display(QWidget *parent = nullptr);
    ~Temperature_Data_Display();

public slots:
    void openSerialPort();
    void closeSerialPort();
    void grabData();

signals:
    void sendData(qreal);

private:
    Ui::Temperature_Data_Display *ui;
    SettingsDialog* port_Settings;
    QSerialPort* port;
    QChart* chart;
    QChartView *chartView;
    QDateTimeAxis* x_Axis;
    QValueAxis* y_Axis;
    QLineSeries* series;
    QDateTime startTime;
};

#endif // TEMPERATURE_DATA_DISPLAY_H
