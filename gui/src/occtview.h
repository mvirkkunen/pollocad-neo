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

    Q_PROPERTY(bool showHighlightedShapes READ showHighlightedShapes WRITE setShowHighlightedShapes NOTIFY showHighlightedShapesChanged)

public:
    OcctView();

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result);
    bool showHighlightedShapes() const;
    void setShowHighlightedShapes(bool show);

signals:
    void showHighlightedShapesChanged();

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void hoverMoveEvent(QHoverEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private slots:
    void sync();
    void cleanup();
    void scheduleRenderJob(std::function<void()> job);
    void handleWindowChanged(QQuickWindow *win);
    void handleMouseEvent(QSinglePointEvent *ev);

private:
    void releaseResources() override;

    OcctRenderer *m_renderer = nullptr;
};

#endif // OCCTVIEW_H
