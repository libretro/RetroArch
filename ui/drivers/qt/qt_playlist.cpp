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
#include <QRegularExpression>
#include <QImageReader>
#include <QtConcurrent>

#include "../ui_qt.h"
#include "playlistentrydialog.h"

#ifndef CXX_BUILD
extern "C" {
#endif

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

#ifndef CXX_BUILD
}
#endif

PlaylistModel::PlaylistModel(QObject *parent)
   : QAbstractListModel(parent)
{
   m_imageFormats = QVector<QByteArray>::fromList(QImageReader::supportedImageFormats());
   m_fileSanitizerRegex = QRegularExpression("[&*/:`<>?\\|]");
   setThumbnailCacheLimit(500);
   connect(this, &PlaylistModel::imageLoaded, this, &PlaylistModel::onImageLoaded);
}

int PlaylistModel::rowCount(const QModelIndex & /* parent */) const
{
   return m_contents.count();
}

int PlaylistModel::columnCount(const QModelIndex & /* parent */) const
{
   return 1;
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
   if (index.column() == 0)
   {
      if (!index.isValid())
         return QVariant();

      if (index.row() >= m_contents.size() || index.row() < 0)
         return QVariant();

      switch (role)
      {
         case Qt::DisplayRole:
         case Qt::EditRole:
         case Qt::ToolTipRole:
            return m_contents.at(index.row())["label_noext"];
         case HASH:
            return QVariant::fromValue(m_contents.at(index.row()));
         case THUMBNAIL:
         {
            QPixmap *cachedPreview = m_cache.object(getCurrentTypeThumbnailPath(index));
            if (cachedPreview)
               return *cachedPreview;
            return QVariant();
         }
      }
   }
   return QVariant();
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
   if (!index.isValid())
      return Qt::ItemIsEnabled;

   return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool PlaylistModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
   if (index.isValid() && role == Qt::EditRole)
   {
      QHash<QString, QString> hash = m_contents.at(index.row());

      hash["label"] = value.toString();
      hash["label_noext"] = QFileInfo(value.toString()).completeBaseName();

      m_contents.replace(index.row(), hash);
      emit dataChanged(index, index, { role });
      return true;
   }
   return false;
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (role != Qt::DisplayRole)
      return QVariant();

   if (orientation == Qt::Horizontal)
      return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);
   return section + 1;
}

void PlaylistModel::setThumbnailType(const ThumbnailType type)
{
   m_thumbnailType = type;
}

void PlaylistModel::setThumbnailCacheLimit(int limit)
{
   m_cache.setMaxCost(limit * 1024);
}

QString PlaylistModel::getThumbnailPath(const QModelIndex &index, QString type) const
{
   return getThumbnailPath(m_contents.at(index.row()), type);
}

QString PlaylistModel::getPlaylistThumbnailsDir(const QString playlistName) const
{
   return QDir::cleanPath(QString(config_get_ptr()->paths.directory_thumbnails)) + "/" + playlistName;
}

bool PlaylistModel::isSupportedImage(const QString path) const
{
   int lastIndex = -1;
   QByteArray extension;
   QString extensionStr;

   lastIndex = path.lastIndexOf('.');

   if (lastIndex >= 0)
   {
      extensionStr = path.mid(lastIndex + 1);

      if (!extensionStr.isEmpty())
         extension = extensionStr.toLower().toUtf8();
   }

   if (!extension.isEmpty() && m_imageFormats.contains(extension))
      return true;

   return false;
}

QString PlaylistModel::getSanitizedThumbnailName(QString label) const
{
   return label.replace(m_fileSanitizerRegex, "_") + ".png";
}

QString PlaylistModel::getThumbnailPath(const QHash<QString, QString> &hash, QString type) const
{
   if (isSupportedImage(hash["path"]))
   {
      /* use thumbnail widgets to show regular image files */
      return hash["path"];
   }

   return getPlaylistThumbnailsDir(hash.value("db_name")) 
      + "/" + type + "/" + getSanitizedThumbnailName(hash["label_noext"]);
}

