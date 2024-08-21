#include "leaguesceneswitcher.h"
#include <QApplication>
#include <QMenu>
#include <windows.h>
#include <tlhelp32.h>
#include <QString>
#include <QDebug>
#include <QIcon>
#include <Qtimer>

LeagueSceneSwitcher::LeagueSceneSwitcher(QWidget *parent)
    : QMainWindow(parent)
    , trayIcon(new QSystemTrayIcon(this))
    , timer(new QTimer(this))
    , switched(false)
{
    createTrayIconAndMenu();
}

LeagueSceneSwitcher::~LeagueSceneSwitcher() {}

void LeagueSceneSwitcher::createTrayIconAndMenu() {
    QIcon icon(":/icons/icon.png");
    trayIcon->setIcon(icon);
    trayIcon->setToolTip("League Scene Switcher");

    QMenu *menu = new QMenu(this);
    QAction *quitAction = menu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayIcon->setContextMenu(menu);
    trayIcon->show();
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &LeagueSceneSwitcher::checkGamePresence);
    timer->start();
}

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

void LeagueSceneSwitcher::setClientScene() {
    INPUT inputs[6] = {};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_SHIFT;

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_PRIOR;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_PRIOR;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[4].type = INPUT_KEYBOARD;
    inputs[4].ki.wVk = VK_SHIFT;
    inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[5].type = INPUT_KEYBOARD;
    inputs[5].ki.wVk = VK_CONTROL;
    inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(6, inputs, sizeof(INPUT));
}

void LeagueSceneSwitcher::setGameScene() {
    INPUT inputs[6] = {};

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_SHIFT;

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_NEXT;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_NEXT;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[4].type = INPUT_KEYBOARD;
    inputs[4].ki.wVk = VK_SHIFT;
    inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[5].type = INPUT_KEYBOARD;
    inputs[5].ki.wVk = VK_CONTROL;
    inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(6, inputs, sizeof(INPUT));
}

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
