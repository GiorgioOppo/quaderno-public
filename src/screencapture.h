#pragma once

#include <QImage>

class QQuickWindow;

// Call on the window's GUI thread. A successful capture can be used by a worker.
QImage captureScreen(QQuickWindow *window);
