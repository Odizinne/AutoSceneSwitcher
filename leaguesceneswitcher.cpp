#include "leaguesceneswitcher.h"
#include "ui_leaguesceneswitcher.h"

#include <QApplication>
#include <QMenu>
#include <windows.h>
#include <tlhelp32.h>
#include <QString>
#include <QDebug>
#include <QIcon>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

const QString LeagueSceneSwitcher::settingsFile =
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AutoSceneSwitcher/settings.json";

LeagueSceneSwitcher::LeagueSceneSwitcher(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LeagueSceneSwitcher)
    , firstRun(false)
    , trayIcon(new QSystemTrayIcon(this))
    , timer(new QTimer(this))
    , switched(false)
{
    ui->setupUi(this);
    toggleUi(false);
    loadSettings();
    setupUiConnections();
    createTrayIconAndMenu();
    if (firstRun) {
        this->show();
    }
}

LeagueSceneSwitcher::~LeagueSceneSwitcher()
{
    trayIcon->hide();
    timer->stop();
    webSocket.close();
    delete ui;
}

void LeagueSceneSwitcher::setupUiConnections()
{
    connect(ui->tokenLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);
    connect(ui->clientLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);
    connect(ui->gameLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);
    connect(ui->processLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);

    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &LeagueSceneSwitcher::checkGamePresence);
    timer->start();

    ui->tokenLineEdit->setEchoMode(QLineEdit::Password);
}

void LeagueSceneSwitcher::loadSettings()
{
    QDir settingsDir(QFileInfo(settingsFile).absolutePath());
    if (!settingsDir.exists()) {
        settingsDir.mkpath(settingsDir.absolutePath());
        firstRun = true;
    }

    QFile file(settingsFile);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        settings = doc.object();
        file.close();
    } else {
        saveSettings();
    }

    applySettings();
}

void LeagueSceneSwitcher::applySettings()
{
    ui->tokenLineEdit->setText(settings.value("streamlabsToken").toString());
    ui->clientLineEdit->setText(settings.value("clientScene").toString());
    ui->gameLineEdit->setText(settings.value("gameScene").toString());
    ui->processLineEdit->setText(settings.value("targetProcess").toString());
}

void LeagueSceneSwitcher::saveSettings()
{
    settings["streamlabsToken"] = ui->tokenLineEdit->text();
    settings["clientScene"] = ui->clientLineEdit->text();
    settings["gameScene"] = ui->gameLineEdit->text();
    settings["targetProcess"] = ui->processLineEdit->text();

    QFile file(settingsFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(settings);
        file.write(doc.toJson());
        file.close();
    }
    qDebug() << "Settings saved.";
}

void LeagueSceneSwitcher::createTrayIconAndMenu()
{
    QIcon icon(":/icons/icon.png");
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("Auto Scene Switcher");

    QMenu *menu = new QMenu(this);

    QAction *showAction = menu->addAction("Show");
    connect(showAction, &QAction::triggered, this, &LeagueSceneSwitcher::showMainWindow);

    QAction *quitAction = menu->addAction("Quit");
    connect(quitAction, &QAction::triggered, this, &QApplication::quit);

    trayIcon->setContextMenu(menu);
    trayIcon->show();
}

void LeagueSceneSwitcher::showMainWindow()
{
    this->show();
    this->activateWindow();
}

