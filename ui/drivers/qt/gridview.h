#ifndef GRIDVIEW_H
#define GRIDVIEW_H

#include <QAbstractItemView>
#include <QStyledItemDelegate>

class ThumbnailDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ThumbnailDelegate(QObject* parent = 0);
   void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex& index) const;
};

class GridView : public QAbstractItemView
{
   Q_OBJECT

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

signals:
   void visibleItemsChangedMaybe() const;

protected slots:
   void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
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
   int m_spacing = 7;
   QVector<QModelIndex> m_visibleIndexes;
   ViewMode m_viewMode = Centered;
   mutable int m_idealHeight;
   mutable QHash<int, QRectF> m_rectForRow;
   mutable bool m_hashIsDirty;
};

#endif
