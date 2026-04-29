QT += widgets
#QT += widgets gui x11extras

TEMPLATE = app
TARGET = hawk
INCLUDEPATH += .

# Add the X11 library for the "Bring to Top" logic
#unix: LIBS += -lX11

# Only include the GUI main.cpp
SOURCES += main.cpp
