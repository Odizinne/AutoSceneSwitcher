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

const QString AutoSceneSwitcher::sceneSettingsFile =
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AutoSceneSwitcher/scene_settings.json";

AutoSceneSwitcher::AutoSceneSwitcher(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AutoSceneSwitcher)
    , firstRun(false)
    , trayIcon(new QSystemTrayIcon(this))
    , timer(new QTimer(this))
    , switched(false)
    , connecting(false)
    , paused(false)
{
    ui->setupUi(this);
    toggleUi(false);
    loadSettings();
    setupUiConnections();
    createTrayIconAndMenu();
    toggleUi(false);
    if (firstRun) {
        this->show();
    }
}

AutoSceneSwitcher::~AutoSceneSwitcher()
{
    timer->stop();
    webSocket.close();
    delete ui;
}

void AutoSceneSwitcher::setupUiConnections()
{
    connect(ui->tokenLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->processLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->IPLineEdit, &QLineEdit::textChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->portSpinBox, &QSpinBox::valueChanged, this, &AutoSceneSwitcher::saveSettings);
    connect(ui->startupCheckBox, &QCheckBox::stateChanged, this, &AutoSceneSwitcher::onStartupCheckBoxStateChanged);
    connect(ui->toggleTokenButton, &QToolButton::clicked, this, &AutoSceneSwitcher::toggleTokenView);
    connect(ui->pauseButton, &QPushButton::clicked, this, &AutoSceneSwitcher::onPauseButtonClicked);
    connect(ui->refreshScenesButton, &QPushButton::clicked, this, &AutoSceneSwitcher::onRefreshScenesButtonClicked);
    ui->refreshScenesButton->setDisabled(true);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &AutoSceneSwitcher::checkGamePresence);
    timer->start();

    ui->tokenLineEdit->setEchoMode(QLineEdit::Password);
}

void AutoSceneSwitcher::onStartupCheckBoxStateChanged()
{
    manageShortcut(ui->startupCheckBox->isChecked());
}

void AutoSceneSwitcher::onPauseButtonClicked()
{
    if (paused) {
        paused = false;
        timer->start();
        ui->pauseButton->setText(tr("Pause"));
        connectToStreamlabs();
    } else {
        paused = true;
        timer->stop();
        ui->pauseButton->setText(tr("Resume"));
        webSocket.close();
    }
}

void AutoSceneSwitcher::onRefreshScenesButtonClicked()
{
    if (webSocket.isValid()) {
        disconnect(ui->gameComboBox, &QComboBox::currentTextChanged, this, &AutoSceneSwitcher::saveSceneSettings);
        disconnect(ui->clientComboBox, &QComboBox::currentTextChanged, this, &AutoSceneSwitcher::saveSceneSettings);
        ui->gameComboBox->clear();
        ui->clientComboBox->clear();
        getScenes();
    }
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
    ui->processLineEdit->setText(settings.value("targetProcess").toString());
    ui->IPLineEdit->setText(settings.value("ipAddress").toString());
    ui->portSpinBox->setValue(settings.value("port").toInt());
    ui->startupCheckBox->setChecked(isShortcutPresent());
}

void AutoSceneSwitcher::saveSettings()
{
    settings["streamlabsToken"] = ui->tokenLineEdit->text();
    settings["targetProcess"] = ui->processLineEdit->text();
    settings["ipAddress"] = ui->IPLineEdit->text();
    settings["port"] = ui->portSpinBox->value();

    QFile file(settingsFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(settings);
        file.write(doc.toJson());
        file.close();
    }
}

void AutoSceneSwitcher::toggleTokenView()
{
    if (ui->tokenLineEdit->echoMode() == QLineEdit::Password) {
        ui->tokenLineEdit->setEchoMode(QLineEdit::Normal);
        ui->toggleTokenButton->setText(tr("Hide"));
    } else {
        ui->tokenLineEdit->setEchoMode(QLineEdit::Password);
        ui->toggleTokenButton->setText(tr("Show"));
    }
}

void AutoSceneSwitcher::createTrayIconAndMenu()
{
    QIcon icon(":/icons/icon.png");
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("Auto Scene Switcher");

    QMenu *menu = new QMenu(this);

    QAction *showAction = menu->addAction(tr("Show"));
    connect(showAction, &QAction::triggered, this, &AutoSceneSwitcher::showMainWindow);

    QAction *quitAction = menu->addAction(tr("Quit"));
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
    QString clientSceneName = ui->clientComboBox->currentText();
    auto it = std::find_if(sceneIdMap.begin(), sceneIdMap.end(),
                           [&clientSceneName](const QString &key) { return key == clientSceneName; });

    if (it != sceneIdMap.end()) {
        QString sceneId = it.value();
        setSceneById(sceneId);
    }
}

