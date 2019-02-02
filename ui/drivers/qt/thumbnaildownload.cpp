#include <QDir>
#include <QApplication>
#include <QProgressDialog>

#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/archive_file.h>
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../../../config.def.h"
#include "../../../configuration.h"
#include "../../../version.h"

#ifndef CXX_BUILD
}
#endif

#undef USER_AGENT
#define USER_AGENT "RetroArch-WIMP/" PACKAGE_VERSION
#define PARTIAL_EXTENSION ".partial"
#define THUMBNAIL_URL_HEADER "https://github.com/libretro-thumbnails/"
#define THUMBNAIL_URL_BRANCH "/blob/master/"
#define THUMBNAIL_IMAGE_EXTENSION ".png"
#define THUMBNAIL_URL_FOOTER THUMBNAIL_IMAGE_EXTENSION "?raw=true"

void MainWindow::onThumbnailDownloadNetworkError(QNetworkReply::NetworkError code)
{
   QNetworkReply *reply = m_thumbnailDownloadReply.data();
   QByteArray errorStringArray;
   const char *errorStringData = NULL;

   m_thumbnailDownloadProgressDialog->cancel();

   if (!reply)
      return;

   errorStringArray = reply->errorString().toUtf8();
   errorStringData = errorStringArray.constData();

   RARCH_ERR("[Qt]: Network error code %d received: %s\n", code, errorStringData);

   /* Deleting the reply here seems to cause a strange heap-use-after-free crash. */
   /*
   reply->disconnect();
   reply->abort();
   reply->deleteLater();
   */
}

void MainWindow::onThumbnailDownloadNetworkSslErrors(const QList<QSslError> &errors)
{
   QNetworkReply *reply = m_thumbnailDownloadReply.data();
   int i;

   if (!reply)
      return;

   for (i = 0; i < errors.count(); i++)
   {
      const QSslError &error = errors.at(i);
      QString string = QString("Ignoring SSL error code ") + QString::number(error.error()) + ": " + error.errorString();
      QByteArray stringArray = string.toUtf8();
      const char *stringData = stringArray.constData();
      RARCH_ERR("[Qt]: %s\n", stringData);
   }

   /* ignore all SSL errors for now, like self-signed, expired etc. */
   reply->ignoreSslErrors();
}

void MainWindow::onThumbnailDownloadCanceled()
{
   m_thumbnailDownloadProgressDialog->cancel();
}

void MainWindow::onThumbnailDownloadFinished()
{
   QString system;
   QString title;
   QString downloadType;
   QNetworkReply *reply = m_thumbnailDownloadReply.data();
   QNetworkReply::NetworkError error;
   int code;

   m_thumbnailDownloadProgressDialog->cancel();

   /* At least on Linux, the progress dialog will refuse to hide itself and will stay on screen in a corrupted way if we happen to show an error message in this function. processEvents() will sometimes fix it, other times not... seems random. */
   qApp->processEvents();

   if (!reply)
      return;

   system = reply->property("system").toString();
   title = reply->property("title").toString();
   downloadType = reply->property("download_type").toString();

   error = reply->error();
   code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

   if (m_thumbnailDownloadFile.isOpen())
      m_thumbnailDownloadFile.close();

   if (code != 200)
   {
      QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

      if (!redirectUrl.isEmpty())
      {
         QByteArray redirectUrlArray = redirectUrl.toString().toUtf8();
         const char *redirectUrlData = redirectUrlArray.constData();

         m_pendingThumbnailDownloadTypes.prepend(downloadType);

         RARCH_LOG("[Qt]: Thumbnail download got redirect with HTTP code %d: %s\n", code, redirectUrlData);

         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         downloadThumbnail(system, title, redirectUrl);

         return;
      }
      else
      {
         /*emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": HTTP Code " + QString::number(code));*/
         m_thumbnailDownloadFile.remove();

         RARCH_ERR("[Qt]: Thumbnail download failed with HTTP status code: %d\n", code);

         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         if (!m_pendingThumbnailDownloadTypes.isEmpty())
            downloadThumbnail(system, title);

         return;
      }
   }

   if (error == QNetworkReply::NoError)
   {
      int index = m_thumbnailDownloadFile.fileName().lastIndexOf(PARTIAL_EXTENSION);
      QString newFileName = m_thumbnailDownloadFile.fileName().left(index);
      QFile newFile(newFileName);

      /* rename() requires the old file to be deleted first if it exists */
      if (newFile.exists() && !newFile.remove())
         RARCH_ERR("[Qt]: Thumbnail download finished, but old file could not be deleted.\n");
      else
      {
         if (m_thumbnailDownloadFile.rename(newFileName))
         {
            RARCH_LOG("[Qt]: Thumbnail download finished successfully.\n");
            /* reload thumbnail image */
            m_playlistModel->reloadThumbnailPath(m_thumbnailDownloadFile.fileName());
            updateVisibleItems();
            emit itemChanged();
         }
         else
         {
            RARCH_ERR("[Qt]: Thumbnail download finished, but temp file could not be renamed.\n");
            emit showErrorMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE));
         }
      }
   }
   else
   {
      QByteArray errorArray = reply->errorString().toUtf8();
      const char *errorData = errorArray.constData();

      m_thumbnailDownloadFile.remove();

      RARCH_ERR("[Qt]: Thumbnail download ended prematurely: %s\n", errorData);
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": Code " + QString::number(code) + ": " + errorData);
   }

   reply->disconnect();
   reply->close();
   reply->deleteLater();

   if (!m_pendingThumbnailDownloadTypes.isEmpty())
      emit gotThumbnailDownload(system, title);
}

