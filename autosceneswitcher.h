#ifndef AUTOSCENESWITCHER_H
#define AUTOSCENESWITCHER_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QString>

namespace Ui {
class AutoSceneSwitcher;
}

class AutoSceneSwitcher : public QMainWindow
{
    Q_OBJECT

public:
    explicit AutoSceneSwitcher(QWidget *parent = nullptr);
    ~AutoSceneSwitcher();

private slots:
    void onStartupCheckBoxStateChanged();
    void toggleTokenView();
    void onPauseButtonClicked();
    void onRefreshScenesButtonClicked();

private:
    Ui::AutoSceneSwitcher *ui;
    void createTrayIconAndMenu();
    bool isProcessRunning(const QString& processName);
    void setSceneById(const QString &sceneId);
    void setClientScene();
    void setGameScene();
    void checkGamePresence();
    void connectToStreamlabs();
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(QString message);
    void getScenes();
    void saveSettings();
    void saveSceneSettings();
    void loadSettings();
    void loadSceneSettings();
    void applySettings();
    void applySceneSettings();
    void setupUiConnections();
    void toggleUi(bool state);
    void showMainWindow();

    bool firstRun;
    QJsonObject settings;
    QJsonObject sceneSettings;
    static const QString settingsFile;
    static const QString sceneSettingsFile;
    QSystemTrayIcon *trayIcon;
    QTimer *timer;
    QWebSocket webSocket;
    QString streamlabsToken;
    QString clientSceneName;
    QString gameSceneName;
    QMap<QString, QString> sceneIdMap;
    bool switched;
    bool connecting;
    bool paused;
};

#endif // AUTOSCENESWITCHER_H
