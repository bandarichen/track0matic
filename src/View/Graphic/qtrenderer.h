#ifndef VIEW_GRAPHIC_QTRENDERER_H
#define VIEW_GRAPHIC_QTRENDERER_H

#include <memory>

#include <boost/uuid/uuid.hpp>

#include <QObject>
#include <QGraphicsEllipseItem>

class QGraphicsScene;
class QGraphicsView;
class QMainWindow;
class Track;

namespace View
{

namespace Graphic
{

class GraphicalTrack : public QGraphicsEllipseItem
{
public:
  GraphicalTrack(boost::uuids::uuid uuid,
                 qreal x, qreal y,
                 qreal width = 5, qreal height = 5);

private:
  const boost::uuids::uuid uuid_;
};

class QtRenderer : public QObject
{
  Q_OBJECT
public:
  QtRenderer(std::size_t width, std::size_t height, QObject* parent = nullptr);
  virtual ~QtRenderer();

  void show();
  void addTrack(const Track*);
  void clearScene();

signals:
  void addTrackSignal(GraphicalTrack*);
  void clearSceneSignal();

protected slots:
  void performAddTrack(GraphicalTrack*);
  void quit();

private:
  static GraphicalTrack* transformTrackFromSnapshot(const Track*);
  void drawStaticGraphics();
  void drawBackground();
  void setupMenu();

  QMainWindow* mainWindow_;
  QGraphicsScene* scene_;
  QGraphicsView* view_;
};

} // namespace Graphic

} // namespace View

#endif // VIEW_GRAPHIC_QTRENDERER_H
