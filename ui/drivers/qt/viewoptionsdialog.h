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

#ifdef HAVE_MENU
class QWidget;
class OptionsCategory;
class QListWidget;
class QStackedLayout;
#endif

class ViewOptionsWidget : public QWidget
{
   Q_OBJECT
public:
   ViewOptionsWidget(MainWindow *mainwindow, QWidget *parent = 0);
public slots:
   void onAccepted();
   void onRejected();
   void loadViewOptions();
   void saveViewOptions();
private slots:
   void onThemeComboBoxIndexChanged(int index);
   void onHighlightColorChoose();
private:
   void showOrHideHighlightColor();

   MainWindow *m_mainwindow;
   QSettings *m_settings;
   QCheckBox *m_saveGeometryCheckBox;
   QCheckBox *m_saveDockPositionsCheckBox;
   QCheckBox *m_saveLastTabCheckBox;
   QCheckBox *m_showHiddenFilesCheckBox;
   QComboBox *m_themeComboBox;
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

class ViewOptionsDialog : public QDialog
{
   Q_OBJECT
public:
   ViewOptionsDialog(MainWindow *window, QWidget *parent = 0);
#ifdef HAVE_MENU
   void repaintIcons();
#endif
public slots:
   void showDialog();
   void hideDialog();
private slots:
   void onRejected();
private:
#ifdef HAVE_MENU
   void addCategory(QWidget *widget, QString name, QString icon);
   void addCategory(OptionsCategory *category);
   QVector<OptionsCategory*> m_categoryList;
   QListWidget *m_optionsList;
   QStackedLayout *m_optionsStack;
#else
   ViewOptionsWidget *m_viewOptionsWidget;
#endif
};

#endif