QString PlaylistModel::getCurrentTypeThumbnailPath(const QModelIndex &index) const
{
   switch (m_thumbnailType)
   {
      case THUMBNAIL_TYPE_BOXART:
         return getThumbnailPath(index, THUMBNAIL_BOXART);
      case THUMBNAIL_TYPE_SCREENSHOT:
         return getThumbnailPath(index, THUMBNAIL_SCREENSHOT);
      case THUMBNAIL_TYPE_TITLE_SCREEN:
         return getThumbnailPath(index, THUMBNAIL_TITLE);
      default:
         break;
   }

   return QString();
}

void PlaylistModel::reloadThumbnail(const QModelIndex &index)
{
   if (index.isValid())
   {
      reloadThumbnailPath(getCurrentTypeThumbnailPath(index));
      loadThumbnail(index);
   }
}

void PlaylistModel::reloadSystemThumbnails(const QString system)
{
   int i = 0;
   QString key;
   QString path = QDir::cleanPath(QString(config_get_ptr()->paths.directory_thumbnails)) + "/" + system;
   QList<QString> keys = m_cache.keys();
   QList<QString> pending = m_pendingImages.values();

   for (i = 0; i < keys.size(); i++)
   {
      key = keys.at(i);
      if (key.startsWith(path))
         m_cache.remove(key);
   }

   for (i = 0; i < pending.size(); i++)
   {
      key = pending.at(i);
      if (key.startsWith(path))
         m_pendingImages.remove(key);
   }
}

void PlaylistModel::reloadThumbnailPath(const QString path)
{
   m_cache.remove(path);
   m_pendingImages.remove(path);
}

void PlaylistModel::loadThumbnail(const QModelIndex &index)
{
   QString path = getCurrentTypeThumbnailPath(index);

   if (!m_pendingImages.contains(path) && !m_cache.contains(path))
   {
      m_pendingImages.insert(path);
      QtConcurrent::run(this, &PlaylistModel::loadImage, index, path);
   }
}

void PlaylistModel::loadImage(const QModelIndex &index, const QString &path)
{
   const QImage image = QImage(path);
   if (!image.isNull())
      emit imageLoaded(image, index, path);
}

void PlaylistModel::onImageLoaded(const QImage image, const QModelIndex &index, const QString &path)
{
   QPixmap *pixmap = new QPixmap(QPixmap::fromImage(image));
   const int cost = pixmap->width() * pixmap->height() * pixmap->depth() / (8 * 1024);
   m_cache.insert(path, pixmap, cost);
   if (index.isValid())
      emit dataChanged(index, index, { THUMBNAIL });
   m_pendingImages.remove(path);
}

inline static bool comp_hash_name_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("name").toLower() < rhs.value("name").toLower();
}

