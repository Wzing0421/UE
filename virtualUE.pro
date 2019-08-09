#-------------------------------------------------
#
# Project created by QtCreator 2019-07-16T16:49:08
#
#-------------------------------------------------

QT       += core gui
QT       += network
QT       += multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = virtualUE
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    audioplaythread.cpp \
    audiosendthread.cpp

HEADERS  += mainwindow.h \
    audioplaythread.h \
    audiosendthread.h \
    config.h

FORMS    += mainwindow.ui
