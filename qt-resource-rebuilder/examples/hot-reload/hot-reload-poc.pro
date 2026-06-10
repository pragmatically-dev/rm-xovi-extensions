TEMPLATE = lib
TARGET   = hot-reload-poc
CONFIG  += shared plugin no_plugin_name_prefix c++17

QMAKE_LFLAGS += -Wl,--no-undefined

OBJECTS_DIR = build/obj
MOC_DIR     = build/moc
XOVI_DIR    = build/xovi

# xovigen turns the .xovi manifest into the import glue (the resolved
# qt-resource-rebuilder$qrr_reload_external_diff pointer).
xoviextension.target   = $$XOVI_DIR/xovi.c
xoviextension.commands = mkdir -p $$XOVI_DIR && python3 $$(XOVI_REPO)/util/xovigen.py -o $$XOVI_DIR/xovi.c -H $$XOVI_DIR/xovi.h hot-reload-poc.xovi
xoviextension.depends  = hot-reload-poc.xovi
QMAKE_EXTRA_TARGETS += xoviextension
PRE_TARGETDEPS      += $$XOVI_DIR/xovi.c

QT += core gui qml quick

SOURCES += \
    $$XOVI_DIR/xovi.c \
    hot-reload-poc.cpp

INCLUDEPATH += $$XOVI_DIR $$PWD

QMAKE_CXXFLAGS += -fPIC -O2
QMAKE_CFLAGS   += -fPIC -O2
