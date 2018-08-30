#include <QFileInfo>
#include <QListWidgetItem>
#include <QApplication>
#include <QProgressDialog>
#include <QDir>
#include <QMenu>
#include <QSettings>
#include <QInputDialog>
#include <QLayout>
#include <QScreen>

#include "../ui_qt.h"
#include "flowlayout.h"
#include "playlistentrydialog.h"

extern "C" {
#include <file/file_path.h>
#include <file/archive_file.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include "../../../file_path_special.h"
#include "../../../playlist.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../setting_list.h"
#include "../../../configuration.h"
#include "../../../core_info.h"
#include "../../../verbosity.h"
}

inline static bool comp_hash_name_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("name").toLower() < rhs.value("name").toLower();
}

inline static bool comp_hash_label_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("label").toLower() < rhs.value("label").toLower();
}

/* https://stackoverflow.com/questions/7246622/how-to-create-a-slider-with-a-non-linear-scale */
static void addDirectoryFilesToList(QStringList &list, QDir &dir)
{
   QStringList dirList = dir.entryList(QStringList(), QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
   int i;

   for (i = 0; i < dirList.count(); i++)
   {
      QString path(dir.path() + "/" + dirList.at(i));
      QFileInfo fileInfo(path);

      if (fileInfo.isDir())
      {
         QDir fileInfoDir(path);

         addDirectoryFilesToList(list, fileInfoDir);
         continue;
      }

      if (fileInfo.isFile())
      {
         list.append(fileInfo.absoluteFilePath());
      }
   }
}

void MainWindow::onPlaylistFilesDropped(QStringList files)
{
   addFilesToPlaylist(files);
}

/* Takes a list of files and folders and adds them to the currently selected playlist. Folders will have their contents added recursively. */
void MainWindow::addFilesToPlaylist(QStringList files)
{
   QStringList list;
   QString currentPlaylistPath;
   QListWidgetItem *currentItem = m_listWidget->currentItem();
   QByteArray currentPlaylistArray;
   QScopedPointer<QProgressDialog> dialog(NULL);
   PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
   QHash<QString, QString> selectedCore;
   QHash<QString, QString> itemToAdd;
   QString selectedDatabase;
   QString selectedName;
   QString selectedPath;
   const char *currentPlaylistData = NULL;
   playlist_t *playlist = NULL;
   int i;

   /* Assume a blank list means we will manually enter in all fields. */
   if (files.isEmpty())
   {
      /* Make sure hash isn't blank, that would mean there's multiple entries to add at once. */
      itemToAdd["label"] = "";
      itemToAdd["path"] = "";
   }
   else if (files.count() == 1)
   {
      QString path = files.at(0);
      QFileInfo info(path);

      if (info.isFile())
      {
         itemToAdd["label"] = info.completeBaseName();
         itemToAdd["path"] = path;
      }
   }

   if (currentItem)
   {
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();

      if (!currentPlaylistPath.isEmpty())
      {
         currentPlaylistArray = currentPlaylistPath.toUtf8();
         currentPlaylistData = currentPlaylistArray.constData();
      }
   }

   if (currentPlaylistPath == ALL_PLAYLISTS_TOKEN)
   {
      showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
      return;
   }

   /* a blank itemToAdd means there will be multiple */
   if (!playlistDialog->showDialog(itemToAdd))
      return;

   selectedName = m_playlistEntryDialog->getSelectedName();
   selectedPath = m_playlistEntryDialog->getSelectedPath();
   selectedCore = m_playlistEntryDialog->getSelectedCore();
   selectedDatabase = m_playlistEntryDialog->getSelectedDatabase();

   if (selectedDatabase.isEmpty())
      selectedDatabase = QFileInfo(currentPlaylistPath).fileName();
   else
      selectedDatabase += file_path_str(FILE_PATH_LPL_EXTENSION);

   dialog.reset(new QProgressDialog(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES), "Cancel", 0, 0, this));
   dialog->setWindowModality(Qt::ApplicationModal);

   if (selectedName.isEmpty() || selectedPath.isEmpty() ||
       selectedDatabase.isEmpty())
   {
      showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
      return;
   }

   if (files.isEmpty())
      files.append(selectedPath);

   for (i = 0; i < files.count(); i++)
   {
      QString path(files.at(i));
      QFileInfo fileInfo(path);

      if (dialog->wasCanceled())
         return;

      if (i % 25 == 0)
      {
         /* Needed to update progress dialog while doing a lot of stuff on the main thread. */
         qApp->processEvents();
      }

      if (fileInfo.isDir())
      {
         QDir dir(path);
         addDirectoryFilesToList(list, dir);
         continue;
      }

      if (fileInfo.isFile())
         list.append(fileInfo.absoluteFilePath());
      else if (files.count() == 1)
      {
         /* If adding a single file, tell user that it doesn't exist. */
         showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         return;
      }
   }

   dialog->setLabelText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST));
   dialog->setMaximum(list.count());

   playlist = playlist_init(currentPlaylistData, COLLECTION_SIZE);

   for (i = 0; i < list.count(); i++)
   {
      QString fileName = list.at(i);
      QFileInfo fileInfo;
      QByteArray fileBaseNameArray;
      QByteArray pathArray;
      QByteArray corePathArray;
      QByteArray coreNameArray;
      QByteArray databaseArray;
      const char *pathData = NULL;
      const char *fileNameNoExten = NULL;
      const char *corePathData = NULL;
      const char *coreNameData = NULL;
      const char *databaseData = NULL;

      if (dialog->wasCanceled())
      {
         playlist_free(playlist);
         return;
      }

      if (fileName.isEmpty())
         continue;

      fileInfo = fileName;

      if (files.count() == 1 && list.count() == 1 && i == 0)
      {
         fileBaseNameArray = selectedName.toUtf8();
         pathArray = QDir::toNativeSeparators(selectedPath).toUtf8();
      }
      else
      {
         fileBaseNameArray = fileInfo.completeBaseName().toUtf8();
         pathArray = QDir::toNativeSeparators(fileName).toUtf8();
      }

      fileNameNoExten = fileBaseNameArray.constData();

      /* a modal QProgressDialog calls processEvents() automatically in setValue() */
      dialog->setValue(i + 1);

      pathData = pathArray.constData();

      if (selectedCore.isEmpty())
      {
         corePathData = "DETECT";
         coreNameData = "DETECT";
      }
      else
      {
         corePathArray = QDir::toNativeSeparators(selectedCore.value("core_path")).toUtf8();
         coreNameArray = selectedCore.value("core_name").toUtf8();
         corePathData = corePathArray.constData();
         coreNameData = coreNameArray.constData();
      }

      databaseArray = selectedDatabase.toUtf8();
      databaseData = databaseArray.constData();

      if (path_is_compressed_file(pathData))
      {
         struct string_list *list = file_archive_get_file_list(pathData, NULL);

         if (list)
         {
            if (list->size == 1)
            {
               /* assume archives with one file should have that file loaded directly */
               pathArray = QDir::toNativeSeparators(QString(pathData) + "#" + list->elems[0].data).toUtf8();
               pathData = pathArray.constData();
            }

            string_list_free(list);
         }
      }

      playlist_push(playlist, pathData, fileNameNoExten,
            corePathData, coreNameData, "00000000|crc", databaseData);
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();
}