void AutoSceneSwitcher::setGameScene()
{
    QString gameSceneName = ui->gameComboBox->currentText();
    auto it = std::find_if(sceneIdMap.begin(), sceneIdMap.end(),
                           [&gameSceneName](const QString &key) { return key == gameSceneName; });

    if (it != sceneIdMap.end()) {
        QString sceneId = it.value();
        setSceneById(sceneId);
    }
}

void AutoSceneSwitcher::toggleUi(bool state)
{
    ui->sceneSettingsLabel->setEnabled(state);
    ui->sceneFrame->setEnabled(state);
    ui->refreshScenesButton->setEnabled(state);
}

void AutoSceneSwitcher::checkGamePresence()
{
    if (ui->tokenLineEdit->text().isEmpty()) {
        return;
    }

    QString streamlabsProcessName = "Streamlabs OBS.exe";
    if (!isProcessRunning(streamlabsProcessName)) {
        return;
    }

    if (!webSocket.isValid()) {
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
    if (connecting) {
        return;
    }
    connecting = true;
    ui->connectionStatusLabel->setText(tr("Connecting to Streamlabs client API... 🔄"));
    connect(&webSocket, &QWebSocket::connected, this, &AutoSceneSwitcher::onConnected);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &AutoSceneSwitcher::onTextMessageReceived);
    connect(&webSocket, &QWebSocket::disconnected, this, &AutoSceneSwitcher::onDisconnected);

    QString ipAddress = ui->IPLineEdit->text();
    if (ipAddress.isEmpty()) {
        ipAddress = "127.0.0.1";
    }
    int port = ui->portSpinBox->value();
    QString urlString = QString("ws://%1:%2/api/websocket").arg(ipAddress).arg(port);
    webSocket.open(QUrl(urlString));
}

void AutoSceneSwitcher::onConnected()
{
    connecting = false;
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
    ui->connectionStatusLabel->setText(tr("Connected to Streamlabs client API ✅"));
}

void AutoSceneSwitcher::onDisconnected()
{
    connecting = false;

    disconnect(&webSocket, &QWebSocket::connected, this, &AutoSceneSwitcher::onConnected);
    disconnect(&webSocket, &QWebSocket::textMessageReceived, this, &AutoSceneSwitcher::onTextMessageReceived);
    disconnect(&webSocket, &QWebSocket::disconnected, this, &AutoSceneSwitcher::onDisconnected);
    disconnect(ui->gameComboBox, &QComboBox::currentTextChanged, this, &AutoSceneSwitcher::saveSceneSettings);
    disconnect(ui->clientComboBox, &QComboBox::currentTextChanged, this, &AutoSceneSwitcher::saveSceneSettings);
    ui->refreshScenesButton->setDisabled(true);

    ui->clientComboBox->clear();
    ui->gameComboBox->clear();
    toggleUi(false);

    ui->connectionStatusLabel->setText(tr("Not connected to Streamlabs client API ❌"));
}

void AutoSceneSwitcher::getScenes()
{
    if (!webSocket.isValid()) {
        return;
    }

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
                ui->gameComboBox->addItem(sceneName);
                ui->clientComboBox->addItem(sceneName);
            }
            loadSceneSettings();
            connect(ui->gameComboBox, &QComboBox::currentTextChanged, this, &AutoSceneSwitcher::saveSceneSettings);
            connect(ui->clientComboBox, &QComboBox::currentTextChanged, this, &AutoSceneSwitcher::saveSceneSettings);
            ui->refreshScenesButton->setEnabled(true);
        }
    }
}

void AutoSceneSwitcher::saveSceneSettings()
{
    sceneSettings["clientScene"] = ui->clientComboBox->currentText();
    sceneSettings["gameScene"] = ui->gameComboBox->currentText();

    QFile file(sceneSettingsFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(sceneSettings);
        file.write(doc.toJson());
        file.close();
    }
}

void AutoSceneSwitcher::loadSceneSettings()
{
    QFile file(sceneSettingsFile);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        sceneSettings = doc.object();
        file.close();
    } else {
        saveSceneSettings();
    }

    applySceneSettings();
}

void AutoSceneSwitcher::applySceneSettings()
{
    ui->clientComboBox->setCurrentText(sceneSettings.value("clientScene").toString());
    ui->gameComboBox->setCurrentText(sceneSettings.value("gameScene").toString());
}
