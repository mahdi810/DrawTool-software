#ifndef DIAGRAMVIEW_H
#define DIAGRAMVIEW_H

#include <QGraphicsView>

class DiagramView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DiagramView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool   m_panning = false;
    QPoint m_panStart;
};

#endif // DIAGRAMVIEW_H