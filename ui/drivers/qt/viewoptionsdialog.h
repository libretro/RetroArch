#ifndef VIEWOPTIONSDIALOG_H
#define VIEWOPTIONSDIALOG_H

#include <QDialog>

class MainWindow;
class QSettings;
class QCheckBox;
class QComboBox;
class QPushButton;
class QColor;
class QLabel;
class QSpinBox;

class ViewOptionsDialog : public QDialog
{
   Q_OBJECT
public:
   ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent = 0);
public slots:
   void showDialog();
   void hideDialog();
   void onAccepted();
   void onRejected();
private slots:
   void onThemeComboBoxIndexChanged(int index);
   void onThumbnailComboBoxIndexChanged(int index);
   void onHighlightColorChoose();
private:
   void loadViewOptions();
   void saveViewOptions();
   void showOrHideHighlightColor();

   MainWindow *m_mainwindow;
   QSettings *m_settings;
   QCheckBox *m_saveGeometryCheckBox;
   QCheckBox *m_saveDockPositionsCheckBox;
   QCheckBox *m_saveLastTabCheckBox;
   QCheckBox *m_showHiddenFilesCheckBox;
   QComboBox *m_themeComboBox;
   QComboBox *m_thumbnailComboBox;
   QSpinBox *m_thumbnailCacheSpinBox;
   QComboBox *m_startupPlaylistComboBox;
   QPushButton *m_highlightColorPushButton;
   QColor m_highlightColor;
   QLabel *m_highlightColorLabel;
   QString m_customThemePath;
   QCheckBox *m_suggestLoadedCoreFirstCheckBox;
   QSpinBox *m_allPlaylistsListMaxCountSpinBox;
   QSpinBox *m_allPlaylistsGridMaxCountSpinBox;
};

#endif
