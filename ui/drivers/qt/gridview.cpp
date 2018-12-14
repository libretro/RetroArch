#include <QScrollBar>
#include <QPainter>

#include "gridview.h"
#include "../ui_qt.h"

/* http://www.informit.com/articles/article.aspx?p=1613548 */

ThumbnailDelegate::ThumbnailDelegate(QObject* parent) :
   QStyledItemDelegate(parent)
{
}

void ThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex& index) const
{
   painter->save();

   QStyleOptionViewItem opt = option;
   initStyleOption(&opt, index);

   const QWidget *widget = opt.widget;

   QStyle *style = widget->style();

   int margin = 11;
   int textMargin = 4;
   QRect rect = opt.rect;
   int textHeight = painter->fontMetrics().height() + margin + margin;
   QRect adjusted = rect.adjusted(margin, margin, -margin, -textHeight + textMargin);
   QPixmap pixmap = index.data(PlaylistModel::THUMBNAIL).value<QPixmap>();

   // draw the background
   style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

   // draw the image
   if (!pixmap.isNull())
   {
      QPixmap pixmapScaled = pixmap.scaled(adjusted.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
      style->drawItemPixmap(painter, adjusted, Qt::AlignHCenter | Qt::AlignBottom, pixmapScaled);
   }

   // draw the text
   if (!opt.text.isEmpty())
   {
      QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;

      if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
         cg = QPalette::Inactive;

      if (opt.state & QStyle::State_Selected)
         painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
      else
         painter->setPen(opt.palette.color(cg, QPalette::Text));

      QRect textRect = QRect(rect.x() + margin, rect.y() + adjusted.height() - textMargin + margin, rect.width() - 2 * margin, textHeight);
      QString elidedText = painter->fontMetrics().elidedText(opt.text, opt.textElideMode, textRect.width(), Qt::TextShowMnemonic);

      painter->setFont(opt.font);
      painter->drawText(textRect, Qt::AlignCenter, elidedText);
   }

   painter->restore();
}

GridView::GridView(QWidget *parent) : QAbstractItemView(parent), m_idealHeight(0), m_hashIsDirty(false)
{
   setFocusPolicy(Qt::WheelFocus);
   horizontalScrollBar()->setRange(0, 0);
   verticalScrollBar()->setRange(0, 0);
}

void GridView::setModel(QAbstractItemModel *newModel)
{
   if (model())
      disconnect(model(), SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(rowsRemoved(QModelIndex, int, int)));

   QAbstractItemView::setModel(newModel);

   connect(newModel, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(rowsRemoved(QModelIndex, int, int)));

   m_hashIsDirty = true;
}

void GridView::setviewMode(ViewMode mode)
{
   m_viewMode = mode;
}

void GridView::calculateRectsIfNecessary() const
{
   if (!m_hashIsDirty)
      return;

   int x = m_spacing;
   int y = m_spacing;
   int row;
   int nextX;

   const int maxWidth = viewport()->width();

   switch (m_viewMode) {
   case Anchored:
   {
      int columns = (maxWidth - m_spacing) / (m_size + m_spacing);
      if (columns > 0)
      {
         const int actualSpacing = (maxWidth - m_spacing - m_size - (columns - 1) * m_size) / columns;
         for (row = 0; row < model()->rowCount(); ++row)
         {
            nextX = x + m_size + actualSpacing;
            if (nextX > maxWidth)
            {
               x = m_spacing;
               y += m_size + m_spacing;
               nextX = x + m_size + actualSpacing;
            }
            m_rectForRow[row] = QRectF(x, y, m_size, m_size);
            x = nextX;
         }
         m_idealHeight = y + m_size + m_spacing;
      }
      break;
   }
   case Centered:
   {
      int columns = (maxWidth - m_spacing) / (m_size + m_spacing);
      if (columns > 0)
      {
         const int actualSpacing = (maxWidth - columns * m_size) / (columns + 1);
         x = actualSpacing;
         for (row = 0; row < model()->rowCount(); ++row)
         {
            nextX = x + m_size + actualSpacing;
            if (nextX > maxWidth)
            {
               x = actualSpacing;
               y += m_size + m_spacing;
               nextX = x + m_size + actualSpacing;
            }
            m_rectForRow[row] = QRectF(x, y, m_size, m_size);
            x = nextX;
         }
         m_idealHeight = y + m_size + m_spacing;
      }
      break;
   }
   case Simple:
      for (row = 0; row < model()->rowCount(); ++row)
      {
         nextX = x + m_size + m_spacing;
         if (nextX > maxWidth)
         {
            x = m_spacing;
            y += m_size + m_spacing;
            nextX = x + m_size + m_spacing;
         }
         m_rectForRow[row] = QRectF(x, y, m_size, m_size);
         x = nextX;
      }
      break;
   }

   m_hashIsDirty = false;
   viewport()->update();
}

QRect GridView::visualRect(const QModelIndex &index) const
{
   QRect rect;
   if (index.isValid())
      rect = viewportRectForRow(index.row()).toRect();
   return rect;
}

QRectF GridView::viewportRectForRow(int row) const
{
   calculateRectsIfNecessary();
   QRectF rect = m_rectForRow.value(row).toRect();
   if (!rect.isValid())
      return rect;
   return QRectF(rect.x() - horizontalScrollBar()->value(), rect.y() - verticalScrollBar()->value(), rect.width(), rect.height());
}

void GridView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint)
{
   QRect viewRect = viewport()->rect();
   QRect itemRect = visualRect(index);

   if (itemRect.left() < viewRect.left())
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() + itemRect.left() - viewRect.left());
   else if (itemRect.right() > viewRect.right())
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() + qMin(itemRect.right() - viewRect.right(), itemRect.left() - viewRect.left()));
   if (itemRect.top() < viewRect.top())
      verticalScrollBar()->setValue(verticalScrollBar()->value() + itemRect.top() - viewRect.top());
   else if (itemRect.bottom() > viewRect.bottom())
      verticalScrollBar()->setValue(verticalScrollBar()->value() + qMin(itemRect.bottom() - viewRect.bottom(), itemRect.top() - viewRect.top()));
   viewport()->update();
}

