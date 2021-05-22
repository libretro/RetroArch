#include <QDir>
#include <QApplication>
#include <QProgressDialog>

#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <queues/task_queue.h>
#include <file/archive_file.h>

#include "../../../config.def.h"
#include "../../../configuration.h"
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../../../version.h"

#ifndef CXX_BUILD
}
#endif

#undef TEMP_EXTENSION
#undef USER_AGENT
#define USER_AGENT "RetroArch-WIMP/" PACKAGE_VERSION
#define PARTIAL_EXTENSION ".partial"
#define TEMP_EXTENSION ".tmp"
#define THUMBNAILPACK_URL_HEADER "http://thumbnailpacks.libretro.com/"
#define THUMBNAILPACK_EXTENSION ".zip"
#define THUMBNAIL_URL_HEADER "https://github.com/libretro-thumbnails/"
#define THUMBNAIL_URL_BRANCH "/blob/master/"
#define THUMBNAIL_IMAGE_EXTENSION ".png"
#define THUMBNAIL_URL_FOOTER THUMBNAIL_IMAGE_EXTENSION "?raw=true"


static void extractThumbnailPackCB(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;
   MainWindow *mainwindow = (MainWindow*)user_data;

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      if (filestream_exists(dec->source_file))
         filestream_delete(dec->source_file);

      free(dec->source_file);
      free(dec);
   }

   mainwindow->onThumbnailPackExtractFinished(string_is_empty(err));
}

void MainWindow::onThumbnailPackDownloadNetworkError(QNetworkReply::NetworkError code)
{
   QByteArray errorStringArray;
   QNetworkReply *reply        = m_thumbnailPackDownloadReply.data();
   const char *errorStringData = NULL;

   m_thumbnailPackDownloadProgressDialog->cancel();

   if (!reply)
      return;

   errorStringArray = reply->errorString().toUtf8();
   errorStringData = errorStringArray.constData();

   RARCH_ERR("[Qt]: Network error code %d received: %s\n", code, errorStringData);

#if 0
   /* Deleting the reply here seems to cause a strange 
    * heap-use-after-free crash. */
   reply->disconnect();
   reply->abort();
   reply->deleteLater();
#endif
}

void MainWindow::onThumbnailPackDownloadNetworkSslErrors(const QList<QSslError> &errors)
{
   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();
   int i;

   if (!reply)
      return;

   for (i = 0; i < errors.count(); i++)
   {
      const QSslError &error = errors.at(i);
      QString string         = 
           QString("Ignoring SSL error code ") 
         + QString::number(error.error()) 
         + ": " 
         + error.errorString();
      QByteArray stringArray = string.toUtf8();
      const char *stringData = stringArray.constData();
      RARCH_ERR("[Qt]: %s\n", stringData);
   }

   /* ignore all SSL errors for now, like self-signed, expired etc. */
   reply->ignoreSslErrors();
}

void MainWindow::onThumbnailPackDownloadCanceled()
{
   m_thumbnailPackDownloadProgressDialog->cancel();
}