bool MainWindow::addDirectoryFilesToList(QProgressDialog *dialog,
      QStringList &list, QDir &dir, QStringList &extensions)
{
   PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
   QStringList dirList = dir.entryList(QStringList(), QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
   int i;

   for (i = 0; i < dirList.count(); i++)
   {
      QString path(dir.path() + "/" + dirList.at(i));
      QByteArray pathArray = path.toUtf8();
      QFileInfo fileInfo(path);
      const char *pathData = pathArray.constData();

      if (dialog->wasCanceled())
         return false;

      /* Needed to update progress dialog while doing 
       * a lot of stuff on the main thread. */
      if (i % 25 == 0)
         qApp->processEvents();

      if (fileInfo.isDir())
      {
         QDir fileInfoDir(path);
         bool success = addDirectoryFilesToList(
               dialog, list, fileInfoDir, extensions);

         if (!success)
            return false;

         continue;
      }

      if (fileInfo.isFile())
      {
         bool add = false;

         if (extensions.isEmpty())
            add = true;
         else
         {
            if (extensions.contains(fileInfo.suffix()))
               add = true;
            else
            {
               if (path_is_compressed_file(pathData))
               {
                  struct string_list *archive_list = 
                     file_archive_get_file_list(pathData, NULL);

                  if (archive_list)
                  {
                     if (archive_list->size == 1)
                     {
                        /* Assume archives with one file should have that file loaded directly.
                         * Don't just extend this to add all files in a zip, because we might hit
                         * something like MAME/FBA where only the archives themselves are valid content. */
                        pathArray = (QString(pathData) + "#" 
                              + archive_list->elems[0].data).toUtf8();
                        pathData = pathArray.constData();

                        if (!extensions.isEmpty() && playlistDialog->filterInArchive())
                        {
                           /* If the user chose to filter extensions inside archives, and this particular file inside the archive
                            * doesn't have one of the chosen extensions, then we skip it. */
                           if (extensions.contains(QFileInfo(pathData).suffix()))
                              add = true;
                        }
                     }

                     string_list_free(archive_list);
                  }
               }
            }
         }

         if (add)
            list.append(fileInfo.absoluteFilePath());
      }
   }

   return true;
}

void MainWindow::onPlaylistFilesDropped(QStringList files)
{
   addFilesToPlaylist(files);
}

/* Takes a list of files and folders and adds them to the currently selected playlist. Folders will have their contents added recursively. */
void MainWindow::addFilesToPlaylist(QStringList files)
{
   int i;
   QStringList list;
   QString currentPlaylistPath;
   QListWidgetItem *currentItem = m_listWidget->currentItem();
   QByteArray currentPlaylistArray;
   QScopedPointer<QProgressDialog> dialog(NULL);
   QHash<QString, QString> selectedCore;
   QHash<QString, QString> itemToAdd;
   QString selectedDatabase;
   QString selectedName;
   QString selectedPath;
   QStringList selectedExtensions;
   PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
   const char *currentPlaylistData     = NULL;
   playlist_t *playlist                = NULL;
   settings_t *settings                = config_get_ptr();

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
   selectedExtensions = m_playlistEntryDialog->getSelectedExtensions();

   if (!selectedExtensions.isEmpty())
      selectedExtensions.replaceInStrings(QRegularExpression("^\\."), "");

   if (selectedDatabase.isEmpty())
      selectedDatabase = QFileInfo(currentPlaylistPath).fileName();
   else
      selectedDatabase += ".lpl";

   dialog.reset(new QProgressDialog(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES), "Cancel", 0, 0, this));
   dialog->setWindowModality(Qt::ApplicationModal);
   dialog->show();

   qApp->processEvents();

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

      /* Needed to update progress dialog while 
       * doing a lot of stuff on the main thread. */
      if (i % 25 == 0)
         qApp->processEvents();

      if (fileInfo.isDir())
      {
         QDir dir(path);
         bool success = addDirectoryFilesToList(dialog.data(), list, dir, selectedExtensions);

         if (!success)
            return;

         continue;
      }

      if (fileInfo.isFile())
      {
         bool add = false;

         if (selectedExtensions.isEmpty())
            add = true;
         else
         {
            QByteArray pathArray = path.toUtf8();
            const char *pathData = pathArray.constData();

            if (selectedExtensions.contains(fileInfo.suffix()))
               add = true;
            else if (playlistDialog->filterInArchive() && path_is_compressed_file(pathData))
            {
               /* We'll add it here but really just delay the check until later when the archive contents are iterated. */
               add = true;
            }
         }

         if (add)
            list.append(fileInfo.absoluteFilePath());
      }
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

      /* Cancel out of everything, the 
       * current progress will not be written 
       * to the playlist at all. */
      if (dialog->wasCanceled())
      {
         playlist_free(playlist);
         return;
      }

      if (fileName.isEmpty())
         continue;

      /* a modal QProgressDialog calls processEvents() automatically in setValue() */
      dialog->setValue(i + 1);

      fileInfo = fileName;

      /* Make sure we're looking at a user-specified field and not just "<multiple>"
       * in case it was a folder with one file in it */
      if (files.count() == 1 && list.count() == 1 && i == 0 && playlistDialog->nameFieldEnabled())
      {
         fileBaseNameArray = selectedName.toUtf8();
         pathArray = QDir::toNativeSeparators(selectedPath).toUtf8();
      }
      /* Otherwise just use the file name itself (minus extension) for the playlist entry title */
      else
      {
         fileBaseNameArray = fileInfo.completeBaseName().toUtf8();
         pathArray = QDir::toNativeSeparators(fileName).toUtf8();
      }

      fileNameNoExten = fileBaseNameArray.constData();

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
               /* Assume archives with one file should have that file loaded directly.
                * Don't just extend this to add all files in a zip, because we might hit
                * something like MAME/FBA where only the archives themselves are valid content. */
               pathArray = QDir::toNativeSeparators(QString(pathData) + "#" + list->elems[0].data).toUtf8();
               pathData = pathArray.constData();

               if (!selectedExtensions.isEmpty() && playlistDialog->filterInArchive())
               {
                  /* If the user chose to filter extensions inside archives, and this particular file inside the archive
                   * doesn't have one of the chosen extensions, then we skip it. */
                  if (!selectedExtensions.contains(QFileInfo(pathData).suffix()))
                  {
                     string_list_free(list);
                     continue;
                  }
               }
            }

            string_list_free(list);
         }
      }

      {
         struct playlist_entry entry = {0};

         /* the push function reads our entry as const, so these casts are safe */
         entry.path      = const_cast<char*>(pathData);
         entry.label     = const_cast<char*>(fileNameNoExten);
         entry.core_path = const_cast<char*>(corePathData);
         entry.core_name = const_cast<char*>(coreNameData);
         entry.crc32     = const_cast<char*>("00000000|crc");
         entry.db_name   = const_cast<char*>(databaseData);

         playlist_push(playlist, &entry, settings->bools.playlist_fuzzy_archive_match);
      }
   }

   playlist_write_file(playlist, settings->bools.playlist_use_old_format);
   playlist_free(playlist);

   reloadPlaylists();
}

