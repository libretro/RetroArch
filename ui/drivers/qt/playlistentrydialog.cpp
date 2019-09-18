#include <QSettings>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QFileDialog>
#include <QFileInfo>

#include "playlistentrydialog.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../core_info.h"
#include "../../../file_path_special.h"
#include "../../../msg_hash.h"

#ifndef CXX_BUILD
}
#endif

inline static bool comp_string_lower(const QString &lhs, const QString &rhs)
{
   return lhs.toLower() < rhs.toLower();
}

inline static bool comp_hash_ui_display_name_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("ui_display_name").toLower() < rhs.value("ui_display_name").toLower();
}

PlaylistEntryDialog::PlaylistEntryDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_nameLineEdit(new QLineEdit(this))
   ,m_pathLineEdit(new QLineEdit(this))
   ,m_extensionsLineEdit(new QLineEdit(this))
   ,m_coreComboBox(new QComboBox(this))
   ,m_databaseComboBox(new QComboBox(this))
   ,m_extensionArchiveCheckBox(new QCheckBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES), this))
{
   QFormLayout *form = new QFormLayout();
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
   QVBoxLayout *databaseVBoxLayout = new QVBoxLayout();
   QHBoxLayout *pathHBoxLayout = new QHBoxLayout();
   QHBoxLayout *extensionHBoxLayout = new QHBoxLayout();
   QLabel *databaseLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS), this);
   QToolButton *pathPushButton = new QToolButton(this);

   pathPushButton->setText(QStringLiteral("..."));

   pathHBoxLayout->addWidget(m_pathLineEdit);
   pathHBoxLayout->addWidget(pathPushButton);

   databaseVBoxLayout->addWidget(m_databaseComboBox);
   databaseVBoxLayout->addWidget(databaseLabel);

   extensionHBoxLayout->addWidget(m_extensionsLineEdit);
   extensionHBoxLayout->addWidget(m_extensionArchiveCheckBox);

   m_extensionsLineEdit->setPlaceholderText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER));

   /* Ensure placeholder text is completely visible. */
   m_extensionsLineEdit->setMinimumWidth(QFontMetrics(m_extensionsLineEdit->font()).boundingRect(m_extensionsLineEdit->placeholderText()).width() + m_extensionsLineEdit->frameSize().width());

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY));

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout(this));

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   connect(this, SIGNAL(accepted()), this, SLOT(onAccepted()));
   connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));

   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME), m_nameLineEdit);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH), pathHBoxLayout);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE), m_coreComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE), databaseVBoxLayout);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS), extensionHBoxLayout);

   qobject_cast<QVBoxLayout*>(layout())->addLayout(form);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);

   connect(pathPushButton, SIGNAL(clicked()), this, SLOT(onPathClicked()));
}

bool PlaylistEntryDialog::filterInArchive()
{
   return m_extensionArchiveCheckBox->isChecked();
}

void PlaylistEntryDialog::onPathClicked()
{
   QString filePath = QFileDialog::getOpenFileName(this);

   if (filePath.isEmpty())
      return;

   m_pathLineEdit->setText(filePath);
}

