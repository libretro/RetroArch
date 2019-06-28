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
#include <queues/task_queue.h>
#include "../../../tasks/tasks_internal.h"
#include "../../../verbosity.h"
#include "../../../config.def.h"

#ifndef CXX_BUILD
}
#endif

#define USER_AGENT "RetroArch-WIMP/1.0"
#define PARTIAL_EXTENSION ".partial"
#define TEMP_EXTENSION ".update_tmp"
#define RETROARCH_NIGHTLY_UPDATE_PATH "../RetroArch_update.zip"

static void extractUpdateCB(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;
   MainWindow      *mainwindow = (MainWindow*)user_data;

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      if (filestream_exists(dec->source_file))
         filestream_delete(dec->source_file);

      free(dec->source_file);
      free(dec);
   }

   mainwindow->onUpdateRetroArchFinished(string_is_empty(err));
}

void MainWindow::removeUpdateTempFiles()
{
   /* a QDir with no path means the current working directory */
   QDir dir;
   QStringList dirList = dir.entryList(QStringList(), QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
   int i;

   for (i = 0; i < dirList.count(); i++)
   {
      QString path(dir.path() + "/" + dirList.at(i));
      QFile file(path);

      if (path.endsWith(TEMP_EXTENSION))
      {
         QByteArray pathArray = path.toUtf8();
         const char *pathData = pathArray.constData();

         if (file.remove())
            RARCH_LOG("[Qt]: removed temporary update file %s\n", pathData);
         else
            RARCH_LOG("[Qt]: could not remove temporary update file %s\n", pathData);
      }
   }
}

void MainWindow::onUpdateNetworkError(QNetworkReply::NetworkError code)
{
   QNetworkReply *reply = m_updateReply.data();
   QByteArray errorStringArray;
   const char *errorStringData = NULL;

   m_updateProgressDialog->cancel();

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

void MainWindow::onUpdateNetworkSslErrors(const QList<QSslError> &errors)
{
   QNetworkReply *reply = m_updateReply.data();
   int i;

   if (!reply)
      return;

   for (i = 0; i < errors.count(); i++)
   {
      const QSslError &error = errors.at(i);
      QString         string = QString("Ignoring SSL error code ") + QString::number(error.error()) + ": " + error.errorString();
      QByteArray stringArray = string.toUtf8();
      const char *stringData = stringArray.constData();

      RARCH_ERR("[Qt]: %s\n", stringData);
   }

   /* ignore all SSL errors for now, like self-signed, expired etc. */
   reply->ignoreSslErrors();
}

void MainWindow::onUpdateDownloadCanceled()
{
   m_updateProgressDialog->cancel();
}

void MainWindow::onRetroArchUpdateDownloadFinished()
{
   QNetworkReply *reply = m_updateReply.data();
   QNetworkReply::NetworkError error;
   int code;

   m_updateProgressDialog->cancel();

   /* At least on Linux, the progress dialog will refuse to hide itself and 
    * will stay onscreen in a corrupted way if we happen to show an error 
    * message in this function. processEvents() will sometimes fix it, 
    * other times not... seems random. */
   qApp->processEvents();

   if (!reply)
      return;

   error = reply->error();
   code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

   if (m_updateFile.isOpen())
      m_updateFile.close();

   if (code != 200)
   {
      emit showErrorMessageDeferred(QString(
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) 
            + ": HTTP Code " + QString::number(code));
      RARCH_ERR("[Qt]: RetroArch update failed with HTTP status code: %d\n", code);
      reply->disconnect();
      reply->abort();
      reply->deleteLater();
      return;
   }

   if (error == QNetworkReply::NoError)
   {
      int index = m_updateFile.fileName().lastIndexOf(PARTIAL_EXTENSION);
      QString newFileName = m_updateFile.fileName().left(index);
      QFile newFile(newFileName);

      /* rename() requires the old file to be deleted first if it exists */
      if (newFile.exists() && !newFile.remove())
         RARCH_ERR("[Qt]: RetroArch update finished, but old file could not be deleted.\n");
      else
      {
         if (m_updateFile.rename(newFileName))
         {
            RARCH_LOG("[Qt]: RetroArch update finished downloading successfully.\n");
            emit extractArchiveDeferred(newFileName, ".", TEMP_EXTENSION, extractUpdateCB);
         }
         else
         {
            RARCH_ERR("[Qt]: RetroArch update finished, but temp file could not be renamed.\n");
            emit showErrorMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE));
         }
      }
   }
   else
   {
      QByteArray errorArray = reply->errorString().toUtf8();
      const char *errorData = errorArray.constData();

      m_updateFile.remove();

      RARCH_ERR("[Qt]: RetroArch update ended prematurely: %s\n", errorData);
      emit showErrorMessageDeferred(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)) + ": Code " + QString::number(code) + ": " + errorData);
   }

   reply->disconnect();
   reply->close();
   reply->deleteLater();
}