bool MainWindow::updateCurrentPlaylistEntry(const QHash<QString, QString> &contentHash)
{
   QString playlistPath = getCurrentPlaylistPath();
   QString path;
   QString label;
   QString corePath;
   QString coreName;
   QString dbName;
   QString crc32;
   QByteArray playlistPathArray;
   QByteArray pathArray;
   QByteArray labelArray;
   QByteArray corePathArray;
   QByteArray coreNameArray;
   QByteArray dbNameArray;
   QByteArray crc32Array;
   const char *playlistPathData = NULL;
   const char *pathData = NULL;
   const char *labelData = NULL;
   const char *corePathData = NULL;
   const char *coreNameData = NULL;
   const char *dbNameData = NULL;
   const char *crc32Data = NULL;
   playlist_t *playlist = NULL;
   unsigned index = 0;
   bool ok = false;

   if (playlistPath.isEmpty() || contentHash.isEmpty() || !contentHash.contains("index"))
      return false;

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return false;

   path = contentHash.value("path");
   label = contentHash.value("label");
   coreName = contentHash.value("core_name");
   corePath = contentHash.value("core_path");
   dbName = contentHash.value("db_name");
   crc32 = contentHash.value("crc32");

   if (path.isEmpty() ||
       label.isEmpty() ||
       coreName.isEmpty() ||
       corePath.isEmpty() ||
       dbName.isEmpty() ||
       crc32.isEmpty()
      )
      return false;

   playlistPathArray = playlistPath.toUtf8();
   pathArray = QDir::toNativeSeparators(path).toUtf8();
   labelArray = label.toUtf8();
   coreNameArray = coreName.toUtf8();
   corePathArray = QDir::toNativeSeparators(corePath).toUtf8();
   dbNameArray = (dbName + file_path_str(FILE_PATH_LPL_EXTENSION)).toUtf8();
   crc32Array = crc32.toUtf8();

   playlistPathData = playlistPathArray.constData();
   pathData = pathArray.constData();
   labelData = labelArray.constData();
   coreNameData = coreNameArray.constData();
   corePathData = corePathArray.constData();
   dbNameData = dbNameArray.constData();
   crc32Data = crc32Array.constData();

   if (path_is_compressed_file(pathData))
   {
      struct string_list *list = file_archive_get_file_list(pathData, NULL);

      if (list)
      {
         if (list->size == 1)
         {
            /* assume archives with one file should have that file loaded directly */
            pathArray = QDir::toNativeSeparators(QString(pathData) + "#" + list->elems[0].data).toUtf8();
            pathData = pathArray.constData();
         }

         string_list_free(list);
      }
   }

   playlist = playlist_init(playlistPathData, COLLECTION_SIZE);

   playlist_update(playlist, index, pathData, labelData,
         corePathData, coreNameData, crc32Data, dbNameData);
   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();

   return true;
}