void MainWindow::onDownloadThumbnail(QString system, QString title)
{
   downloadThumbnail(system, title);
}

void MainWindow::onThumbnailDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
   QNetworkReply *reply = m_thumbnailDownloadReply.data();
   int progress = (bytesReceived / (float)bytesTotal) * 100.0f;

   if (!reply)
      return;

   m_thumbnailDownloadProgressDialog->setValue(progress);
}

void MainWindow::onThumbnailDownloadReadyRead()
{
   QNetworkReply *reply = m_thumbnailDownloadReply.data();

   if (!reply)
      return;

   m_thumbnailDownloadFile.write(reply->readAll());
}

void MainWindow::downloadThumbnail(QString system, QString title, QUrl url)
{
   QString systemUnderscore = system;
   QString urlString;
   QNetworkReply *reply = NULL;
   QNetworkRequest request;
   QByteArray urlArray;
   QString downloadType;
   settings_t *settings = config_get_ptr();
   const char *urlData = NULL;

   if (!settings || m_pendingThumbnailDownloadTypes.isEmpty())
      return;

   title = getScrubbedString(title);
   downloadType = m_pendingThumbnailDownloadTypes.takeFirst();

   systemUnderscore = systemUnderscore.replace(" ", "_");

   urlString = QString(THUMBNAIL_URL_HEADER) + systemUnderscore + THUMBNAIL_URL_BRANCH + downloadType + "/" + title + THUMBNAIL_URL_FOOTER;

   if (url.isEmpty())
      url = urlString;

   request.setUrl(url);

   urlArray = url.toString().toUtf8();
   urlData = urlArray.constData();

   if (m_thumbnailDownloadFile.isOpen())
   {
      RARCH_ERR("[Qt]: File is already open.\n");
      return;
   }
   else
   {
      QString dirString = QString(settings->paths.directory_thumbnails) + "/" + system + "/" + downloadType;
      QString fileName = dirString + "/" + title + THUMBNAIL_IMAGE_EXTENSION + PARTIAL_EXTENSION;
      QDir dir;
      QByteArray fileNameArray = fileName.toUtf8();
      const char *fileNameData = fileNameArray.constData();

      dir.mkpath(dirString);

      m_thumbnailDownloadFile.setFileName(fileName);

      if (!m_thumbnailDownloadFile.open(QIODevice::WriteOnly))
      {
         m_thumbnailDownloadProgressDialog->cancel();
         showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         RARCH_ERR("[Qt]: Could not open file for writing: %s\n", fileNameData);

         if (m_thumbnailDownloadReply)
         {
            m_thumbnailDownloadReply->disconnect();
            m_thumbnailDownloadReply->abort();
            m_thumbnailDownloadReply->deleteLater();
         }

         if (m_pendingThumbnailDownloadTypes.isEmpty())
            m_thumbnailDownloadProgressDialog->cancel();
         else
            downloadThumbnail(system, title);

         return;
      }
   }

   RARCH_LOG("[Qt]: Starting thumbnail download...\n");
   RARCH_LOG("[Qt]: Downloading URL %s\n", urlData);

   request.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

   m_thumbnailDownloadProgressDialog->setWindowModality(Qt::NonModal);
   m_thumbnailDownloadProgressDialog->setMinimumDuration(0);
   m_thumbnailDownloadProgressDialog->setRange(0, 100);
   m_thumbnailDownloadProgressDialog->setAutoClose(true);
   m_thumbnailDownloadProgressDialog->setAutoReset(true);
   m_thumbnailDownloadProgressDialog->setValue(0);
   m_thumbnailDownloadProgressDialog->setLabelText(QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_thumbnailDownloadProgressDialog->setCancelButtonText(tr("Cancel"));
   m_thumbnailDownloadProgressDialog->show();

   m_thumbnailDownloadReply = m_networkManager->get(request);

   reply = m_thumbnailDownloadReply.data();
   reply->setProperty("system", system);
   reply->setProperty("title", title);
   reply->setProperty("download_type", downloadType);

   /* make sure any previous connection is removed first */
   disconnect(m_thumbnailDownloadProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));
   connect(m_thumbnailDownloadProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));

   connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onThumbnailDownloadNetworkError(QNetworkReply::NetworkError)));
   connect(reply, SIGNAL(sslErrors(const QList<QSslError>&)), this, SLOT(onThumbnailDownloadNetworkSslErrors(const QList<QSslError>&)));
   connect(reply, SIGNAL(finished()), this, SLOT(onThumbnailDownloadFinished()));
   connect(reply, SIGNAL(readyRead()), this, SLOT(onThumbnailDownloadReadyRead()));
   connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onThumbnailDownloadProgress(qint64, qint64)));
}