bool MainWindow::updateCurrentPlaylistEntry(const QHash<QString, QString> &contentHash)
{
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
   settings_t *settings         = config_get_ptr();
   QString playlistPath         = getCurrentPlaylistPath();
   const char *playlistPathData = NULL;
   const char *pathData         = NULL;
   const char *labelData        = NULL;
   const char *corePathData     = NULL;
   const char *coreNameData     = NULL;
   const char *dbNameData       = NULL;
   const char *crc32Data        = NULL;
   playlist_t *playlist         = NULL;
   unsigned index               = 0;
   bool ok                      = false;

   if (  playlistPath.isEmpty() || 
         contentHash.isEmpty()  || 
         !contentHash.contains("index"))
      return false;

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return false;

   path     = contentHash.value("path");
   label    = contentHash.value("label");
   coreName = contentHash.value("core_name");
   corePath = contentHash.value("core_path");
   dbName   = contentHash.value("db_name");
   crc32    = contentHash.value("crc32");

   if (path.isEmpty()     ||
       label.isEmpty()    ||
       coreName.isEmpty() ||
       corePath.isEmpty()
      )
      return false;

   playlistPathArray = playlistPath.toUtf8();
   pathArray         = QDir::toNativeSeparators(path).toUtf8();
   labelArray        = label.toUtf8();
   coreNameArray     = coreName.toUtf8();
   corePathArray     = QDir::toNativeSeparators(corePath).toUtf8();

   if (!dbName.isEmpty())
   {
      dbNameArray = (dbName + ".lpl").toUtf8();
      dbNameData  = dbNameArray.constData();
   }

   playlistPathData = playlistPathArray.constData();
   pathData         = pathArray.constData();
   labelData        = labelArray.constData();
   coreNameData     = coreNameArray.constData();
   corePathData     = corePathArray.constData();

   if (!crc32.isEmpty())
   {
      crc32Array    = crc32.toUtf8();
      crc32Data     = crc32Array.constData();
   }

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

   {
      struct playlist_entry entry = {0};

      /* the update function reads our entry as const, so these casts are safe */
      entry.path      = const_cast<char*>(pathData);
      entry.label     = const_cast<char*>(labelData);
      entry.core_path = const_cast<char*>(corePathData);
      entry.core_name = const_cast<char*>(coreNameData);
      entry.crc32     = const_cast<char*>(crc32Data);
      entry.db_name   = const_cast<char*>(dbNameData);

      playlist_update(playlist, index, &entry);
   }

   playlist_write_file(playlist, settings->bools.playlist_use_old_format);
   playlist_free(playlist);

   reloadPlaylists();

   return true;
}

