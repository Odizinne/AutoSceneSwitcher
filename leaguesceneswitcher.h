#ifndef LEAGUESCENESWITCHER_H
#define LEAGUESCENESWITCHER_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QString>

class LeagueSceneSwitcher : public QMainWindow
{
    Q_OBJECT

public:
    explicit LeagueSceneSwitcher(QWidget *parent = nullptr);
    ~LeagueSceneSwitcher();

private:
    bool prepareConfig();
    void loadConfig();
    void createTrayIconAndMenu();
    bool isProcessRunning(const QString& processName);
    void setSceneById(const QString &sceneId);
    void setClientScene();
    void setGameScene();
    void checkGamePresence();
    void connectToStreamlabs();
    void onConnected();
    void onTextMessageReceived(QString message);
    void getScenes();

    QSystemTrayIcon *trayIcon;
    QTimer *timer;
    QWebSocket webSocket;
    QString streamlabsToken;
    QString clientSceneName;
    QString gameSceneName;
    QMap<QString, QString> sceneIdMap; // Maps scene names to their IDs
    bool switched;
};

#endif // LEAGUESCENESWITCHER_H