void MainWindow::onThumbnailPackDownloadFinished()
{
   QString system;
   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();
   QNetworkReply::NetworkError error;
   int code;

   m_thumbnailPackDownloadProgressDialog->cancel();

   /* At least on Linux, the progress dialog will refuse 
    * to hide itself and will stay on screen in a corrupted 
    * way if we happen to show an error message in this function. 
    * processEvents() will sometimes fix it, other times not... 
    * seems random. */
   qApp->processEvents();

   if (!reply)
      return;

   system = reply->property("system").toString();

   error  = reply->error();
   code   = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

   if (m_thumbnailPackDownloadFile.isOpen())
      m_thumbnailPackDownloadFile.close();

   if (code != 200)
   {
      QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

      if (!redirectUrl.isEmpty())
      {
         QByteArray redirectUrlArray = redirectUrl.toString().toUtf8();
         const char *redirectUrlData = redirectUrlArray.constData();

         RARCH_LOG("[Qt]: Thumbnail pack download got redirect with HTTP code %d: %s\n", code, redirectUrlData);

         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         downloadAllThumbnails(system, redirectUrl);

         return;
      }

      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": HTTP Code " + QString::number(code));

      m_thumbnailPackDownloadFile.remove();

      RARCH_ERR("[Qt]: Thumbnail pack download failed with HTTP status code: %d\n", code);

      reply->disconnect();
      reply->abort();
      reply->deleteLater();

      return;
   }

   if (error == QNetworkReply::NoError)
   {
      int index = m_thumbnailPackDownloadFile.fileName().lastIndexOf(PARTIAL_EXTENSION);
      QString newFileName = m_thumbnailPackDownloadFile.fileName().left(index);
      QFile newFile(newFileName);

      /* rename() requires the old file to be deleted first if it exists */
      if (newFile.exists() && !newFile.remove())
         RARCH_ERR("[Qt]: Thumbnail pack download finished, but old file could not be deleted.\n");
      else
      {
         if (m_thumbnailPackDownloadFile.rename(newFileName))
         {
            settings_t *settings = config_get_ptr();

            if (settings)
            {
               RARCH_LOG("[Qt]: Thumbnail pack download finished successfully.\n");
               emit extractArchiveDeferred(newFileName, settings->paths.directory_thumbnails, TEMP_EXTENSION, extractThumbnailPackCB);
            }
         }
         else
         {
            RARCH_ERR("[Qt]: Thumbnail pack download finished, but temp file could not be renamed.\n");
            emit showErrorMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE));
         }
      }
   }
   else
   {
      QByteArray errorArray = reply->errorString().toUtf8();
      const char *errorData = errorArray.constData();

      m_thumbnailPackDownloadFile.remove();

      RARCH_ERR("[Qt]: Thumbnail pack download ended prematurely: %s\n", errorData);
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": Code " + QString::number(code) + ": " + errorData);
   }

   reply->disconnect();
   reply->close();
}

void MainWindow::onThumbnailPackDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();
   int progress = (bytesReceived / (float)bytesTotal) * 100.0f;

   if (!reply)
      return;

   m_thumbnailPackDownloadProgressDialog->setValue(progress);
}

void MainWindow::onThumbnailPackDownloadReadyRead()
{
   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();

   if (!reply)
      return;

   m_thumbnailPackDownloadFile.write(reply->readAll());
}