void MainWindow::onPlaylistWidgetContextMenuRequested(const QPoint&)
{
   settings_t *settings             = config_get_ptr();
   QScopedPointer<QMenu> menu;
   QScopedPointer<QMenu> associateMenu;
   QScopedPointer<QMenu> hiddenPlaylistsMenu;
   QScopedPointer<QMenu> downloadAllThumbnailsMenu;
   QScopedPointer<QAction> hideAction;
   QScopedPointer<QAction> newPlaylistAction;
   QScopedPointer<QAction> deletePlaylistAction;
   QScopedPointer<QAction> renamePlaylistAction;
   QScopedPointer<QAction> downloadAllThumbnailsEntireSystemAction;
   QScopedPointer<QAction> downloadAllThumbnailsThisPlaylistAction;
   QPointer<QAction> selectedAction;
   QPoint cursorPos = QCursor::pos();
   QDir playlistDir(settings->paths.directory_playlist);
   QString currentPlaylistDirPath;
   QString currentPlaylistPath;
   QString currentPlaylistFileName;
   QFile currentPlaylistFile;
   QFileInfo currentPlaylistFileInfo;
   QMap<QString, const core_info_t*> coreList;
   QListWidgetItem *selectedItem    = m_listWidget->itemAt(
         m_listWidget->viewport()->mapFromGlobal(cursorPos));
   QString playlistDirAbsPath       = playlistDir.absolutePath();
   core_info_list_t *core_info_list = NULL;
   unsigned i                       = 0;
   int j                            = 0;
   bool specialPlaylist             = false;
   bool foundHiddenPlaylist         = false;

   if (selectedItem)
   {
      currentPlaylistPath = selectedItem->data(Qt::UserRole).toString();
      currentPlaylistFile.setFileName(currentPlaylistPath);

      currentPlaylistFileInfo = QFileInfo(currentPlaylistPath);
      currentPlaylistFileName = currentPlaylistFileInfo.fileName();
      currentPlaylistDirPath = currentPlaylistFileInfo.absoluteDir().absolutePath();
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

      renamePlaylistAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST)) + "...", this));
      menu->addAction(renamePlaylistAction.data());
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
      return;

   if (!specialPlaylist && selectedAction->parent() == associateMenu.data())
   {
      core_info_ctx_find_t coreInfo;
      playlist_t *cachedPlaylist              = playlist_get_cached();
      playlist_t *playlist                    = NULL;
      bool loadPlaylist                       = true;
      QByteArray currentPlaylistPathByteArray = currentPlaylistPath.toUtf8();
      const char *currentPlaylistPathCString  = currentPlaylistPathByteArray.data();
      QByteArray corePathByteArray            = selectedAction->property("core_path").toString().toUtf8();
      const char *corePath                    = corePathByteArray.data();

      /* Load playlist, if required */
      if (cachedPlaylist)
      {
         if (string_is_equal(currentPlaylistPathCString, playlist_get_conf_path(cachedPlaylist)))
         {
            playlist = cachedPlaylist;
            loadPlaylist = false;
         }
      }

      if (loadPlaylist)
         playlist = playlist_init(currentPlaylistPathCString, COLLECTION_SIZE);

      if (playlist)
      {
         /* Get core info */
         coreInfo.inf  = NULL;
         coreInfo.path = corePath;

         if (core_info_find(&coreInfo, corePath))
         {
            /* Set new core association */
            playlist_set_default_core_path(playlist, coreInfo.inf->path);
            playlist_set_default_core_name(playlist, coreInfo.inf->display_name);
         }
         else
         {
            playlist_set_default_core_path(playlist, "DETECT");
            playlist_set_default_core_name(playlist, "DETECT");
         }

         /* Write changes to disk */
         playlist_write_file(playlist,
               settings->bools.playlist_use_old_format);

         /* Free playlist, if required */
         if (loadPlaylist)
            playlist_free(playlist);
      }
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
   else if (selectedItem && selectedAction == renamePlaylistAction.data())
   {
      if (currentPlaylistFile.exists())
      {
         QString oldName = selectedItem->text();
         QString name = QInputDialog::getText(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME), QLineEdit::Normal, oldName);

         if (!name.isEmpty())
         {
            renamePlaylistItem(selectedItem, name);
            reloadPlaylists();
         }
      }
   }
   else if (selectedAction == newPlaylistAction.data())
   {
      QString name = QInputDialog::getText(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME));
      QString newPlaylistPath = playlistDirAbsPath + "/" + name + ".lpl";
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
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();

   getPlaylistFiles();

   m_listWidget->clear();
   m_listWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
   m_listWidget->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

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

      fileDisplayName.remove(".lpl");

      iconPath = QString(settings->paths.directory_assets) + ICON_PATH + fileDisplayName + ".png";

      hasIcon = QFile::exists(iconPath);

      if (hasIcon)
         icon = QIcon(iconPath);
      else
         icon = m_folderIcon;

      item = new QListWidgetItem(icon, fileDisplayName);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
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
               m_listWidget->setCurrentItem(initialItem);
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
   QString playlistPath;
   QListWidgetItem *playlistItem = m_listWidget->currentItem();

   if (!playlistItem)
      return playlistPath;

   playlistPath = playlistItem->data(Qt::UserRole).toString();

   return playlistPath;
}

