#include "autosceneswitcher.h"
#include <QApplication>
#include <QSharedMemory>
#include <QLocale>
#include <QString>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QSharedMemory sharedMemory("AutoSceneSwitcherVitiIsGoodMan");

    if (sharedMemory.attach()) {
        return 0;
    }

    if (!sharedMemory.create(1)) {
        qDebug() << "Unable to create shared memory segment.";
        return 1;
    }

    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    QLocale locale;
    QString languageCode = locale.name().section('_', 0, 0);
    QTranslator translator;
    if (translator.load(":/translations/Tr/AutoSceneSwitcher_" + languageCode + ".qm")) {
        a.installTranslator(&translator);
    }

    AutoSceneSwitcher w;
    QObject::connect(&a, &QApplication::aboutToQuit, [&sharedMemory]() { sharedMemory.detach(); });
    return a.exec();
}