void MainWindow::downloadAllThumbnails(QString system, QUrl url)
{
   QString urlString;
   QNetworkRequest request;
   QByteArray urlArray;
   QNetworkReply *reply = NULL;
   settings_t *settings = config_get_ptr();
   const char *urlData  = NULL;

   if (!settings)
      return;

   urlString            = 
      QString(THUMBNAILPACK_URL_HEADER) 
      + system 
      + THUMBNAILPACK_EXTENSION;

   if (url.isEmpty())
      url               = urlString;

   request.setUrl(url);

   urlArray             = url.toString().toUtf8();
   urlData              = urlArray.constData();

   if (m_thumbnailPackDownloadFile.isOpen())
   {
      RARCH_ERR("[Qt]: File is already open.\n");
      return;
   }
   else
   {
      QDir dir;
      const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
      QString dirString               = QString(path_dir_thumbnails);
      QString fileName                = 
         dirString 
         + "/" 
         + system 
         + THUMBNAILPACK_EXTENSION 
         + PARTIAL_EXTENSION;
      QByteArray fileNameArray        = fileName.toUtf8();
      const char *fileNameData        = fileNameArray.constData();

      dir.mkpath(dirString);

      m_thumbnailPackDownloadFile.setFileName(fileName);

      if (!m_thumbnailPackDownloadFile.open(QIODevice::WriteOnly))
      {
         m_thumbnailPackDownloadProgressDialog->cancel();
         showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         RARCH_ERR("[Qt]: Could not open file for writing: %s\n", fileNameData);
         return;
      }
   }

   RARCH_LOG("[Qt]: Starting thumbnail pack download...\n");
   RARCH_LOG("[Qt]: Downloading URL %s\n", urlData);

   request.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

   m_thumbnailPackDownloadProgressDialog->setWindowModality(Qt::NonModal);
   m_thumbnailPackDownloadProgressDialog->setMinimumDuration(0);
   m_thumbnailPackDownloadProgressDialog->setRange(0, 100);
   m_thumbnailPackDownloadProgressDialog->setAutoClose(true);
   m_thumbnailPackDownloadProgressDialog->setAutoReset(true);
   m_thumbnailPackDownloadProgressDialog->setValue(0);
   m_thumbnailPackDownloadProgressDialog->setLabelText(QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_thumbnailPackDownloadProgressDialog->setCancelButtonText(tr("Cancel"));
   m_thumbnailPackDownloadProgressDialog->show();

   m_thumbnailPackDownloadReply = m_networkManager->get(request);

   reply = m_thumbnailPackDownloadReply.data();
   reply->setProperty("system", system);

   /* make sure any previous connection is removed first */
   disconnect(m_thumbnailPackDownloadProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));
   connect(m_thumbnailPackDownloadProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));

   connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onThumbnailPackDownloadNetworkError(QNetworkReply::NetworkError)));
   connect(reply, SIGNAL(sslErrors(const QList<QSslError>&)), this, SLOT(onThumbnailPackDownloadNetworkSslErrors(const QList<QSslError>&)));
   connect(reply, SIGNAL(finished()), this, SLOT(onThumbnailPackDownloadFinished()));
   connect(reply, SIGNAL(readyRead()), this, SLOT(onThumbnailPackDownloadReadyRead()));
   connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onThumbnailPackDownloadProgress(qint64, qint64)));
}

void MainWindow::onThumbnailPackExtractFinished(bool success)
{
   m_updateProgressDialog->cancel();

   if (!success)
   {
      RARCH_ERR("[Qt]: Thumbnail pack extraction failed.\n");
      emit showErrorMessageDeferred(msg_hash_to_str(MSG_DECOMPRESSION_FAILED));
      return;
   }

   RARCH_LOG("[Qt]: Thumbnail pack extracted successfully.\n");

   emit showInfoMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY));

   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();

   m_playlistModel->reloadSystemThumbnails(reply->property("system").toString());
   reply->deleteLater();
   updateVisibleItems();

   /* Reload thumbnail image */
   emit itemChanged();
}

void MainWindow::onThumbnailDownloadNetworkError(QNetworkReply::NetworkError code)
{
   QByteArray errorStringArray;
   QNetworkReply        *reply = m_thumbnailDownloadReply.data();
   const char *errorStringData = NULL;

   m_thumbnailDownloadProgressDialog->cancel();

   if (!reply)
      return;

   errorStringArray = reply->errorString().toUtf8();
   errorStringData = errorStringArray.constData();

   RARCH_ERR("[Qt]: Network error code %d received: %s\n",
         code, errorStringData);

   /* Deleting the reply here seems to cause a strange 
    * heap-use-after-free crash. */
   /*
   reply->disconnect();
   reply->abort();
   reply->deleteLater();
   */
}

