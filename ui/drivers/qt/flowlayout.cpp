/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

/* Original work Copyright (C) 2016 The Qt Company Ltd.
 * Modified work Copyright (C) 2018 - Brad Parker
 */

#include <QtWidgets>

#include "flowlayout.h"
#include "../ui_qt.h"

FlowLayout::FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
   : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
   setContentsMargins(margin, margin, margin, margin);

   connect(this, SIGNAL(signalAddWidgetDeferred(QPointer<ThumbnailWidget>)), this, SLOT(onAddWidgetDeferred(QPointer<ThumbnailWidget>)), Qt::QueuedConnection);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
   : m_hSpace(hSpacing), m_vSpace(vSpacing)
{
   setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
   QLayoutItem *item = NULL;

   while ((item = takeAt(0)) != NULL)
      delete item;
}

void FlowLayout::addItem(QLayoutItem *item)
{
   itemList.append(item);
}

int FlowLayout::horizontalSpacing() const
{
   if (m_hSpace >= 0)
       return m_hSpace;
   else
      return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
   if (m_vSpace >= 0)
      return m_vSpace;
   else
      return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::count() const
{
   return itemList.size();
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
   return itemList.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index)
{
   if (index >= 0 && index < itemList.size())
      return itemList.takeAt(index);
   else
      return NULL;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
   return 0;
}

bool FlowLayout::hasHeightForWidth() const
{
   return true;
}

int FlowLayout::heightForWidth(int width) const
{
   int height = doLayout(QRect(0, 0, width, 0), true);
   return height;
}

void FlowLayout::setGeometry(const QRect &rect)
{
   QLayout::setGeometry(rect);
   doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
   return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
   QSize size;
   int i = 0;

   if (itemList.isEmpty())
      return size;

   for (i = 0; i < itemList.count(); i++)
   {
      const QLayoutItem *item = itemList.at(i);
      size = size.expandedTo(item->minimumSize());
   }

   size += QSize(2 * margin(), 2 * margin());
   return size;
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
   QRect effectiveRect;
   int left = 0, top = 0, right = 0, bottom = 0;
   int x = 0;
   int y = 0;
   int lineHeight = 0;
   int i = 0;

   getContentsMargins(&left, &top, &right, &bottom);
   effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
   x = effectiveRect.x();
   y = effectiveRect.y();

   if (itemList.isEmpty())
      return y + lineHeight - rect.y() + bottom;

   for (i = 0; i < itemList.count(); i++)
   {
      QLayoutItem *item = itemList.at(i);
      const QWidget *wid = item->widget();
      int spaceX = horizontalSpacing();
      int spaceY = 0;
      int nextX = 0;

      if (spaceX == -1)
         spaceX = wid->style()->layoutSpacing(
         QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);

      spaceY = verticalSpacing();

      if (spaceY == -1)
         spaceY = wid->style()->layoutSpacing(
            QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);

      nextX = x + item->sizeHint().width() + spaceX;

      if (nextX - spaceX > effectiveRect.right() && lineHeight > 0)
      {
         x = effectiveRect.x();
         y = y + lineHeight + spaceY;
         nextX = x + item->sizeHint().width() + spaceX;
         lineHeight = 0;
      }

      if (!testOnly)
         item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));

      x = nextX;
      lineHeight = qMax(lineHeight, item->sizeHint().height());
   }

   return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
   const QObject *parentObj = parent();

   if (!parentObj)
      return -1;
   else if (parentObj->isWidgetType())
   {
      const QWidget *pw = static_cast<const QWidget*>(parentObj);
      return pw->style()->pixelMetric(pm, NULL, pw);
   }
   else
       return static_cast<const QLayout*>(parentObj)->spacing();
}

void FlowLayout::addWidgetDeferred(QPointer<ThumbnailWidget> widget)
{
   emit signalAddWidgetDeferred(widget);
}

void FlowLayout::onAddWidgetDeferred(QPointer<ThumbnailWidget> widget)
{
   /* widget might have been deleted before we got to it since this uses a queued connection, hence the guarded QPointer */
   if (!widget)
      return;

   addWidget(widget);
}