void MainWindow::onPlaylistWidgetContextMenuRequested(const QPoint&)
{
   settings_t *settings = config_get_ptr();
   QScopedPointer<QMenu> menu;
   QScopedPointer<QMenu> associateMenu;
   QScopedPointer<QMenu> hiddenPlaylistsMenu;
   QScopedPointer<QMenu> downloadAllThumbnailsMenu;
   QScopedPointer<QAction> hideAction;
   QScopedPointer<QAction> newPlaylistAction;
   QScopedPointer<QAction> deletePlaylistAction;
   QScopedPointer<QAction> downloadAllThumbnailsEntireSystemAction;
   QScopedPointer<QAction> downloadAllThumbnailsThisPlaylistAction;
   QPointer<QAction> selectedAction;
   QPoint cursorPos = QCursor::pos();
   QListWidgetItem *selectedItem = m_listWidget->itemAt(m_listWidget->viewport()->mapFromGlobal(cursorPos));
   QDir playlistDir(settings->paths.directory_playlist);
   QString playlistDirAbsPath = playlistDir.absolutePath();
   QString currentPlaylistDirPath;
   QString currentPlaylistPath;
   QString currentPlaylistFileName;
   QFile currentPlaylistFile;
   QByteArray currentPlaylistFileNameArray;
   QFileInfo currentPlaylistFileInfo;
   QMap<QString, const core_info_t*> coreList;
   core_info_list_t *core_info_list = NULL;
   union string_list_elem_attr attr = {0};
   struct string_list *stnames = NULL;
   struct string_list *stcores = NULL;
   unsigned i = 0;
   int j = 0;
   size_t found = 0;
   const char *currentPlaylistFileNameData = NULL;
   char new_playlist_names[PATH_MAX_LENGTH];
   char new_playlist_cores[PATH_MAX_LENGTH];
   bool specialPlaylist = false;
   bool foundHiddenPlaylist = false;

   new_playlist_names[0] = new_playlist_cores[0] = '\0';

   stnames = string_split(settings->arrays.playlist_names, ";");
   stcores = string_split(settings->arrays.playlist_cores, ";");

   if (selectedItem)
   {
      currentPlaylistPath = selectedItem->data(Qt::UserRole).toString();
      currentPlaylistFile.setFileName(currentPlaylistPath);

      currentPlaylistFileInfo = QFileInfo(currentPlaylistPath);
      currentPlaylistFileName = currentPlaylistFileInfo.fileName();
      currentPlaylistDirPath = currentPlaylistFileInfo.absoluteDir().absolutePath();

      currentPlaylistFileNameArray.append(currentPlaylistFileName);
      currentPlaylistFileNameData = currentPlaylistFileNameArray.constData();
   }

   menu.reset(new QMenu(this));
   menu->setObjectName("menu");

   hiddenPlaylistsMenu.reset(new QMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS), this));
   newPlaylistAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST)) + "...", this));

   hiddenPlaylistsMenu->setObjectName("hiddenPlaylistsMenu");

   menu->addAction(newPlaylistAction.data());

   if (currentPlaylistFile.exists())
   {
      deletePlaylistAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST)) + "...", this));
      menu->addAction(deletePlaylistAction.data());
   }

   if (selectedItem)
   {
      hideAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_HIDE), this));
      menu->addAction(hideAction.data());
   }

   for (j = 0; j < m_listWidget->count(); j++)
   {
      QListWidgetItem *item = m_listWidget->item(j);
      bool hidden = m_listWidget->isItemHidden(item);

      if (hidden)
      {
         QAction *action = hiddenPlaylistsMenu->addAction(item->text());
         action->setProperty("row", j);
         action->setProperty("core_path", item->data(Qt::UserRole).toString());
         foundHiddenPlaylist = true;
      }
   }

   if (!foundHiddenPlaylist)
   {
      QAction *action = hiddenPlaylistsMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE));
      action->setProperty("row", -1);
   }

   menu->addMenu(hiddenPlaylistsMenu.data());

   /* Don't just compare strings in case there are case differences on Windows that should be ignored. */
   if (QDir(currentPlaylistDirPath) != QDir(playlistDirAbsPath))
   {
      /* special playlists like history etc. can't have an association */
      specialPlaylist = true;
   }

   if (!specialPlaylist)
   {
      associateMenu.reset(new QMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE), this));
      associateMenu->setObjectName("associateMenu");

      core_info_get_list(&core_info_list);

      for (i = 0; i < core_info_list->count && core_info_list->count > 0; i++)
      {
         const core_info_t *core = &core_info_list->list[i];
         coreList[core->core_name] = core;
      }

      {
         QMapIterator<QString, const core_info_t*> coreListIterator(coreList);
         QVector<QHash<QString, QString> > cores;

         while (coreListIterator.hasNext())
         {
            QString key;
            const core_info_t *core = NULL;
            QString name;
            QHash<QString, QString> hash;

            coreListIterator.next();

            key = coreListIterator.key();
            core = coreList.value(key);

            if (string_is_empty(core->core_name))
               name = core->display_name;
            else
               name = core->core_name;

            if (name.isEmpty())
               continue;

            hash["name"] = name;
            hash["core_path"] = core->path;

            cores.append(hash);
         }

         std::sort(cores.begin(), cores.end(), comp_hash_name_key_lower);

         for (j = 0; j < cores.count(); j++)
         {
            const QHash<QString, QString> &hash = cores.at(j);
            QAction *action = associateMenu->addAction(hash.value("name"));

            action->setProperty("core_path", hash.value("core_path"));
         }
      }

      menu->addMenu(associateMenu.data());
   }

   if (!specialPlaylist)
   {
      downloadAllThumbnailsMenu.reset(new QMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS), this));
      downloadAllThumbnailsMenu->setObjectName("downloadAllThumbnailsMenu");

      downloadAllThumbnailsThisPlaylistAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST), downloadAllThumbnailsMenu.data()));
      downloadAllThumbnailsEntireSystemAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM), downloadAllThumbnailsMenu.data()));

      downloadAllThumbnailsMenu->addAction(downloadAllThumbnailsThisPlaylistAction.data());
      downloadAllThumbnailsMenu->addAction(downloadAllThumbnailsEntireSystemAction.data());

      menu->addMenu(downloadAllThumbnailsMenu.data());
   }

   selectedAction = menu->exec(cursorPos);

   if (!selectedAction)
      goto end;

   if (!specialPlaylist && selectedAction->parent() == associateMenu.data())
   {
      found = string_list_find_elem(stnames, currentPlaylistFileNameData);

      if (found)
         string_list_set(stcores, static_cast<unsigned>(found - 1), selectedAction->property("core_path").toString().toUtf8().constData());
      else
      {
         string_list_append(stnames, currentPlaylistFileNameData, attr);
         string_list_append(stcores, "DETECT", attr);

         found = string_list_find_elem(stnames, currentPlaylistFileNameData);

         if (found)
            string_list_set(stcores, static_cast<unsigned>(found - 1), selectedAction->property("core_path").toString().toUtf8().constData());
      }

      string_list_join_concat(new_playlist_names,
            sizeof(new_playlist_names), stnames, ";");
      string_list_join_concat(new_playlist_cores,
            sizeof(new_playlist_cores), stcores, ";");

      strlcpy(settings->arrays.playlist_names,
            new_playlist_names, sizeof(settings->arrays.playlist_names));
      strlcpy(settings->arrays.playlist_cores,
            new_playlist_cores, sizeof(settings->arrays.playlist_cores));
   }
   else if (selectedItem && selectedAction == deletePlaylistAction.data())
   {
      if (currentPlaylistFile.exists())
      {
         if (showMessageBox(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST)).arg(selectedItem->text()), MainWindow::MSGBOX_TYPE_QUESTION_YESNO, Qt::ApplicationModal, false))
         {
            if (currentPlaylistFile.remove())
               reloadPlaylists();
            else
               showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         }
      }
   }
   else if (selectedAction == newPlaylistAction.data())
   {
      QString name = QInputDialog::getText(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME));
      QString newPlaylistPath = playlistDirAbsPath + "/" + name + file_path_str(FILE_PATH_LPL_EXTENSION);
      QFile file(newPlaylistPath);

      if (!name.isEmpty())
      {
         if (file.open(QIODevice::WriteOnly))
            file.close();

         reloadPlaylists();
      }
   }
   else if (selectedItem && selectedAction == hideAction.data())
   {
      int row = m_listWidget->row(selectedItem);

      if (row >= 0)
      {
         QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();

         if (!hiddenPlaylists.contains(currentPlaylistFileName))
         {
            hiddenPlaylists.append(currentPlaylistFileName);
            m_settings->setValue("hidden_playlists", hiddenPlaylists);
         }

         m_listWidget->setRowHidden(row, true);
      }
   }
   else if (selectedAction->parent() == hiddenPlaylistsMenu.data())
   {
      QVariant rowVariant = selectedAction->property("row");

      if (rowVariant.isValid())
      {
         QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();
         int row = rowVariant.toInt();

         if (row >= 0)
         {
            QString playlistPath = selectedAction->property("core_path").toString();
            QFileInfo playlistFileInfo(playlistPath);
            QString playlistFileName = playlistFileInfo.fileName();

            if (hiddenPlaylists.contains(playlistFileName))
            {
               hiddenPlaylists.removeOne(playlistFileName);
               m_settings->setValue("hidden_playlists", hiddenPlaylists);
            }

            m_listWidget->setRowHidden(row, false);
         }
      }
   }
   else if (selectedItem && !specialPlaylist && selectedAction->parent() == downloadAllThumbnailsMenu.data())
   {
      if (selectedAction == downloadAllThumbnailsEntireSystemAction.data())
      {
         int row = m_listWidget->row(selectedItem);

         if (row >= 0)
            downloadAllThumbnails(currentPlaylistFileInfo.completeBaseName());
      }
      else if (selectedAction == downloadAllThumbnailsThisPlaylistAction.data())
      {
         downloadPlaylistThumbnails(currentPlaylistPath);
      }
   }

   setCoreActions();

