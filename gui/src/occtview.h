#ifndef OCCTVIEW_H
#define OCCTVIEW_H

#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <TopoDS_Compound.hxx>

#include "spanobj.h"

class BackgroundExecutorResult;
class OcctRenderer;

class OcctView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool showHighlightedShapes READ showHighlightedShapes WRITE setShowHighlightedShapes NOTIFY showHighlightedShapesChanged)
    Q_PROPERTY(int hoveredPosition READ hoveredPosition WRITE setHoveredPosition NOTIFY hoveredPositionChanged);
    Q_PROPERTY(QList<SpanObj> hoveredSpans READ hoveredSpans NOTIFY hoveredSpansChanged)

public:
    OcctView();

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result);
    Q_INVOKABLE void exportResult(QUrl url);

    int hoveredPosition() const { return m_hoveredPosition; }
    void setHoveredPosition(int position);

    QList<SpanObj> hoveredSpans() const { return m_hoveredSpans; }

    bool showHighlightedShapes() const { return m_showHighlightedShapes; }
    void setShowHighlightedShapes(bool show);

signals:
    void showHighlightedShapesChanged();
    void hoveredPositionChanged();
    void hoveredSpansChanged();

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
    int m_hoveredPosition = -1;
    QList<SpanObj> m_hoveredSpans;
    bool m_showHighlightedShapes = true;
    TopoDS_Compound m_resultCompound;
};

#endif // OCCTVIEW_H