bool MainWindow::currentPlaylistIsSpecial()
{
   QFileInfo currentPlaylistFileInfo;
   QString currentPlaylistPath;
   QString currentPlaylistDirPath;
   bool specialPlaylist                 = false;
   settings_t *settings                 = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);
   QString playlistDirAbsPath           = playlistDir.absolutePath();
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();

   if (!currentPlaylistItem)
      return false;

   currentPlaylistPath     = currentPlaylistItem->data(Qt::UserRole).toString();
   currentPlaylistFileInfo = QFileInfo(currentPlaylistPath);
   currentPlaylistDirPath  = currentPlaylistFileInfo.absoluteDir().absolutePath();

   /* Don't just compare strings in case there are 
    * case differences on Windows that should be ignored. */
   if (QDir(currentPlaylistDirPath) != QDir(playlistDirAbsPath))
      specialPlaylist = true;

   return specialPlaylist;
}

bool MainWindow::currentPlaylistIsAll()
{
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();
   bool all = false;

   if (!currentPlaylistItem)
      return false;

   if (currentPlaylistItem->data(Qt::UserRole).toString() 
         == ALL_PLAYLISTS_TOKEN)
      all = true;

   return all;
}

void MainWindow::deleteCurrentPlaylistItem()
{
   QByteArray playlistArray;
   QString playlistPath                = getCurrentPlaylistPath();
   QHash<QString, QString> contentHash = getCurrentContentHash();
   playlist_t *playlist                = NULL;
   const char *playlistData            = NULL;
   unsigned index                      = 0;
   bool ok                             = false;
   bool isAllPlaylist                  = currentPlaylistIsAll();
   settings_t *settings                = config_get_ptr();

   if (isAllPlaylist)
      return;

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
   playlist_write_file(playlist, settings->bools.playlist_use_old_format);
   playlist_free(playlist);

   reloadPlaylists();
}