end:
   if (stnames)
      string_list_free(stnames);
   if (stcores)
      string_list_free(stcores);
}

void MainWindow::deferReloadPlaylists()
{
   emit gotReloadPlaylists();
}

void MainWindow::onGotReloadPlaylists()
{
   reloadPlaylists();
}

void MainWindow::reloadPlaylists()
{
   QListWidgetItem *allPlaylistsItem = NULL;
   QListWidgetItem *favoritesPlaylistsItem = NULL;
   QListWidgetItem *imagePlaylistsItem = NULL;
   QListWidgetItem *musicPlaylistsItem = NULL;
   QListWidgetItem *videoPlaylistsItem = NULL;
   QListWidgetItem *firstItem = NULL;
   QListWidgetItem *currentItem = NULL;
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);
   QString currentPlaylistPath;
   QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();
   int i = 0;

   currentItem = m_listWidget->currentItem();

   if (currentItem)
   {
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();
   }

   getPlaylistFiles();

   m_listWidget->clear();

   allPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS));
   allPlaylistsItem->setData(Qt::UserRole, ALL_PLAYLISTS_TOKEN);

   favoritesPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB));
   favoritesPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_favorites);

   m_historyPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB));
   m_historyPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_history);

   imagePlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_IMAGES_TAB));
   imagePlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_image_history);

   musicPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MUSIC_TAB));
   musicPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_music_history);

   videoPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_TAB));
   videoPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_video_history);

   m_listWidget->addItem(allPlaylistsItem);
   m_listWidget->addItem(favoritesPlaylistsItem);
   m_listWidget->addItem(m_historyPlaylistsItem);
   m_listWidget->addItem(imagePlaylistsItem);
   m_listWidget->addItem(musicPlaylistsItem);
   m_listWidget->addItem(videoPlaylistsItem);

   if (hiddenPlaylists.contains(ALL_PLAYLISTS_TOKEN))
      m_listWidget->setRowHidden(m_listWidget->row(allPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_favorites).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(favoritesPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(m_historyPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_image_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(imagePlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_music_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(musicPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_video_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(videoPlaylistsItem), true);

   for (i = 0; i < m_playlistFiles.count(); i++)
   {
      QListWidgetItem *item = NULL;
      const QString &file = m_playlistFiles.at(i);
      QString fileDisplayName = file;
      QString fileName = file;
      bool hasIcon = false;
      QIcon icon;
      QString iconPath;

      fileDisplayName.remove(file_path_str(FILE_PATH_LPL_EXTENSION));

      iconPath = QString(settings->paths.directory_assets) + ICON_PATH + fileDisplayName + ".png";

      hasIcon = QFile::exists(iconPath);

      if (hasIcon)
         icon = QIcon(iconPath);
      else
         icon = m_folderIcon;

      item = new QListWidgetItem(icon, fileDisplayName);
      item->setData(Qt::UserRole, playlistDir.absoluteFilePath(file));

      m_listWidget->addItem(item);

      if (hiddenPlaylists.contains(fileName))
      {
         int row = m_listWidget->row(item);

         if (row >= 0)
            m_listWidget->setRowHidden(row, true);
      }
   }

   if (m_listWidget->count() > 0)
   {
      firstItem = m_listWidget->item(0);

      if (firstItem)
      {
         bool foundCurrent = false;
         bool foundInitial = false;
         QString initialPlaylist = m_settings->value("initial_playlist", m_historyPlaylistsItem->data(Qt::UserRole).toString()).toString();
         QListWidgetItem *initialItem = NULL;

         for (i = 0; i < m_listWidget->count(); i++)
         {
            QListWidgetItem *item = m_listWidget->item(i);
            QString path;

            if (item)
            {
               path = item->data(Qt::UserRole).toString();

               if (!path.isEmpty())
               {
                  /* don't break early here since we want to make sure we've found both initial and current items if they exist */
                  if (!foundInitial && path == initialPlaylist)
                  {
                     foundInitial = true;
                     initialItem = item;
                  }
                  if (!foundCurrent && !currentPlaylistPath.isEmpty() && path == currentPlaylistPath)
                  {
                     foundCurrent = true;
                     m_listWidget->setCurrentItem(item);
                  }
               }
            }
         }

         if (!foundCurrent)
         {
            if (foundInitial && initialItem)
            {
               m_listWidget->setCurrentItem(initialItem);
            }
            else
            {
               /* the previous playlist must be gone now, just select the first one */
               m_listWidget->setCurrentItem(firstItem);
            }
         }
      }
   }
}

QString MainWindow::getCurrentPlaylistPath()
{
   QListWidgetItem *playlistItem = m_listWidget->currentItem();
   QHash<QString, QString> contentHash;
   QString playlistPath;

   if (!playlistItem)
      return playlistPath;

   playlistPath = playlistItem->data(Qt::UserRole).toString();

   return playlistPath;
}

void MainWindow::deleteCurrentPlaylistItem()
{
   QString playlistPath = getCurrentPlaylistPath();
   QByteArray playlistArray;
   QHash<QString, QString> contentHash = getCurrentContentHash();
   playlist_t *playlist = NULL;
   const char *playlistData = NULL;
   unsigned index = 0;
   bool ok = false;

   if (playlistPath.isEmpty())
      return;

   if (contentHash.isEmpty())
      return;

   playlistArray = playlistPath.toUtf8();
   playlistData = playlistArray.constData();

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return;

   if (!showMessageBox(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM)).arg(contentHash["label"]), MainWindow::MSGBOX_TYPE_QUESTION_YESNO, Qt::ApplicationModal, false))
      return;

   playlist = playlist_init(playlistData, COLLECTION_SIZE);

   playlist_delete_index(playlist, index);
   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();
}

QVector<QHash<QString, QString> > MainWindow::getPlaylistDefaultCores()
{
   settings_t *settings = config_get_ptr();
   struct string_list *playlists = string_split(settings->arrays.playlist_names, ";");
   struct string_list *cores = string_split(settings->arrays.playlist_cores, ";");
   unsigned i = 0;
   QVector<QHash<QString, QString> > coreList;

   if (!playlists || !cores)
   {
      RARCH_WARN("[Qt]: Could not parse one of playlist_names or playlist_cores\n");
      goto finish;
   }
   else if (playlists->size != cores->size)
   {
      RARCH_WARN("[Qt]: playlist_names array size differs from playlist_cores\n");
      goto finish;
   }

   if (playlists->size == 0)
      goto finish;

   for (i = 0; i < playlists->size; i++)
   {
      const char *playlist = playlists->elems[i].data;
      const char *core = cores->elems[i].data;
      QHash<QString, QString> hash;

      hash["playlist_filename"] = playlist;
      hash["playlist_filename"].remove(file_path_str(FILE_PATH_LPL_EXTENSION));
      hash["core_path"] = core;

      coreList.append(hash);
   }

finish:
   if (playlists)
      string_list_free(playlists);
   if (cores)
      string_list_free(cores);

   return coreList;
}

void MainWindow::getPlaylistFiles()
{
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);

   m_playlistFiles = playlistDir.entryList(QDir::NoDotAndDotDot | QDir::Readable | QDir::Files, QDir::Name);
}