bool LeagueSceneSwitcher::isProcessRunning(const QString& processName)
{
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        qWarning() << "Failed to create process snapshot.";
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        qWarning() << "Failed to retrieve the first process.";
        return false;
    }

    bool found = false;
    do {
        if (QString::fromWCharArray(pe32.szExeFile).compare(processName, Qt::CaseInsensitive) == 0) {
            found = true;
            break;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return found;
}

void LeagueSceneSwitcher::setSceneById(const QString &sceneId)
{
    QJsonObject params;
    params["resource"] = "ScenesService";
    params["args"] = QJsonArray{ sceneId };

    QJsonObject setSceneMessage;
    setSceneMessage["jsonrpc"] = "2.0";
    setSceneMessage["id"] = 32;
    setSceneMessage["method"] = "makeSceneActive";
    setSceneMessage["params"] = params;

    QJsonDocument doc(setSceneMessage);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "Sending setScene message:" << jsonString;
    webSocket.sendTextMessage(jsonString);
}

void LeagueSceneSwitcher::setClientScene()
{
    QString clientSceneName = ui->clientLineEdit->text().trimmed();
    qDebug() << "Attempting to set client scene with name:" << clientSceneName;

    if (sceneIdMap.contains(clientSceneName)) {
        QString sceneId = sceneIdMap[clientSceneName];
        setSceneById(sceneId);
        qDebug() << "Client scene set to:" << clientSceneName;
    } else {
        qWarning() << "Client scene name not found in map.";
    }
}

void LeagueSceneSwitcher::setGameScene()
{
    QString gameSceneName = ui->gameLineEdit->text().trimmed();
    qDebug() << "Attempting to set game scene with name:" << gameSceneName;

    if (sceneIdMap.contains(gameSceneName)) {
        QString sceneId = sceneIdMap[gameSceneName];
        setSceneById(sceneId);
        qDebug() << "Game scene set to:" << gameSceneName;
    } else {
        qWarning() << "Game scene name not found in map.";
    }
}

void LeagueSceneSwitcher::toggleUi(bool state)
{
    ui->sceneSettingsLabel->setVisible(state);
    ui->clientFrame->setVisible(state);
    ui->gameFrame->setVisible(state);
    ui->noConnectionLabel->setVisible(!state);
    this->adjustSize();
}

void LeagueSceneSwitcher::checkGamePresence()
{

    if (ui->tokenLineEdit->text().isEmpty()) {
        toggleUi(false);
        return;
    }

    QString streamlabsProcessName = "Streamlabs OBS.exe";
    if (!isProcessRunning(streamlabsProcessName)) {
        toggleUi(false);
        return;
    }

    if (!webSocket.isValid()) {
        toggleUi(false);
        connectToStreamlabs();
    }
    toggleUi(true);

    QString targetProcess = ui->processLineEdit->text();
    if (isProcessRunning(targetProcess) && !switched && !ui->processLineEdit->text().isEmpty() && !ui->processLineEdit->hasFocus()) {
        qInfo() << targetProcess << "is running.";
        setGameScene();
        switched = true;
    } else if (!isProcessRunning(targetProcess) && switched) {
        qInfo() << targetProcess << "is not running.";
        setClientScene();
        switched = false;
    }
}

void LeagueSceneSwitcher::connectToStreamlabs()
{
    connect(&webSocket, &QWebSocket::connected, this, &LeagueSceneSwitcher::onConnected);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &LeagueSceneSwitcher::onTextMessageReceived);

    webSocket.open(QUrl("ws://127.0.0.1:59650/api/websocket"));
}

void LeagueSceneSwitcher::onConnected()
{
    qDebug() << "Connected to Streamlabs OBS WebSocket.";

    QJsonObject authParams;
    authParams["resource"] = "TcpServerService";
    authParams["args"] = QJsonArray{ ui->tokenLineEdit->text() };

    QJsonObject authMessage;
    authMessage["jsonrpc"] = "2.0";
    authMessage["id"] = 8;
    authMessage["method"] = "auth";
    authMessage["params"] = authParams;

    QJsonDocument doc(authMessage);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact)) + "\n";

    qDebug() << "Sending authentication message: " << jsonString;
    webSocket.sendTextMessage(jsonString);

    getScenes();
}

void LeagueSceneSwitcher::getScenes()
{
    QJsonObject params;
    params["resource"] = "ScenesService";

    QJsonObject getScenesMessage;
    getScenesMessage["jsonrpc"] = "2.0";
    getScenesMessage["id"] = 31;
    getScenesMessage["method"] = "getScenes";
    getScenesMessage["params"] = params;

    QJsonDocument doc(getScenesMessage);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "Sending getScenes message:" << jsonString;
    webSocket.sendTextMessage(jsonString);
}

void LeagueSceneSwitcher::onTextMessageReceived(QString message)
{
    qDebug() << "Message received from Streamlabs OBS:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Received message is not a valid JSON object.";
        return;
    }

    QJsonObject jsonObj = doc.object();

    if (jsonObj.contains("id") && jsonObj["id"].toInt() == 31) {
        if (jsonObj.contains("result")) {
            QJsonArray scenes = jsonObj["result"].toArray();
            qDebug() << "Scenes available:";
            sceneIdMap.clear();

            for (const QJsonValue &sceneValue : scenes) {
                QJsonObject sceneObj = sceneValue.toObject();
                QString sceneId = sceneObj["id"].toString();
                QString sceneName = sceneObj["name"].toString();

                sceneIdMap[sceneName] = sceneId;

                qDebug() << "Scene ID:" << sceneId << "Scene Name:" << sceneName;
            }
        }
    } else {
        qWarning() << "Unexpected message ID or method.";
    }
}

