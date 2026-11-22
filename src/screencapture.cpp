#include "screencapture.h"

#include <QQuickWindow>
#include <QThread>

QImage captureScreen(QQuickWindow *window)
{
    if (!window || QThread::currentThread() != window->thread()
        || !window->isVisible() || !window->isExposed() || !window->isSceneGraphInitialized())
        return {};

    // On the Paper Pro E Ink backend, grabWindow reads the last rendered image.
    // Detach its pixels before the next frame or a worker can use them.
    return window->grabWindow().copy();
}
