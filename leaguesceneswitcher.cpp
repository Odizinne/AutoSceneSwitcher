#include "leaguesceneswitcher.h"
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

LeagueSceneSwitcher::LeagueSceneSwitcher(QWidget *parent)
    : QMainWindow(parent)
    , trayIcon(new QSystemTrayIcon(this))
    , timer(new QTimer(this))
    , switched(false)
{
    if (!prepareConfig()) {
        qCritical() << "Failed to prepare config file. Exiting.";
        exit(1);
    }

    loadConfig();
    if (streamlabsToken.isEmpty() || clientSceneName.isEmpty() || gameSceneName.isEmpty()) {
        qCritical() << "Token or scene names not found in config file. Exiting.";
        exit(1);
    }

    createTrayIconAndMenu();
    connectToStreamlabs();
}

LeagueSceneSwitcher::~LeagueSceneSwitcher() {
    // Clean up resources
    trayIcon->hide();
    timer->stop();
    webSocket.close();
}

bool LeagueSceneSwitcher::prepareConfig() {
    QString configDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir configDir(configDirPath);

    if (!configDir.exists()) {
        if (!configDir.mkpath(configDirPath)) {
            qWarning() << "Failed to create config directory:" << configDirPath;
            return false;
        }
    }

    QString configFilePath = configDirPath + "/config.json";
    QFile configFile(configFilePath);

    if (!configFile.exists()) {
        // Create an empty config file with placeholders for streamlabsToken, clientScene, and gameScene
        if (!configFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create config file:" << configFilePath;
            return false;
        }

        QJsonObject initialConfig;
        initialConfig["streamlabsToken"] = "";  // Placeholder for the token
        initialConfig["clientScene"] = "";      // Placeholder for client scene name
        initialConfig["gameScene"] = "";        // Placeholder for game scene name

        QJsonDocument doc(initialConfig);
        configFile.write(doc.toJson(QJsonDocument::Compact));
        configFile.close();
    }

    return true;
}

void LeagueSceneSwitcher::loadConfig() {
    QString configFilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config.json";
    QFile configFile(configFilePath);

    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << configFilePath;
        return;
    }

    QByteArray configData = configFile.readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(configData);

    if (!jsonDoc.isObject()) {
        qWarning() << "Invalid config file format.";
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    if (jsonObj.contains("streamlabsToken")) {
        streamlabsToken = jsonObj["streamlabsToken"].toString();
    } else {
        streamlabsToken.clear();
    }
    if (jsonObj.contains("clientScene")) {
        clientSceneName = jsonObj["clientScene"].toString();
    }
    if (jsonObj.contains("gameScene")) {
        gameSceneName = jsonObj["gameScene"].toString();
    }
}

// Create the system tray icon and menu
void LeagueSceneSwitcher::createTrayIconAndMenu() {
    QIcon icon(":/icons/icon.png");
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("League Scene Switcher");

    QMenu *menu = new QMenu(this);
    QAction *quitAction = menu->addAction("Quit");
    connect(quitAction, &QAction::triggered, this, &QApplication::quit);

    trayIcon->setContextMenu(menu);
    trayIcon->show();
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &LeagueSceneSwitcher::checkGamePresence);
    timer->start();
}

// Check if a process is running
bool LeagueSceneSwitcher::isProcessRunning(const QString& processName) {
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

// Set the scene by ID
void LeagueSceneSwitcher::setSceneById(const QString &sceneId) {
    QJsonObject params;
    params["resource"] = "ScenesService";
    params["args"] = QJsonArray{ sceneId };

    QJsonObject setSceneMessage;
    setSceneMessage["jsonrpc"] = "2.0";
    setSceneMessage["id"] = 32;  // Adjust the ID as needed
    setSceneMessage["method"] = "makeSceneActive";
    setSceneMessage["params"] = params;

    QJsonDocument doc(setSceneMessage);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "Sending setScene message:" << jsonString;
    webSocket.sendTextMessage(jsonString);
}

void LeagueSceneSwitcher::setClientScene() {
    if (sceneIdMap.contains(clientSceneName)) {
        QString sceneId = sceneIdMap[clientSceneName];
        setSceneById(sceneId);
    } else {
        qWarning() << "Client scene name not found in map.";
    }
}

void LeagueSceneSwitcher::setGameScene() {
    if (sceneIdMap.contains(gameSceneName)) {
        QString sceneId = sceneIdMap[gameSceneName];
        setSceneById(sceneId);
    } else {
        qWarning() << "Game scene name not found in map.";
    }
}

// Check game presence and switch scenes
void LeagueSceneSwitcher::checkGamePresence() {
    QString streamlabs = "Streamlabs OBS.exe";
    if (!isProcessRunning(streamlabs)) {
        return;
    }

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

// Connect to Streamlabs WebSocket
void LeagueSceneSwitcher::connectToStreamlabs() {
    connect(&webSocket, &QWebSocket::connected, this, &LeagueSceneSwitcher::onConnected);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &LeagueSceneSwitcher::onTextMessageReceived);
    webSocket.open(QUrl("ws://127.0.0.1:59650/api/websocket"));
}

// Handle WebSocket connection
void LeagueSceneSwitcher::onConnected() {
    qDebug() << "Connected to Streamlabs OBS WebSocket.";

    QJsonObject authParams;
    authParams["resource"] = "TcpServerService";
    authParams["args"] = QJsonArray{ streamlabsToken };

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

void LeagueSceneSwitcher::getScenes() {
    QJsonObject params;
    params["resource"] = "ScenesService";

    QJsonObject getScenesMessage;
    getScenesMessage["jsonrpc"] = "2.0";
    getScenesMessage["id"] = 31;  // Adjust the ID as needed
    getScenesMessage["method"] = "getScenes";
    getScenesMessage["params"] = params;

    QJsonDocument doc(getScenesMessage);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qDebug() << "Sending getScenes message:" << jsonString;
    webSocket.sendTextMessage(jsonString);
}

void LeagueSceneSwitcher::onTextMessageReceived(QString message) {
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

                qDebug() << "Scene ID:" << sceneId;
                qDebug() << "Scene Name:" << sceneName;

                sceneIdMap[sceneName] = sceneId;

                QJsonArray nodes = sceneObj["nodes"].toArray();
                qDebug() << "Nodes in scene:";
                for (const QJsonValue &nodeValue : nodes) {
                    QJsonObject nodeObj = nodeValue.toObject();
                    QString nodeId = nodeObj["id"].toString();
                    QString nodeName = nodeObj["name"].toString();
                    QString sourceId = nodeObj["sourceId"].toString();

                    qDebug() << "Node ID:" << nodeId;
                    qDebug() << "Node Name:" << nodeName;
                    qDebug() << "Source ID:" << sourceId;
                }
            }

            // Ensure that the scene names are mapped correctly
            if (sceneIdMap.contains(clientSceneName)) {
                qDebug() << "Client scene ID:" << sceneIdMap[clientSceneName];
            } else {
                qWarning() << "Client scene name not found.";
            }

            if (sceneIdMap.contains(gameSceneName)) {
                qDebug() << "Game scene ID:" << sceneIdMap[gameSceneName];
            } else {
                qWarning() << "Game scene name not found.";
            }
        }
    } else {
        qWarning() << "Unexpected message ID or method.";
    }
}