void MainWindow::onThumbnailDownloadNetworkSslErrors(
      const QList<QSslError> &errors)
{
   QNetworkReply *reply = m_thumbnailDownloadReply.data();
   int i;

   if (!reply)
      return;

   for (i = 0; i < errors.count(); i++)
   {
      const QSslError &error = errors.at(i);
      QString         string = 
           QString("Ignoring SSL error code ") 
         + QString::number(error.error()) 
         + ": " 
         + error.errorString();
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
   int code;
   QString system;
   QString title;
   QString downloadType;
   QNetworkReply::NetworkError error;
   QNetworkReply *reply = m_thumbnailDownloadReply.data();

   m_thumbnailDownloadProgressDialog->cancel();

   /* At least on Linux, the progress dialog will refuse 
    * to hide itself and will stay on screen in a corrupted 
    * way if we happen to show an error message in this 
    * function. processEvents() will sometimes fix it, 
    * other times not... seems random. */
   qApp->processEvents();

   if (!reply)
      return;

   system       = reply->property("system").toString();
   title        = reply->property("title").toString();
   downloadType = reply->property("download_type").toString();

   error        = reply->error();
   code         = reply->attribute(
         QNetworkRequest::HttpStatusCodeAttribute).toInt();

   if (m_thumbnailDownloadFile.isOpen())
      m_thumbnailDownloadFile.close();

   if (code != 200)
   {
      QUrl redirectUrl               = 
         reply->attribute(
               QNetworkRequest::RedirectionTargetAttribute).toUrl();

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

#if 0
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": HTTP Code " + QString::number(code));
#endif
      m_thumbnailDownloadFile.remove();

      RARCH_ERR("[Qt]: Thumbnail download failed with HTTP status code: %d\n", code);

      reply->disconnect();
      reply->abort();
      reply->deleteLater();

      if (!m_pendingThumbnailDownloadTypes.isEmpty())
         downloadThumbnail(system, title);

      return;
   }

   if (error == QNetworkReply::NoError)
   {
      int index           = m_thumbnailDownloadFile.fileName().
         lastIndexOf(PARTIAL_EXTENSION);
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
      emit showErrorMessageDeferred(
              QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) 
            + ": Code " 
            + QString::number(code) 
            + ": " 
            + errorData);
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

void MainWindow::onThumbnailDownloadProgress(
      qint64 bytesReceived, qint64 bytesTotal)
{
   QNetworkReply *reply = m_thumbnailDownloadReply.data();
   int         progress = (bytesReceived / (float)bytesTotal) * 100.0f;

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
   QString urlString;
   QNetworkRequest request;
   QByteArray urlArray;
   QString downloadType;
   QString systemUnderscore = system;
   QNetworkReply *reply     = NULL;
   const char *urlData      = NULL;
   settings_t *settings     = config_get_ptr();

   if (!settings || m_pendingThumbnailDownloadTypes.isEmpty())
      return;

   title                    = getScrubbedString(title);
   downloadType             = m_pendingThumbnailDownloadTypes.takeFirst();
   systemUnderscore         = systemUnderscore.replace(" ", "_");
   urlString                = QString(THUMBNAIL_URL_HEADER) 
      + systemUnderscore 
      + THUMBNAIL_URL_BRANCH 
      + downloadType + "/" 
      + title 
      + THUMBNAIL_URL_FOOTER;

   if (url.isEmpty())
      url = urlString;

   request.setUrl(url);

   urlArray                 = url.toString().toUtf8();
   urlData                  = urlArray.constData();

   if (m_thumbnailDownloadFile.isOpen())
   {
      RARCH_ERR("[Qt]: File is already open.\n");
      return;
   }
   else
   {
      QDir dir;
      const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
      QString               dirString = QString(path_dir_thumbnails) + "/" + system + "/" + downloadType;
      QString fileName                = dirString 
         + "/" 
         + title 
         + THUMBNAIL_IMAGE_EXTENSION 
         + PARTIAL_EXTENSION;
      QByteArray fileNameArray        = fileName.toUtf8();
      const char *fileNameData        = fileNameArray.constData();

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

void MainWindow::onPlaylistThumbnailDownloadNetworkError(QNetworkReply::NetworkError /*code*/)
{
}

void MainWindow::onPlaylistThumbnailDownloadNetworkSslErrors(const QList<QSslError> &errors)
{
   QNetworkReply *reply = m_playlistThumbnailDownloadReply.data();
   int i;

   if (!reply)
      return;

   for (i = 0; i < errors.count(); i++)
   {
      const QSslError &error = errors.at(i);
      QString string         = 
           QString("Ignoring SSL error code ") 
         + QString::number(error.error()) 
         + ": " 
         + error.errorString();
      QByteArray stringArray = string.toUtf8();
      const char *stringData = stringArray.constData();
      RARCH_ERR("[Qt]: %s\n", stringData);
   }

   /* ignore all SSL errors for now, like self-signed, expired etc. */
   reply->ignoreSslErrors();
}

void MainWindow::onPlaylistThumbnailDownloadCanceled()
{
   m_playlistThumbnailDownloadProgressDialog->cancel();
   m_playlistThumbnailDownloadWasCanceled = true;
   RARCH_LOG("[Qt]: Playlist thumbnail download was canceled.\n");
}

void MainWindow::onPlaylistThumbnailDownloadFinished()
{
   int code;
   QString playlistPath;
   QNetworkReply::NetworkError error;
   QNetworkReply *reply = m_playlistThumbnailDownloadReply.data();

   if (!reply)
      return;

   playlistPath         = reply->property("playlist").toString();

   error                = reply->error();
   code                 = reply->attribute(
         QNetworkRequest::HttpStatusCodeAttribute).toInt();

   if (m_playlistThumbnailDownloadFile.isOpen())
      m_playlistThumbnailDownloadFile.close();

   if (code != 200)
   {
      QUrl redirectUrl  = reply->attribute(
            QNetworkRequest::RedirectionTargetAttribute).toUrl();

      if (!redirectUrl.isEmpty())
      {
         QByteArray redirectUrlArray = redirectUrl.toString().toUtf8();
#if 0
         const char *redirectUrlData = redirectUrlArray.constData();

         RARCH_LOG("[Qt]: Thumbnail download got redirect with"
               " HTTP code %d: %s\n", code, redirectUrlData);
#endif
         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         downloadNextPlaylistThumbnail(reply->property("system").toString(), reply->property("title").toString(), reply->property("type").toString(), redirectUrl);

         return;
      }

      m_playlistThumbnailDownloadFile.remove();

      m_failedThumbnails++;

#if 0
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": HTTP Code " + QString::number(code));

      RARCH_ERR("[Qt]: Thumbnail download failed with HTTP status code: %d\n", code);

      reply->disconnect();
      reply->abort();
      reply->deleteLater();

      return;
#endif
   }

   if (error == QNetworkReply::NoError)
   {
      int index = m_playlistThumbnailDownloadFile.fileName().lastIndexOf(PARTIAL_EXTENSION);
      QString newFileName = m_playlistThumbnailDownloadFile.fileName().left(index);
      QFile newFile(newFileName);

      /* rename() requires the old file to be deleted first if it exists */
      if (newFile.exists() && !newFile.remove())
      {
         m_failedThumbnails++;
         RARCH_ERR("[Qt]: Thumbnail download finished, but old file could not be deleted.\n");
      }
      else
      {
         /* Thumbnail download finished succesfully? */
         if (m_playlistThumbnailDownloadFile.rename(newFileName))
            m_downloadedThumbnails++;
         else
         {
#if 0
            RARCH_ERR("[Qt]: Thumbnail download finished, but temp file could not be renamed.\n");
            emit showErrorMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE));
#endif
            m_failedThumbnails++;
         }
      }
   }
   else
   {
#if 0
      QByteArray errorArray = reply->errorString().toUtf8();
      const char *errorData = errorArray.constData();
#endif

      m_playlistThumbnailDownloadFile.remove();

      m_failedThumbnails++;

#if 0
      RARCH_ERR("[Qt]: Thumbnail download ended prematurely: %s\n", errorData);
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": Code " + QString::number(code) + ": " + errorData);
#endif
   }

   m_playlistModel->reloadThumbnailPath(m_playlistThumbnailDownloadFile.fileName());

   if (!m_playlistThumbnailDownloadWasCanceled && m_pendingPlaylistThumbnails.count() > 0)
   {
      QHash<QString, QString> nextThumbnail = m_pendingPlaylistThumbnails.takeAt(0);
      ViewType viewType = getCurrentViewType();

      updateVisibleItems();
      downloadNextPlaylistThumbnail(nextThumbnail.value("db_name"), nextThumbnail.value("label_noext"), nextThumbnail.value("type"));
   }
   else
   {
      RARCH_LOG("[Qt]: Playlist thumbnails finished downloading.\n");
      /* update thumbnail */
      emit itemChanged();
   }

   reply->disconnect();
   reply->close();
   reply->deleteLater();
}

