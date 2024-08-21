#ifndef LEAGUESCENESWITCHER_H
#define LEAGUESCENESWITCHER_H

#include <QMainWindow>
#include <QSystemTrayIcon>

class LeagueSceneSwitcher : public QMainWindow
{
    Q_OBJECT

public:
    LeagueSceneSwitcher(QWidget *parent = nullptr);
    ~LeagueSceneSwitcher();

private slots:
    void checkGamePresence();

private:
    QSystemTrayIcon *trayIcon;
    QTimer *timer;
    bool switched;
    void createTrayIconAndMenu();
    bool isProcessRunning(const QString& processName);
    void setClientScene();
    void setGameScene();
};
#endif // LEAGUESCENESWITCHER_H