void MainWindow::addPlaylistItemsToGrid(const QStringList &paths, bool add)
{
   QVector<QHash<QString, QString> > items;
   int i;

   if (paths.isEmpty())
      return;

   for (i = 0; i < paths.size(); i++)
   {
      int j;
      QVector<QHash<QString, QString> > vec = getPlaylistItems(paths.at(i));
      /* QVector::append() wasn't added until 5.5, so just do it the old fashioned way */
      for (j = 0; j < vec.size(); j++)
      {
         if (add && m_allPlaylistsGridMaxCount > 0 && items.size() >= m_allPlaylistsGridMaxCount)
            goto finish;

         items.append(vec.at(j));
      }
   }
finish:
   std::sort(items.begin(), items.end(), comp_hash_label_key_lower);

   addPlaylistHashToGrid(items);
}

void MainWindow::addPlaylistHashToGrid(const QVector<QHash<QString, QString> > &items)
{
   QScreen *screen = qApp->primaryScreen();
   QSize screenSize = screen->size();
   QListWidgetItem *currentItem = m_listWidget->currentItem();
   settings_t *settings = config_get_ptr();
   int i = 0;
   int zoomValue = m_zoomSlider->value();

   m_gridProgressBar->setMinimum(0);
   m_gridProgressBar->setMaximum(qMax(0, items.count() - 1));
   m_gridProgressBar->setValue(0);

   for (i = 0; i < items.count(); i++)
   {
      const QHash<QString, QString> &hash = items.at(i);
      QPointer<GridItem> item;
      QPointer<ThumbnailLabel> label;
      QString thumbnailFileNameNoExt;
      QLabel *newLabel = NULL;
      QSize thumbnailWidgetSizeHint(screenSize.width() / 8, screenSize.height() / 8);
      QByteArray extension;
      QString extensionStr;
      QString imagePath;
      int lastIndex = -1;

      if (m_listWidget->currentItem() != currentItem)
      {
         /* user changed the current playlist before we finished loading... abort */
         m_gridProgressWidget->hide();
         break;
      }

      item = new GridItem();

      lastIndex = hash["path"].lastIndexOf('.');

      if (lastIndex >= 0)
      {
         extensionStr = hash["path"].mid(lastIndex + 1);

         if (!extensionStr.isEmpty())
         {
            extension = extensionStr.toLower().toUtf8();
         }
      }

      if (!extension.isEmpty() && m_imageFormats.contains(extension))
      {
         /* use thumbnail widgets to show regular image files */
         imagePath = hash["path"];
      }
      else
      {
         thumbnailFileNameNoExt = hash["label_noext"];
         thumbnailFileNameNoExt.replace(m_fileSanitizerRegex, "_");
         imagePath = QString(settings->paths.directory_thumbnails) + "/" + hash.value("db_name") + "/" + THUMBNAIL_BOXART + "/" + thumbnailFileNameNoExt + ".png";
      }

      item->hash = hash;
      item->widget = new ThumbnailWidget();
      item->widget->setSizeHint(thumbnailWidgetSizeHint);
      item->widget->setFixedSize(item->widget->sizeHint());
      item->widget->setLayout(new QVBoxLayout());
      item->widget->setObjectName("thumbnailWidget");
      item->widget->setProperty("hash", QVariant::fromValue<QHash<QString, QString> >(hash));
      item->widget->setProperty("image_path", imagePath);

      connect(item->widget, SIGNAL(mouseDoubleClicked()), this, SLOT(onGridItemDoubleClicked()));
      connect(item->widget, SIGNAL(mousePressed()), this, SLOT(onGridItemClicked()));

      label = new ThumbnailLabel(item->widget);
      label->setObjectName("thumbnailGridLabel");

      item->label = label;
      item->labelText = hash.value("label");

      newLabel = new QLabel(item->labelText, item->widget);
      newLabel->setObjectName("thumbnailQLabel");
      newLabel->setAlignment(Qt::AlignCenter);
      newLabel->setToolTip(item->labelText);

      calcGridItemSize(item, zoomValue);

      item->widget->layout()->addWidget(label);

      item->widget->layout()->addWidget(newLabel);
      qobject_cast<QVBoxLayout*>(item->widget->layout())->setStretchFactor(label, 1);

      m_gridLayout->addWidgetDeferred(item->widget);
      m_gridItems.append(item);

      loadImageDeferred(item, imagePath);

      if (i % 25 == 0)
      {
         /* Needed to update progress dialog while doing a lot of stuff on the main thread. */
         qApp->processEvents();
      }

      m_gridProgressBar->setValue(i);
   }

   /* If there's only one entry, a min/max/value of all zero would make an indeterminate progress bar that never ends... so just hide it when we are done. */
   if (m_gridProgressBar->value() == m_gridProgressBar->maximum())
      m_gridProgressWidget->hide();
}

