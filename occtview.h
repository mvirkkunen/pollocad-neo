#ifndef OCCTVIEW_H
#define OCCTVIEW_H

#include <QQuickItem>
#include <QQuickWindow>

#include "executor.h"

class OcctWrapper;

class OcctRenderer : public QObject {
    Q_OBJECT

public:
    ~OcctRenderer();

    void setParent(QQuickItem *parent) { m_parent = parent; }
    void setShape(Shape *shape);
    void wheelEvent(int delta);
    void mouseEvent(QPoint pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

public slots:
    void init();
    void paint();

private:
    QSize m_viewportSize;
    QQuickItem *m_parent = nullptr;
    OcctWrapper* m_wrapper = nullptr;
};

class OcctView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    OcctView();

    Q_INVOKABLE void setShape(Shape *shape);

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private slots:
    void sync();
    void cleanup();
    void handleWindowChanged(QQuickWindow *win);
    void handleMouseEvent(QMouseEvent *ev);

private:
    void releaseResources() override;

    OcctRenderer *m_renderer = nullptr;
};

#endif // OCCTVIEW_H
