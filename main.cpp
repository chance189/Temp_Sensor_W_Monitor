#include "temperature_data_display.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Temperature_Data_Display w;
    w.show();

    return a.exec();
}
