QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20
QMAKE_CXXFLAGS += -std=c++20
QMAKE_CXXFLAGS += -fconcepts
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

msvc {
    QMAKE_CXXFLAGS += /utf-8
    QMAKE_CFLAGS += /utf-8
}


SOURCES += \
    card.cpp \
    cards.cpp \
    gamecontrol.cpp \
    gamemaster.cpp \
    main.cpp \
    player.cpp \
    playhand.cpp \
    server.cpp \
    userplayer.cpp

HEADERS += \
    card.h \
    cards.h \
    gamecontrol.h \
    gamemaster.h \
    player.h \
    playhand.h \
    server.cpp.autosave \
    server.h \
    userplayer.h

FORMS += \
    server.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