QString MainWindow::getPlaylistDefaultCore(QString dbName)
{
   settings_t *settings       = config_get_ptr();
   QByteArray dbNameByteArray = dbName.toUtf8();
   const char *dbNameCString  = dbNameByteArray.data();
   playlist_t *cachedPlaylist = playlist_get_cached();
   playlist_t *playlist       = NULL;
   bool loadPlaylist          = true;
   QString corePath           = QString();
   char playlistPath[PATH_MAX_LENGTH];

   playlistPath[0] = '\0';

   if (!settings || string_is_empty(dbNameCString))
      return corePath;

   /* Get playlist path */
   fill_pathname_join(
      playlistPath,
      settings->paths.directory_playlist, dbNameCString,
      sizeof(playlistPath));
   strlcat(playlistPath, ".lpl", sizeof(playlistPath));

   /* Load playlist, if required */
   if (cachedPlaylist)
   {
      if (string_is_equal(playlistPath, playlist_get_conf_path(cachedPlaylist)))
      {
         playlist = cachedPlaylist;
         loadPlaylist = false;
      }
   }

   if (loadPlaylist)
      playlist = playlist_init(playlistPath, COLLECTION_SIZE);

   if (playlist)
   {
      const char *defaultCorePath = playlist_get_default_core_path(playlist);

      /* Get default core path */
      if (!string_is_empty(defaultCorePath) &&
          !string_is_equal(defaultCorePath, "DETECT"))
         corePath = QString::fromUtf8(defaultCorePath);

      /* Free playlist, if required */
      if (loadPlaylist)
         playlist_free(playlist);
   }

   return corePath;
}

void MainWindow::getPlaylistFiles()
{
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);

   m_playlistFiles = playlistDir.entryList(QDir::NoDotAndDotDot | QDir::Readable | QDir::Files, QDir::Name);
}

void PlaylistModel::getPlaylistItems(QString path)
{
   QByteArray pathArray;
   const char *pathData = NULL;
   playlist_t *playlist = NULL;
   unsigned playlistSize = 0;
   unsigned i = 0;

   pathArray.append(path);
   pathData = pathArray.constData();

   playlist = playlist_init(pathData, COLLECTION_SIZE);
   playlistSize = playlist_get_size(playlist);

   for (i = 0; i < playlistSize; i++)
   {
      const struct playlist_entry *entry  = NULL;
      QHash<QString, QString> hash;

      playlist_get_index(playlist, i, &entry);

      if (string_is_empty(entry->path))
         continue;
      else
         hash["path"] = entry->path;

      hash["index"] = QString::number(i);

      if (string_is_empty(entry->label))
      {
         hash["label"] = entry->path;
         hash["label_noext"] = entry->path;
      }
      else
      {
         hash["label"] = entry->label;
         hash["label_noext"] = entry->label;
      }

      if (!string_is_empty(entry->core_path))
         hash["core_path"] = entry->core_path;

      if (!string_is_empty(entry->core_name))
         hash["core_name"] = entry->core_name;

      if (!string_is_empty(entry->crc32))
         hash["crc32"] = entry->crc32;

      if (!string_is_empty(entry->db_name))
      {
         hash["db_name"] = entry->db_name;
         hash["db_name"].remove(".lpl");
      }

      m_contents.append(hash);
   }

   playlist_free(playlist);
   playlist = NULL;
}

void PlaylistModel::addPlaylistItems(const QStringList &paths, bool add)
{
   int i;

   if (paths.isEmpty())
      return;

   beginResetModel();

   m_contents.clear();

   for (i = 0; i < paths.size(); i++)
      getPlaylistItems(paths.at(i));

   endResetModel();
}

void PlaylistModel::addDir(QString path, QFlags<QDir::Filter> showHidden)
{
   QDir dir = path;
   QStringList dirList;
   int i = 0;

   dirList = dir.entryList(QDir::NoDotAndDotDot |
      QDir::Readable |
      QDir::Files |
      showHidden,
      QDir::Name);

   if (dirList.count() == 0)
      return;

   beginResetModel();

   m_contents.clear();

   for (i = 0; i < dirList.count(); i++)
   {
      QString fileName = dirList.at(i);
      QHash<QString, QString> hash;
      QString filePath(QDir::toNativeSeparators(dir.absoluteFilePath(fileName)));
      QFileInfo fileInfo(filePath);

      hash["path"]        = filePath;
      hash["label"]       = hash["path"];
      hash["label_noext"] = fileInfo.completeBaseName();
      hash["db_name"]     = fileInfo.dir().dirName();

      m_contents.append(hash);
   }

   endResetModel();
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