QVector<QHash<QString, QString> > MainWindow::getPlaylistItems(QString pathString)
{
   QByteArray pathArray;
   QVector<QHash<QString, QString> > items;
   const char *pathData = NULL;
   playlist_t *playlist = NULL;
   unsigned playlistSize = 0;
   unsigned i = 0;

   pathArray.append(pathString);
   pathData = pathArray.constData();

   playlist = playlist_init(pathData, COLLECTION_SIZE);
   playlistSize = playlist_get_size(playlist);

   for (i = 0; i < playlistSize; i++)
   {
      const char *path = NULL;
      const char *label = NULL;
      const char *core_path = NULL;
      const char *core_name = NULL;
      const char *crc32 = NULL;
      const char *db_name = NULL;
      QHash<QString, QString> hash;

      playlist_get_index(playlist, i,
                         &path, &label, &core_path,
                         &core_name, &crc32, &db_name);

      if (string_is_empty(path))
         continue;
      else
         hash["path"] = path;

      hash["index"] = QString::number(i);

      if (string_is_empty(label))
      {
         hash["label"] = path;
         hash["label_noext"] = path;
      }
      else
      {
         hash["label"] = label;
         hash["label_noext"] = label;
      }

      if (!string_is_empty(core_path))
         hash["core_path"] = core_path;

      if (!string_is_empty(core_name))
         hash["core_name"] = core_name;

      if (!string_is_empty(crc32))
         hash["crc32"] = crc32;

      if (!string_is_empty(db_name))
      {
         hash["db_name"] = db_name;
         hash["db_name"].remove(file_path_str(FILE_PATH_LPL_EXTENSION));
      }

      items.append(hash);
   }

   playlist_free(playlist);
   playlist = NULL;

   return items;
}

