#include "leaguesceneswitcher.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
    LeagueSceneSwitcher w;
    return a.exec();
}
