#include <QScrollBar>
#include <QPainter>

#include "gridview.h"
#include "../ui_qt.h"

/* http://www.informit.com/articles/article.aspx?p=1613548 */

ThumbnailDelegate::ThumbnailDelegate(const GridItem &gridItem, QObject* parent) :
   QStyledItemDelegate(parent), m_style(gridItem)
{
}
void ThumbnailDelegate::paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex& index) const
{
   QStyleOptionViewItem opt = option;
   const QWidget *widget = opt.widget;
   QStyle *style = widget->style();
   int padding = m_style.padding;
   int textTopMargin = 4; /* Qt seemingly reports -4 the actual line height. */
   int textHeight = painter->fontMetrics().height() + padding + padding;
   QRect rect = opt.rect;
   QRect adjusted = rect.adjusted(padding, padding, -padding, -textHeight + textTopMargin);
   QPixmap pixmap = index.data(PlaylistModel::THUMBNAIL).value<QPixmap>();

   painter->save();

   initStyleOption(&opt, index);

   /* draw the background */
   style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

   /* draw the image */
   if (!pixmap.isNull())
   {
      QPixmap pixmapScaled = pixmap.scaled(adjusted.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
      style->drawItemPixmap(painter, adjusted, Qt::AlignHCenter | m_style.thumbnailVerticalAlignmentFlag, pixmapScaled);
   }

   /* draw the text */
   if (!opt.text.isEmpty())
   {
      QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
      QRect textRect = QRect(rect.x() + padding, rect.y() + adjusted.height() - textTopMargin + padding, rect.width() - 2 * padding, textHeight);
      QString elidedText = painter->fontMetrics().elidedText(opt.text, opt.textElideMode, textRect.width(), Qt::TextShowMnemonic);

      if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
         cg = QPalette::Inactive;

      if (opt.state & QStyle::State_Selected)
         painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
      else
         painter->setPen(opt.palette.color(cg, QPalette::Text));

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

   if (m_size + m_spacing * 2 > maxWidth)
   {
      m_rectForRow[0] = QRectF(x, y, m_size, m_size);

      for (row = 1; row < model()->rowCount(); ++row)
      {
         y += m_size + m_spacing;
         m_rectForRow[row] = QRectF(x, y, m_size, m_size);
      }
   }
   else
   {
      switch (m_viewMode)
      {
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
   }

   m_idealHeight = y + m_size + m_spacing;
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
   QRectF rect;
   calculateRectsIfNecessary();
   rect = m_rectForRow.value(row).toRect();
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

/* TODO: Make this more efficient by changing m_rectForRow for another data structure. Look at how Qt's own views do it. */
QModelIndex GridView::indexAt(const QPoint &point_) const
{
   QPoint point(point_);
   QHash<int, QRectF>::const_iterator i;
   point.rx() += horizontalScrollBar()->value();
   point.ry() += verticalScrollBar()->value();

   calculateRectsIfNecessary();

   i = m_rectForRow.constBegin();

   while (i != m_rectForRow.constEnd())
   {
      if (i.value().contains(point))
         return model()->index(i.key(), 0, rootIndex());
      i++;
   }
   return QModelIndex();
}

void GridView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
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

/* TODO: Maybe add a way to get the previous/next visible indexes. */
QVector<QModelIndex> GridView::visibleIndexes() const {
   return m_visibleIndexes;
}

void GridView::setSelection(const QRect &rect, QFlags<QItemSelectionModel::SelectionFlag> flags)
{
   QRect rectangle;
   QHash<int, QRectF>::const_iterator i;
   int firstRow = model()->rowCount();
   int lastRow = -1;

   calculateRectsIfNecessary();

   rectangle = rect.translated(horizontalScrollBar()->value(), verticalScrollBar()->value()).normalized();

   i = m_rectForRow.constBegin();
   while (i != m_rectForRow.constEnd())
   {
      if (i.value().intersects(rectangle))
      {
         firstRow = firstRow < i.key() ? firstRow : i.key();
         lastRow = lastRow > i.key() ? lastRow : i.key();
      }
      i++;
   }
   if (firstRow != model()->rowCount() && lastRow != -1)
   {
      QItemSelection selection(model()->index(firstRow, 0, rootIndex()), model()->index(lastRow, 0, rootIndex()));
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
   QItemSelectionRange range;
   int i;

   for (i = 0; i < selection.size(); i++)
   {
      range = selection.at(i);
      int row;
      for (row = range.top(); row <= range.bottom(); row++)
      {
         int column;
         for (column = range.left(); column < range.right(); column++)
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
      QStyleOptionViewItem option = viewOptions();

      if (!rect.isValid() || rect.bottom() < 0 || rect.y() > viewport()->height())
         continue;

      m_visibleIndexes.append(index);
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

QString GridView::getLayout() const
{
   switch (m_viewMode)
   {
      case Simple:
         return "simple";
      case Anchored:
         return "anchored";
      case Centered:
      default:
         break;
   }

   return "centered";
}

void GridView::setLayout(QString layout)
{
   if (layout == "anchored")
      m_viewMode = Anchored;
   else if (layout == "centered")
      m_viewMode = Centered;
   else if (layout == "fixed")
      m_viewMode = Simple;
}

int GridView::getSpacing() const
{
   return m_spacing;
}

void GridView::setSpacing(const int spacing)
{
   m_spacing = spacing;
}

GridItem::GridItem(QWidget* parent) : QWidget(parent)
, thumbnailVerticalAlignmentFlag(Qt::AlignBottom)
, padding(11)
{
}

int GridItem::getPadding() const
{
   return padding;
}

void GridItem::setPadding(const int value)
{
   padding = value;
}

QString GridItem::getThumbnailVerticalAlign() const
{
   switch (thumbnailVerticalAlignmentFlag)
   {
      case Qt::AlignTop:
         return "top";
      case Qt::AlignVCenter:
         return "center";
      case Qt::AlignBottom:
      default:
         break;
   }

   return "bottom";
}

void GridItem::setThumbnailVerticalAlign(const QString valign)
{
   if (valign == "top")
      thumbnailVerticalAlignmentFlag = Qt::AlignTop;
   else if (valign == "center")
      thumbnailVerticalAlignmentFlag = Qt::AlignVCenter;
   else if (valign == "bottom")
      thumbnailVerticalAlignmentFlag = Qt::AlignBottom;
}