void MainWindow::onUpdateRetroArchFinished(bool success)
{
   m_updateProgressDialog->cancel();

   if (!success)
   {
      RARCH_ERR("[Qt]: RetroArch update failed.\n");
      emit showErrorMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED));
      return;
   }

   RARCH_LOG("[Qt]: RetroArch update finished successfully.\n");

   emit showInfoMessageDeferred(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED));
}

void MainWindow::onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
   QNetworkReply *reply = m_updateReply.data();
   int progress = (bytesReceived / (float)bytesTotal) * 100.0f;

   if (!reply)
      return;

   m_updateProgressDialog->setValue(progress);
}

void MainWindow::onUpdateDownloadReadyRead()
{
   QNetworkReply *reply = m_updateReply.data();

   if (!reply)
      return;

   m_updateFile.write(reply->readAll());
}

void MainWindow::updateRetroArchNightly()
{
   QUrl url(QUrl(buildbot_server_url).resolved(QUrl(RETROARCH_NIGHTLY_UPDATE_PATH)));
   QNetworkRequest request(url);
   QNetworkReply *reply = NULL;
   QByteArray urlArray = url.toString().toUtf8();
   const char *urlData = urlArray.constData();

   if (m_updateFile.isOpen())
   {
      RARCH_ERR("[Qt]: File is already open.\n");
      return;
   }
   else
   {
      QString fileName = QFileInfo(url.toString()).fileName() + PARTIAL_EXTENSION;
      QByteArray fileNameArray = fileName.toUtf8();
      const char *fileNameData = fileNameArray.constData();

      m_updateFile.setFileName(fileName);

      if (!m_updateFile.open(QIODevice::WriteOnly))
      {
         showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         RARCH_ERR("[Qt]: Could not open file for writing: %s\n", fileNameData);
         return;
      }
   }

   RARCH_LOG("[Qt]: Starting update of RetroArch...\n");
   RARCH_LOG("[Qt]: Downloading URL %s\n", urlData);

   request.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);

   m_updateProgressDialog->setWindowModality(Qt::NonModal);
   m_updateProgressDialog->setMinimumDuration(0);
   m_updateProgressDialog->setRange(0, 100);
   m_updateProgressDialog->setAutoClose(true);
   m_updateProgressDialog->setAutoReset(true);
   m_updateProgressDialog->setValue(0);
   m_updateProgressDialog->setLabelText(QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_updateProgressDialog->setCancelButtonText(tr("Cancel"));
   m_updateProgressDialog->show();

   m_updateReply = m_networkManager->get(request);

   reply = m_updateReply.data();

   /* make sure any previous connection is removed first */
   disconnect(m_updateProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));
   disconnect(m_updateProgressDialog, SIGNAL(canceled()), m_updateProgressDialog, SLOT(cancel()));
   connect(m_updateProgressDialog, SIGNAL(canceled()), reply, SLOT(abort()));
   connect(m_updateProgressDialog, SIGNAL(canceled()), m_updateProgressDialog, SLOT(cancel()));

   connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onUpdateNetworkError(QNetworkReply::NetworkError)));
   connect(reply, SIGNAL(sslErrors(const QList<QSslError>&)), this, SLOT(onUpdateNetworkSslErrors(const QList<QSslError>&)));
   connect(reply, SIGNAL(finished()), this, SLOT(onRetroArchUpdateDownloadFinished()));
   connect(reply, SIGNAL(readyRead()), this, SLOT(onUpdateDownloadReadyRead()));
   connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onUpdateDownloadProgress(qint64, qint64)));

}