void MainWindow::onPlaylistThumbnailDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
   QNetworkReply *reply = m_playlistThumbnailDownloadReply.data();
#if 0
   int progress         = (bytesReceived / (float)bytesTotal) * 100.0f;
#endif

   if (!reply)
      return;
}

void MainWindow::onPlaylistThumbnailDownloadReadyRead()
{
   QNetworkReply *reply = m_playlistThumbnailDownloadReply.data();

   if (!reply)
      return;

   m_playlistThumbnailDownloadFile.write(reply->readAll());
}

void MainWindow::downloadNextPlaylistThumbnail(
      QString system, QString title, QString type, QUrl url)
{
   QString systemUnderscore = system;
   QString urlString;
   QNetworkRequest request;
   QNetworkReply *reply = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   title                = getScrubbedString(title);
   systemUnderscore     = systemUnderscore.replace(" ", "_");

   urlString            = 
        QString(THUMBNAIL_URL_HEADER) 
      + systemUnderscore 
      + THUMBNAIL_URL_BRANCH 
      + type 
      + "/" 
      + title 
      + THUMBNAIL_URL_FOOTER;

   if (url.isEmpty())
      url               = urlString;

   request.setUrl(url);

   if (m_playlistThumbnailDownloadFile.isOpen())
   {
      RARCH_ERR("[Qt]: File is already open.\n");
      return;
   }
   else
   {
      QString dirString = QString(settings->paths.directory_thumbnails);
      QString fileName = dirString + "/" + system + "/" + type + "/" + title + THUMBNAIL_IMAGE_EXTENSION + PARTIAL_EXTENSION;
      QByteArray fileNameArray = fileName.toUtf8();
      QDir dir;
      const char *fileNameData = fileNameArray.constData();

      dir.mkpath(dirString + "/" + system + "/" + THUMBNAIL_BOXART);
      dir.mkpath(dirString + "/" + system + "/" + THUMBNAIL_SCREENSHOT);
      dir.mkpath(dirString + "/" + system + "/" + THUMBNAIL_TITLE);

      m_playlistThumbnailDownloadFile.setFileName(fileName);

      if (!m_playlistThumbnailDownloadFile.open(QIODevice::WriteOnly))
      {
         m_failedThumbnails++;

         RARCH_ERR("[Qt]: Could not open file for writing: %s\n", fileNameData);

         if (m_pendingPlaylistThumbnails.count() > 0)
         {
            QHash<QString, QString> nextThumbnail = m_pendingPlaylistThumbnails.takeAt(0);
            downloadNextPlaylistThumbnail(nextThumbnail.value("db_name"), nextThumbnail.value("label_noext"), nextThumbnail.value("type"));
         }
         else
            m_playlistThumbnailDownloadProgressDialog->cancel();

         return;
      }
   }

#if 0
   RARCH_LOG("[Qt]: Starting thumbnail download...\n");
   RARCH_LOG("[Qt]: Downloading URL %s\n", urlData);
#endif

   request.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

   m_playlistThumbnailDownloadReply = m_networkManager->get(request);

   reply = m_playlistThumbnailDownloadReply.data();
   reply->setProperty("system", system);
   reply->setProperty("title", title);
   reply->setProperty("type", type);

   /* make sure any previous connection is removed first */
   disconnect(m_playlistThumbnailDownloadProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));
   connect(m_playlistThumbnailDownloadProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));

   connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onPlaylistThumbnailDownloadNetworkError(QNetworkReply::NetworkError)));
   connect(reply, SIGNAL(sslErrors(const QList<QSslError>&)), this, SLOT(onPlaylistThumbnailDownloadNetworkSslErrors(const QList<QSslError>&)));
   connect(reply, SIGNAL(finished()), this, SLOT(onPlaylistThumbnailDownloadFinished()));
   connect(reply, SIGNAL(readyRead()), this, SLOT(onPlaylistThumbnailDownloadReadyRead()));
   connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onPlaylistThumbnailDownloadProgress(qint64, qint64)));

   m_playlistThumbnailDownloadProgressDialog->setValue(m_playlistThumbnailDownloadProgressDialog->maximum() - m_pendingPlaylistThumbnails.count());

   {
      QString labelText  = QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...\n";
      QString labelText2 = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS)).arg(m_downloadedThumbnails).arg(m_failedThumbnails);

      labelText.append(labelText2);

      m_playlistThumbnailDownloadProgressDialog->setLabelText(labelText);
   }
}

