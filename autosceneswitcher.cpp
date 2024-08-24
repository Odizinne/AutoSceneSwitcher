#include "autosceneswitcher.h"
#include "ui_autosceneswitcher.h"
#include "shortcutmanager.h"
#include <QApplication>
#include <QMenu>
#include <windows.h>
#include <tlhelp32.h>
#include <QString>
#include <QIcon>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

const QString AutoSceneSwitcher::settingsFile =
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AutoSceneSwitcher/settings.json";

AutoSceneSwitcher::AutoSceneSwitcher(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AutoSceneSwitcher)
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

AutoSceneSwitcher::~AutoSceneSwitcher()
{
    trayIcon->hide();
    timer->stop();
    webSocket.close();
    delete ui;
}

void AutoSceneSwitcher::setupUiConnections()
{
    connect(ui->tokenLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->clientLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->gameLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->processLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->startupCheckBox, &QCheckBox::stateChanged, this, &AutoSceneSwitcher::onStartupCheckBoxStateChanged);

    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &AutoSceneSwitcher::checkGamePresence);
    timer->start();

    ui->tokenLineEdit->setEchoMode(QLineEdit::Password);
}

void AutoSceneSwitcher::onStartupCheckBoxStateChanged()
{
    manageShortcut(ui->startupCheckBox->isChecked());
}

void AutoSceneSwitcher::loadSettings()
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

void AutoSceneSwitcher::applySettings()
{
    ui->tokenLineEdit->setText(settings.value("streamlabsToken").toString());
    ui->clientLineEdit->setText(settings.value("clientScene").toString());
    ui->gameLineEdit->setText(settings.value("gameScene").toString());
    ui->processLineEdit->setText(settings.value("targetProcess").toString());
    ui->startupCheckBox->setChecked(isShortcutPresent());
}

void AutoSceneSwitcher::saveSettings()
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
}

void AutoSceneSwitcher::createTrayIconAndMenu()
{
    QIcon icon(":/icons/icon.png");
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("Auto Scene Switcher");

    QMenu *menu = new QMenu(this);

    QAction *showAction = menu->addAction("Show");
    connect(showAction, &QAction::triggered, this, &AutoSceneSwitcher::showMainWindow);

    QAction *quitAction = menu->addAction("Quit");
    connect(quitAction, &QAction::triggered, this, &QApplication::quit);

    trayIcon->setContextMenu(menu);
    trayIcon->show();
}

void AutoSceneSwitcher::showMainWindow()
{
    this->show();
    this->activateWindow();
}

bool AutoSceneSwitcher::isProcessRunning(const QString& processName)
{
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
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

void AutoSceneSwitcher::setSceneById(const QString &sceneId)
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

    webSocket.sendTextMessage(jsonString);
}

void AutoSceneSwitcher::setClientScene()
{
    QString clientSceneName = ui->clientLineEdit->text().trimmed();
    if (sceneIdMap.contains(clientSceneName)) {
        QString sceneId = sceneIdMap[clientSceneName];
        setSceneById(sceneId);
    }
}

void AutoSceneSwitcher::setGameScene()
{
    QString gameSceneName = ui->gameLineEdit->text().trimmed();
    if (sceneIdMap.contains(gameSceneName)) {
        QString sceneId = sceneIdMap[gameSceneName];
        setSceneById(sceneId);
    }
}

void AutoSceneSwitcher::toggleUi(bool state)
{
    ui->sceneSettingsLabel->setVisible(state);
    ui->clientFrame->setVisible(state);
    ui->gameFrame->setVisible(state);
    ui->noConnectionLabel->setVisible(!state);
    this->adjustSize();
}

void AutoSceneSwitcher::checkGamePresence()
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

    QString targetProcess = ui->processLineEdit->text();
    if (isProcessRunning(targetProcess) && !switched && !ui->processLineEdit->text().isEmpty() && !ui->processLineEdit->hasFocus()) {
        setGameScene();
        switched = true;
    } else if (!isProcessRunning(targetProcess) && switched) {
        setClientScene();
        switched = false;
    }
}

void AutoSceneSwitcher::connectToStreamlabs()
{
    connect(&webSocket, &QWebSocket::connected, this, &AutoSceneSwitcher::onConnected);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &AutoSceneSwitcher::onTextMessageReceived);

    webSocket.open(QUrl("ws://127.0.0.1:59650/api/websocket"));
}

void AutoSceneSwitcher::onConnected()
{
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

    webSocket.sendTextMessage(jsonString);

    getScenes();
    toggleUi(true);
}

void AutoSceneSwitcher::getScenes()
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

    webSocket.sendTextMessage(jsonString);
}

void AutoSceneSwitcher::onTextMessageReceived(QString message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    QJsonObject jsonObj = doc.object();

    if (jsonObj.contains("id") && jsonObj["id"].toInt() == 31) {
        if (jsonObj.contains("result")) {
            QJsonArray scenes = jsonObj["result"].toArray();
            sceneIdMap.clear();

            for (const QJsonValue &sceneValue : scenes) {
                QJsonObject sceneObj = sceneValue.toObject();
                QString sceneId = sceneObj["id"].toString();
                QString sceneName = sceneObj["name"].toString();

                sceneIdMap[sceneName] = sceneId;
            }
        }
    }
}