QModelIndex GridView::indexAt(const QPoint &point_) const
{
   QPoint point(point_);
   point.rx() += horizontalScrollBar()->value();
   point.ry() += verticalScrollBar()->value();
   calculateRectsIfNecessary();
   QHashIterator<int, QRectF> i(m_rectForRow);
   while (i.hasNext())
   {
      i.next();
      if (i.value().contains(point))
         return model()->index(i.key(), 0, rootIndex());
   }
   return QModelIndex();
}

void GridView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
   m_hashIsDirty = true;
   QAbstractItemView::dataChanged(topLeft, bottomRight);
}

void GridView::refresh()
{
   m_hashIsDirty = true;
   calculateRectsIfNecessary();
   updateGeometries();
}

void GridView::rowsInserted(const QModelIndex &parent, int start, int end)
{
   QAbstractItemView::rowsInserted(parent, start, end);
   refresh();
}

void GridView::rowsRemoved(const QModelIndex &parent, int start, int end)
{
   refresh();
}

void GridView::setGridSize(const int newSize)
{
   if (newSize != m_size)
   {
      m_size = newSize;
      refresh();
   }
}

void GridView::resizeEvent(QResizeEvent*)
{
   refresh();
}

void GridView::reset()
{
   m_visibleIndexes.clear();
   QAbstractItemView::reset();
   refresh();
}

