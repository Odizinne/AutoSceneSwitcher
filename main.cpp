#include "leaguesceneswitcher.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle("fusion");
    LeagueSceneSwitcher w;
    return a.exec();
}