void MainWindow::addPlaylistItemsToTable(const QStringList &paths, bool add)
{
   QVector<QHash<QString, QString> > items;
   int i;

   if (paths.isEmpty())
      return;

   for (i = 0; i < paths.size(); i++)
   {
      int j;
      QVector<QHash<QString, QString> > vec = getPlaylistItems(paths.at(i));
      /* QVector::append() wasn't added until 5.5, so just do it the old fashioned way */
      for (j = 0; j < vec.size(); j++)
      {
         if (add && m_allPlaylistsListMaxCount > 0 && items.size() >= m_allPlaylistsListMaxCount)
            goto finish;

         items.append(vec.at(j));
      }
   }
finish:
   addPlaylistHashToTable(items);
}

void MainWindow::addPlaylistHashToTable(const QVector<QHash<QString, QString> > &items)
{
   int i = 0;
   int oldRowCount = m_tableWidget->rowCount();

   m_tableWidget->setRowCount(oldRowCount + items.count());

   for (i = 0; i < items.count(); i++)
   {
      QTableWidgetItem *labelItem = NULL;
      const QHash<QString, QString> &hash = items.at(i);

      labelItem = new QTableWidgetItem(hash.value("label"));
      labelItem->setData(Qt::UserRole, QVariant::fromValue<QHash<QString, QString> >(hash));
      labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);

      m_tableWidget->setItem(oldRowCount + i, 0, labelItem);
   }
}

void MainWindow::setAllPlaylistsListMaxCount(int count)
{
   if (count < 1)
      count = 0;

   m_allPlaylistsListMaxCount = count;
}

void MainWindow::setAllPlaylistsGridMaxCount(int count)
{
   if (count < 1)
      count = 0;

   m_allPlaylistsGridMaxCount = count;
}

