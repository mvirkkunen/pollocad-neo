#ifndef OCCTVIEW_H
#define OCCTVIEW_H

#include <QQuickItem>
#include <QQuickWindow>

#include "backgroundexecutor.h"

class OcctRenderer;

class OcctView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    OcctView();

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result);

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void hoverMoveEvent(QHoverEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private slots:
    void sync();
    void cleanup();
    void handleWindowChanged(QQuickWindow *win);
    void handleMouseEvent(QSinglePointEvent *ev);

private:
    void releaseResources() override;

    OcctRenderer *m_renderer = nullptr;
};

#endif // OCCTVIEW_H