void MainWindow::downloadPlaylistThumbnails(QString playlistPath)
{
   int i, count;
   QString system, title, type;
   QFile playlistFile(playlistPath);
   settings_t *settings = config_get_ptr();

   if (!settings || !playlistFile.exists())
      return;

   m_pendingPlaylistThumbnails.clear();
   m_downloadedThumbnails                 = 0;
   m_failedThumbnails                     = 0;
   m_playlistThumbnailDownloadWasCanceled = false;

   count = m_playlistModel->rowCount();

   if (count == 0)
      return;

   for (i = 0; i < count; i++)
   {
      QHash<QString, QString> hash;
      QHash<QString, QString> hash2;
      QHash<QString, QString> hash3;
      const QHash<QString, QString> &itemHash = m_playlistModel->index(i, 0).data(PlaylistModel::HASH).value< QHash<QString, QString> >();

      hash["db_name"]     = itemHash.value("db_name");
      hash["label_noext"] = itemHash.value("label_noext");
      hash["type"]        = THUMBNAIL_BOXART;

      hash2               = hash;
      hash3               = hash;

      hash2["type"]       = THUMBNAIL_SCREENSHOT;
      hash3["type"]       = THUMBNAIL_TITLE;

      m_pendingPlaylistThumbnails.append(hash);
      m_pendingPlaylistThumbnails.append(hash2);
      m_pendingPlaylistThumbnails.append(hash3);
   }

   m_playlistThumbnailDownloadProgressDialog->setWindowModality(Qt::NonModal);
   m_playlistThumbnailDownloadProgressDialog->setMinimumDuration(0);
   m_playlistThumbnailDownloadProgressDialog->setRange(0, m_pendingPlaylistThumbnails.count());
   m_playlistThumbnailDownloadProgressDialog->setAutoClose(true);
   m_playlistThumbnailDownloadProgressDialog->setAutoReset(true);
   m_playlistThumbnailDownloadProgressDialog->setValue(0);
   m_playlistThumbnailDownloadProgressDialog->setLabelText(QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_playlistThumbnailDownloadProgressDialog->setCancelButtonText(tr("Cancel"));
   m_playlistThumbnailDownloadProgressDialog->show();

   {
      QHash<QString, QString> firstThumbnail = m_pendingPlaylistThumbnails.takeAt(0);

      /* Start downloading the first thumbnail, 
       * the rest will download as each one finishes. */
      downloadNextPlaylistThumbnail(firstThumbnail.value("db_name"), firstThumbnail.value("label_noext"), firstThumbnail.value("type"));
   }
}
