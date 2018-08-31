#ifndef PLAYLISTENTRYDIALOG_H
#define PLAYLISTENTRYDIALOG_H

#include <QDialog>

class QSettings;
class QLineEdit;
class QComboBox;
class QCheckBox;
class MainWindow;

class PlaylistEntryDialog : public QDialog
{
   Q_OBJECT
public:
   PlaylistEntryDialog(MainWindow *mainwindow, QWidget *parent = 0);
   const QHash<QString, QString> getSelectedCore();
   const QString getSelectedDatabase();
   const QString getSelectedName();
   const QString getSelectedPath();
   const QStringList getSelectedExtensions();
   bool filterInArchive();
   bool nameFieldEnabled();
   void setEntryValues(const QHash<QString, QString> &contentHash);
public slots:
   bool showDialog(const QHash<QString, QString> &hash = QHash<QString, QString>());
   void hideDialog();
   void onAccepted();
   void onRejected();
private slots:
   void onPathClicked();
private:
   void loadPlaylistOptions();

   MainWindow *m_mainwindow;
   QSettings *m_settings;
   QLineEdit *m_nameLineEdit;
   QLineEdit *m_pathLineEdit;
   QLineEdit *m_extensionsLineEdit;
   QComboBox *m_coreComboBox;
   QComboBox *m_databaseComboBox;
   QCheckBox *m_extensionArchiveCheckBox;
};

#endif
