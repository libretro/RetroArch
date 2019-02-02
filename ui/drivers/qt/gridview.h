#ifndef GRIDVIEW_H
#define GRIDVIEW_H

#include <QAbstractItemView>
#include <QStyledItemDelegate>

#define DEFAULT_GRID_ITEM_MARGIN 11
#define DEFAULT_GRID_ITEM_THUMBNAIL_ALIGNMENT "bottom"
#define DEFAULT_GRID_SPACING 7
#define DEFAULT_GRID_LAYOUT "centered"

class GridItem;

class ThumbnailDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ThumbnailDelegate(const GridItem &gridItem, QObject* parent = 0);
   void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex& index) const;

private:
   const GridItem &m_style;
};

class GridView : public QAbstractItemView
{
   Q_OBJECT

   Q_PROPERTY(QString layout READ getLayout WRITE setLayout DESIGNABLE true SCRIPTABLE true)
   Q_PROPERTY(int spacing READ getSpacing WRITE setSpacing DESIGNABLE true SCRIPTABLE true)

public:
   enum ViewMode
   {
      Simple,
      Centered,
      Anchored
   };

   GridView(QWidget *parent = 0);
   ~GridView() {}

   QModelIndex indexAt(const QPoint &point_) const;
   QVector<QModelIndex> visibleIndexes() const;
   QRect visualRect(const QModelIndex &index) const;
   void setModel(QAbstractItemModel *model);
   void scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint);
   void setGridSize(const int newSize);
   void setviewMode(ViewMode mode);
   QString getLayout() const;
   void setLayout(QString layout);
   int getSpacing() const;
   void setSpacing(const int spacing);

signals:
   void visibleItemsChangedMaybe() const;

protected slots:
   void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());
   void rowsInserted(const QModelIndex &parent, int start, int end);
   void rowsRemoved(const QModelIndex &parent, int start, int end);
   void updateGeometries();
   void reset();

protected:
   QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
   QRegion visualRegionForSelection(const QItemSelection &selection) const;
   bool isIndexHidden(const QModelIndex&) const { return false; }
   int horizontalOffset() const;
   int verticalOffset() const;
   void scrollContentsBy(int dx, int dy);
   void setSelection(const QRect &rect, QFlags<QItemSelectionModel::SelectionFlag> flags);
   void paintEvent(QPaintEvent*);
   void resizeEvent(QResizeEvent*);

private:
   QRectF viewportRectForRow(int row) const;
   void calculateRectsIfNecessary() const;
   void refresh();

   int m_size = 255;
   int m_spacing = DEFAULT_GRID_SPACING;
   QVector<QModelIndex> m_visibleIndexes;
   ViewMode m_viewMode = Centered;
   mutable int m_idealHeight;
   mutable QHash<int, QRectF> m_rectForRow;
   mutable bool m_hashIsDirty;
};

#endif
