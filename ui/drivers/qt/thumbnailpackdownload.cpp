#include <QDir>
#include <QApplication>
#include <QProgressDialog>

#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <queues/task_queue.h>
#include <file/archive_file.h>
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../../../config.def.h"
#include "../../../configuration.h"
#include "../../../version.h"

#ifndef CXX_BUILD
}
#endif

#undef TEMP_EXTENSION
#define USER_AGENT "RetroArch-WIMP/" PACKAGE_VERSION
#define PARTIAL_EXTENSION ".partial"
#define TEMP_EXTENSION ".tmp"
#define THUMBNAILPACK_URL_HEADER "http://thumbnailpacks.libretro.com/"
#define THUMBNAILPACK_EXTENSION ".zip"

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
   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();
   QByteArray errorStringArray;
   const char *errorStringData = NULL;

   m_thumbnailPackDownloadProgressDialog->cancel();

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

void MainWindow::onThumbnailPackDownloadNetworkSslErrors(const QList<QSslError> &errors)
{
   QNetworkReply *reply = m_thumbnailPackDownloadReply.data();
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

   /* At least on Linux, the progress dialog will refuse to hide itself and will stay on screen in a corrupted way if we happen to show an error message in this function. processEvents() will sometimes fix it, other times not... seems random. */
   qApp->processEvents();

   if (!reply)
      return;

   system = reply->property("system").toString();

   error = reply->error();
   code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

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
      else
      {
         emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": HTTP Code " + QString::number(code));

         m_thumbnailPackDownloadFile.remove();

         RARCH_ERR("[Qt]: Thumbnail pack download failed with HTTP status code: %d\n", code);

         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         return;
      }
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
   QNetworkReply *reply = NULL;
   QNetworkRequest request;
   QByteArray urlArray;
   settings_t *settings = config_get_ptr();
   const char *urlData = NULL;

   if (!settings)
      return;

   urlString = QString(THUMBNAILPACK_URL_HEADER) + system + THUMBNAILPACK_EXTENSION;

   if (url.isEmpty())
      url = urlString;

   request.setUrl(url);

   urlArray = url.toString().toUtf8();
   urlData = urlArray.constData();

   if (m_thumbnailPackDownloadFile.isOpen())
   {
      RARCH_ERR("[Qt]: File is already open.\n");
      return;
   }
   else
   {
      QString dirString = QString(settings->paths.directory_thumbnails);
      QString fileName = dirString + "/" + system + THUMBNAILPACK_EXTENSION + PARTIAL_EXTENSION;
      QDir dir;
      QByteArray fileNameArray = fileName.toUtf8();
      const char *fileNameData = fileNameArray.constData();

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
   /* reload thumbnail image */
   emit itemChanged();
}
