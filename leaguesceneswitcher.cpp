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

// Constant for settings file location
const QString LeagueSceneSwitcher::settingsFile =
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/LeagueSceneSwitcher/settings.json";

// Constructor
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

// Destructor
LeagueSceneSwitcher::~LeagueSceneSwitcher()
{
    trayIcon->hide();
    timer->stop();
    webSocket.close();
    delete ui;
}

// Set up UI connections
void LeagueSceneSwitcher::setupUiConnections()
{
    connect(ui->tokenLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);
    connect(ui->clientLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);
    connect(ui->gameLineEdit, &QLineEdit::textChanged, this, &LeagueSceneSwitcher::saveSettings);

    // Set up timer to check game presence and Streamlabs connection every second
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &LeagueSceneSwitcher::checkGamePresence);
    timer->start();
}

// Load settings from JSON file
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
        saveSettings();  // Save default settings if the file doesn't exist
    }

    applySettings();
}

// Apply settings to the UI
void LeagueSceneSwitcher::applySettings()
{
    ui->tokenLineEdit->setText(settings.value("streamlabsToken").toString());
    ui->clientLineEdit->setText(settings.value("clientScene").toString());
    ui->gameLineEdit->setText(settings.value("gameScene").toString());
}

// Save settings to JSON file
void LeagueSceneSwitcher::saveSettings()
{
    settings["streamlabsToken"] = ui->tokenLineEdit->text();
    settings["clientScene"] = ui->clientLineEdit->text();
    settings["gameScene"] = ui->gameLineEdit->text();

    QFile file(settingsFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(settings);
        file.write(doc.toJson());
        file.close();
    }
    qDebug() << "Settings saved.";
}

// Create system tray icon and menu
void LeagueSceneSwitcher::createTrayIconAndMenu()
{
    QIcon icon(":/icons/icon.png");
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("League Scene Switcher");

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

// Check if a process is running
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

// Set the scene by ID via WebSocket
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

// Set client scene
void LeagueSceneSwitcher::setClientScene()
{
    QString clientSceneName = ui->clientLineEdit->text().trimmed();  // Read from UI
    qDebug() << "Attempting to set client scene with name:" << clientSceneName;

    if (sceneIdMap.contains(clientSceneName)) {
        QString sceneId = sceneIdMap[clientSceneName];
        setSceneById(sceneId);
        qDebug() << "Client scene set to:" << clientSceneName;
    } else {
        qWarning() << "Client scene name not found in map.";
    }
}

// Set game scene
void LeagueSceneSwitcher::setGameScene()
{
    QString gameSceneName = ui->gameLineEdit->text().trimmed();  // Read from UI
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
    ui->clientFrame->setVisible(state);
    ui->gameFrame->setVisible(state);
    ui->noConnectionLabel->setVisible(!state);
    this->adjustSize();
}
// Check game presence and Streamlabs connection
void LeagueSceneSwitcher::checkGamePresence()
{

    // Check if the token is not empty
    if (ui->tokenLineEdit->text().isEmpty()) {
        toggleUi(false);
        return;
    }

    // Check if Streamlabs Desktop is running
    QString streamlabsProcessName = "Streamlabs OBS.exe";
    if (!isProcessRunning(streamlabsProcessName)) {
        toggleUi(false);
        return;
    }

    // Connect to Streamlabs API if not already connected
    if (!webSocket.isValid()) {
        toggleUi(false);
        connectToStreamlabs();
    }
    toggleUi(true);

    // Check for League of Legends process and switch scenes accordingly
    QString leagueGame = "League of Legends.exe";
    if (isProcessRunning(leagueGame) && !switched) {
        qInfo() << leagueGame << "is running.";
        setGameScene();
        switched = true;
    } else if (!isProcessRunning(leagueGame) && switched) {
        qInfo() << leagueGame << "is not running.";
        setClientScene();
        switched = false;
    }
}

// Connect to Streamlabs WebSocket API
void LeagueSceneSwitcher::connectToStreamlabs()
{
    connect(&webSocket, &QWebSocket::connected, this, &LeagueSceneSwitcher::onConnected);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &LeagueSceneSwitcher::onTextMessageReceived);

    webSocket.open(QUrl("ws://127.0.0.1:59650/api/websocket"));
}

// Handle WebSocket connection
void LeagueSceneSwitcher::onConnected()
{
    qDebug() << "Connected to Streamlabs OBS WebSocket.";

    // Authenticate with the provided Streamlabs token
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

    // Request scenes after connecting
    getScenes();
}

// Request scenes from Streamlabs OBS
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

// Handle received WebSocket messages
void LeagueSceneSwitcher::onTextMessageReceived(QString message)
{
    qDebug() << "Message received from Streamlabs OBS:" << message;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Received message is not a valid JSON object.";
        return;
    }

    QJsonObject jsonObj = doc.object();

    // Handle the getScenes response
    if (jsonObj.contains("id") && jsonObj["id"].toInt() == 31) {
        if (jsonObj.contains("result")) {
            QJsonArray scenes = jsonObj["result"].toArray();
            qDebug() << "Scenes available:";
            sceneIdMap.clear();

            // Populate the scene ID map and ComboBoxes
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

