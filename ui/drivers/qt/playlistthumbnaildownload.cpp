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

#define USER_AGENT "RetroArch-WIMP/" PACKAGE_VERSION
#define PARTIAL_EXTENSION ".partial"
#define THUMBNAIL_URL_HEADER "https://github.com/libretro-thumbnails/"
#define THUMBNAIL_URL_BRANCH "/blob/master/"
#define THUMBNAIL_IMAGE_EXTENSION ".png"
#define THUMBNAIL_URL_FOOTER THUMBNAIL_IMAGE_EXTENSION "?raw=true"

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
      QString string = QString("Ignoring SSL error code ") + QString::number(error.error()) + ": " + error.errorString();
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
   QString playlistPath;
   QNetworkReply *reply = m_playlistThumbnailDownloadReply.data();
   QNetworkReply::NetworkError error;
   int code;

   if (!reply)
      return;

   playlistPath = reply->property("playlist").toString();

   error = reply->error();
   code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

   if (m_playlistThumbnailDownloadFile.isOpen())
      m_playlistThumbnailDownloadFile.close();

   if (code != 200)
   {
      QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();

      if (!redirectUrl.isEmpty())
      {
         QByteArray redirectUrlArray = redirectUrl.toString().toUtf8();
#if 0
         const char *redirectUrlData = redirectUrlArray.constData();

         /*RARCH_LOG("[Qt]: Thumbnail download got redirect with HTTP code %d: %s\n", code, redirectUrlData);*/
#endif
         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         downloadNextPlaylistThumbnail(reply->property("system").toString(), reply->property("title").toString(), reply->property("type").toString(), redirectUrl);

         return;
      }
      else
      {
         m_playlistThumbnailDownloadFile.remove();

         m_failedThumbnails++;

         /*emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": HTTP Code " + QString::number(code));

         RARCH_ERR("[Qt]: Thumbnail download failed with HTTP status code: %d\n", code);

         reply->disconnect();
         reply->abort();
         reply->deleteLater();

         return;*/
      }
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
         if (m_playlistThumbnailDownloadFile.rename(newFileName))
         {
            /*RARCH_LOG("[Qt]: Thumbnail download finished successfully.\n");*/
            m_downloadedThumbnails++;
         }
         else
         {
            /*RARCH_ERR("[Qt]: Thumbnail download finished, but temp file could not be renamed.\n");
            emit showErrorMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE));*/
            m_failedThumbnails++;
         }
      }
   }
   else
   {
      /*QByteArray errorArray = reply->errorString().toUtf8();
      const char *errorData = errorArray.constData();*/

      m_playlistThumbnailDownloadFile.remove();

      m_failedThumbnails++;

      /*RARCH_ERR("[Qt]: Thumbnail download ended prematurely: %s\n", errorData);
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": Code " + QString::number(code) + ": " + errorData);*/
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
   int progress = (bytesReceived / (float)bytesTotal) * 100.0f;
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

void MainWindow::downloadNextPlaylistThumbnail(QString system, QString title, QString type, QUrl url)
{
   QString systemUnderscore = system;
   QString urlString;
   QNetworkReply *reply = NULL;
   QNetworkRequest request;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   title = getScrubbedString(title);
   systemUnderscore = systemUnderscore.replace(" ", "_");

   urlString = QString(THUMBNAIL_URL_HEADER) + systemUnderscore + THUMBNAIL_URL_BRANCH + type + "/" + title + THUMBNAIL_URL_FOOTER;

   if (url.isEmpty())
      url = urlString;

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

   /*RARCH_LOG("[Qt]: Starting thumbnail download...\n");
   RARCH_LOG("[Qt]: Downloading URL %s\n", urlData);*/

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
      QString labelText = QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...\n";

      labelText += QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS)).arg(m_downloadedThumbnails).arg(m_failedThumbnails);

      m_playlistThumbnailDownloadProgressDialog->setLabelText(labelText);
   }
}

void MainWindow::downloadPlaylistThumbnails(QString playlistPath)
{
   QFile playlistFile(playlistPath);
   QString system, title, type;
   settings_t *settings = config_get_ptr();
   int i;
   int count;

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
      const QHash<QString, QString> &itemHash = m_playlistModel->index(i, 0).data(PlaylistModel::HASH).value< QHash<QString, QString> >();
      QHash<QString, QString> hash;
      QHash<QString, QString> hash2;
      QHash<QString, QString> hash3;

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

      /* Start downloading the first thumbnail, the rest will download as each one finishes. */
      downloadNextPlaylistThumbnail(firstThumbnail.value("db_name"), firstThumbnail.value("label_noext"), firstThumbnail.value("type"));
   }
}
