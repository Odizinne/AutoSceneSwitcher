#include "autosceneswitcher.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
    AutoSceneSwitcher w;
    return a.exec();
}
