INCLUDEPATH += $$PWD/include

!android {
    DEFINES += SDL_SUPPORTED
}

win32* {
    DEFINES += SDL_WIN
    DEFINES += SDL_MAIN_HANDLED
}

win32-g++* {
    LIBS += -L$$PWD/bin/windows/mingw/ -lSDL2
}

win32-msvc* {
    contains (QMAKE_TARGET.arch, x86_64) {
        LIBS += -L$$PWD/bin/windows/msvc/x64/ -lSDL2
    }

    else {
        LIBS += -L$$PWD/bin/windows/msvc/x86/ -lSDL2
    }
}

macx* {
    INCLUDEPATH += /opt/homebrew/include
    LIBS += -L/opt/homebrew/lib -lSDL2
}

linux:!android {
    LIBS += -lSDL2
}