void PlaylistEntryDialog::loadPlaylistOptions()
{
   unsigned i, j;
   core_info_list_t *core_info_list = NULL;

   m_nameLineEdit->clear();
   m_pathLineEdit->clear();
   m_coreComboBox->clear();
   m_databaseComboBox->clear();

   m_coreComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK));
   m_databaseComboBox->addItem(QString("<") + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) + ">", QFileInfo(m_mainwindow->getCurrentPlaylistPath()).fileName().remove(".lpl"));

   core_info_get_list(&core_info_list);

   if (core_info_list && core_info_list->count > 0)
   {
      QVector<QHash<QString, QString> > allCores;
      QStringList allDatabases;

      for (i = 0; i < core_info_list->count; i++)
      {
         QString ui_display_name;
         QHash<QString, QString> hash;
         const core_info_t *core = &core_info_list->list[i];
         QStringList databases   = QString(core->databases).split("|");

         hash["core_name"]         = core->core_name;
         hash["core_display_name"] = core->display_name;
         hash["core_path"]         = core->path;
         hash["core_databases"]    = core->databases;

         ui_display_name           = hash.value("core_name");

         if (ui_display_name.isEmpty())
            ui_display_name        = hash.value("core_display_name");
         if (ui_display_name.isEmpty())
            ui_display_name        = QFileInfo(
                  hash.value("core_path")).fileName();

         if (ui_display_name.isEmpty())
            continue;

         hash["ui_display_name"] = ui_display_name;

         for (j = 0; static_cast<int>(j) < databases.count(); j++)
         {
            QString database = databases.at(static_cast<int>(j));

            if (database.isEmpty())
               continue;

            if (!allDatabases.contains(database))
               allDatabases.append(database);
         }

         if (!allCores.contains(hash))
            allCores.append(hash);
      }

      std::sort(allCores.begin(), allCores.end(), comp_hash_ui_display_name_key_lower);
      std::sort(allDatabases.begin(), allDatabases.end(), comp_string_lower);

      for (j = 0; static_cast<int>(j) < allCores.count(); j++)
      {
         const QHash<QString, QString> &hash = allCores.at(static_cast<int>(j));

         m_coreComboBox->addItem(hash.value("ui_display_name"), QVariant::fromValue(hash));
      }

      for (j = 0; static_cast<int>(j) < allDatabases.count(); j++)
      {
         QString database = allDatabases.at(static_cast<int>(j));
         m_databaseComboBox->addItem(database, database);
      }
   }
}

bool PlaylistEntryDialog::nameFieldEnabled()
{
   return m_nameLineEdit->isEnabled();
}

void PlaylistEntryDialog::setEntryValues(
      const QHash<QString, QString> &contentHash)
{
   QString db;
   QString coreName = contentHash.value("core_name");
   int foundDB = 0;
   int i       = 0;

   loadPlaylistOptions();

   if (contentHash.isEmpty())
   {
      m_nameLineEdit->setText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_pathLineEdit->setText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_nameLineEdit->setEnabled(false);
      m_pathLineEdit->setEnabled(false);
   }
   else
   {
      m_nameLineEdit->setEnabled(true);
      m_pathLineEdit->setEnabled(true);
      m_nameLineEdit->setText(contentHash.value("label"));
      m_pathLineEdit->setText(contentHash.value("path"));
   }

   for (i = 0; i < m_coreComboBox->count(); i++)
   {
      const QHash<QString, QString> hash = m_coreComboBox->itemData(i, Qt::UserRole).value<QHash<QString, QString> >();

      if (hash.isEmpty() || coreName.isEmpty())
         continue;

      if (hash.value("core_name") == coreName)
      {
         m_coreComboBox->setCurrentIndex(i);
         break;
      }
   }

   db = contentHash.value("db_name");

   if (!db.isEmpty())
   {
      foundDB = m_databaseComboBox->findText(db);

      if (foundDB >= 0)
         m_databaseComboBox->setCurrentIndex(foundDB);
   }
}

const QHash<QString, QString> PlaylistEntryDialog::getSelectedCore()
{
   return m_coreComboBox->currentData(Qt::UserRole).value<QHash<QString, QString> >();
}

const QString PlaylistEntryDialog::getSelectedName()
{
   return m_nameLineEdit->text();
}

const QString PlaylistEntryDialog::getSelectedPath()
{
   return m_pathLineEdit->text();
}

const QString PlaylistEntryDialog::getSelectedDatabase()
{
   return m_databaseComboBox->currentData(Qt::UserRole).toString();
}

const QStringList PlaylistEntryDialog::getSelectedExtensions()
{
   QStringList list;
   QString text = m_extensionsLineEdit->text();

   /* Otherwise it would create a QStringList with a single blank entry... */
   if (!text.isEmpty())
      list = text.split(' ');

   return list;
}

void PlaylistEntryDialog::onAccepted()
{
}

void PlaylistEntryDialog::onRejected()
{
}

bool PlaylistEntryDialog::showDialog(const QHash<QString, QString> &hash)
{
   loadPlaylistOptions();
   setEntryValues(hash);

   if (exec() == QDialog::Accepted)
      return true;

   return false;
}

void PlaylistEntryDialog::hideDialog()
{
   reject();
}
