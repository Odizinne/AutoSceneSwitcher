QT       += core gui websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
    src/AutoSceneSwitcher/ \
    src/ShortcutManager/

SOURCES += \
    src/AutoSceneSwitcher/autosceneswitcher.cpp \
    src/ShortcutManager/shortcutmanager.cpp \
    src/main.cpp

HEADERS += \
    src/AutoSceneSwitcher/autosceneswitcher.h \
    src/ShortcutManager/shortcutmanager.h

FORMS += \
    src/AutoSceneSwitcher/autosceneswitcher.ui

TRANSLATIONS += \
    src/Resources/Tr/AutoSceneSwitcher_fr.ts \
    src/Resources/Tr/AutoSceneSwitcher_en.ts

RESOURCES += \
    src/Resources/resources.qrc \
    src/Resources/translations.qrc

RC_FILE = src/Resources/appicon.rc

LIBS += -luser32 -lole32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