QModelIndex GridView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers)
{
   QModelIndex index = currentIndex();
   if (index.isValid())
   {
      if ((cursorAction == MoveLeft && index.row() > 0) || (cursorAction == MoveRight && index.row() + 1 < model()->rowCount()))
      {
         const int offset = (cursorAction == MoveLeft ? -1 : 1);
         index = model()->index(index.row() + offset, index.column(), index.parent());
      }
      else if ((cursorAction == MoveUp && index.row() > 0) || (cursorAction == MoveDown && index.row() + 1 < model()->rowCount()))
      {
         const int offset = ((m_size + m_spacing) * (cursorAction == MoveUp ? -1 : 1));
         QRect rect = viewportRectForRow(index.row()).toRect();
         QPoint point(rect.center().x(), rect.center().y() + offset);
         index = indexAt(point);
      }
   }
   return index;
}

int GridView::horizontalOffset() const
{
   return horizontalScrollBar()->value();
}

int GridView::verticalOffset() const
{
   return verticalScrollBar()->value();
}

void GridView::scrollContentsBy(int dx, int dy)
{
   scrollDirtyRegion(dx, dy);
   viewport()->scroll(dx, dy);
   emit(visibleItemsChangedMaybe());
}

QVector<QModelIndex> GridView::visibleIndexes() const {
   return m_visibleIndexes;
}

void GridView::setSelection(const QRect &rect, QFlags<QItemSelectionModel::SelectionFlag> flags)
{
   QRect rectangle = rect.translated(horizontalScrollBar()->value(), verticalScrollBar()->value()).normalized();
   calculateRectsIfNecessary();
   QHashIterator<int, QRectF> i(m_rectForRow);
   int firstRow = model()->rowCount();
   int lastRow = -1;
   while (i.hasNext())
   {
      i.next();
      if (i.value().intersects(rectangle))
      {
         firstRow = firstRow < i.key() ? firstRow : i.key();
         lastRow = lastRow > i.key() ? lastRow : i.key();
      }
   }
   if (firstRow != model()->rowCount() && lastRow != -1)
   {
      QItemSelection selection( model()->index(firstRow, 0, rootIndex()), model()->index(lastRow, 0, rootIndex()));
      selectionModel()->select(selection, flags);
   }
   else
   {
      QModelIndex invalid;
      QItemSelection selection(invalid, invalid);
      selectionModel()->select(selection, flags);
   }
}

QRegion GridView::visualRegionForSelection(const QItemSelection &selection) const
{
   QRegion region;
   foreach(const QItemSelectionRange &range, selection)
   {
      for (int row = range.top(); row <= range.bottom(); ++row)
      {
         for (int column = range.left(); column < range.right(); ++column)
         {
            QModelIndex index = model()->index(row, column, rootIndex());
            region += visualRect(index);
         }
      }
   }
   return region;
}

void GridView::paintEvent(QPaintEvent*)
{
   QPainter painter(viewport());
   int row;

   painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
   m_visibleIndexes.clear();

   for (row = 0; row < model()->rowCount(rootIndex()); ++row)
   {
      QModelIndex index = model()->index(row, 0, rootIndex());
      QRectF rect = viewportRectForRow(row);

      if (!rect.isValid() || rect.bottom() < 0 || rect.y() > viewport()->height())
         continue;

      m_visibleIndexes.append(index);
      QStyleOptionViewItem option = viewOptions();
      option.rect = rect.toRect();

      if (selectionModel()->isSelected(index))
         option.state |= QStyle::State_Selected;

      if (currentIndex() == index)
         option.state |= QStyle::State_HasFocus;

      itemDelegate()->paint(&painter, option, index);
   }
}

void GridView::updateGeometries()
{
   const int RowHeight = m_size + m_spacing;

   QAbstractItemView::updateGeometries();

   verticalScrollBar()->setSingleStep(RowHeight);
   verticalScrollBar()->setPageStep(viewport()->height());
   verticalScrollBar()->setRange(0, qMax(0, m_idealHeight - viewport()->height()));

   horizontalScrollBar()->setPageStep(viewport()->width());
   horizontalScrollBar()->setRange(0, qMax(0, RowHeight - viewport()->width()));

   emit(visibleItemsChangedMaybe());
}
