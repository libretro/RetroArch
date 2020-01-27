#ifndef OPTIONS_H
#define OPTIONS_H

#include <QTabWidget>
#include <QColorDialog>
#include <QScrollArea>
#include <QDebug>

#include "../settingswidgets.h"

class MainWindow;
class ViewOptionsWidget;

class OptionsPage : public QObject
{
   Q_OBJECT

public:
   OptionsPage(QObject *parent = nullptr)
      : QObject(parent)
   {
   }

   QString displayName() const
   {
      return m_displayName;
   }

   virtual QWidget *widget() = 0;
   virtual void load() {}
   virtual void apply() {}

protected:
   void setDisplayName(msg_hash_enums name)
   {
      m_displayName = msg_hash_to_str(name);
   }

   void setDisplayName(const QString& name)
   {
      m_displayName = name;
   }

   QString m_displayName = "General";
};

class OptionsCategory : public QObject
{
   Q_OBJECT
public:
   OptionsCategory(QObject *parent = nullptr) : QObject(parent) {}
   OptionsCategory(MainWindow *mainwindow, QObject *parent = nullptr) : QObject(parent) {}
   virtual QVector<OptionsPage*> pages() = 0;
   QString displayName() const { return m_displayName; }
   QString categoryIconName() const { return m_categoryIconName; }
   virtual void load()
   {
      for (int i = 0; i < m_pages.size(); i++)
         m_pages.at(i)->load();
   }
   virtual void apply()
   {
      for (int i = 0; i < m_pages.size(); i++)
         m_pages.at(i)->apply();
   }
protected:
   void setDisplayName(msg_hash_enums name) { m_displayName = msg_hash_to_str(name); }
   void setCategoryIcon(const QString &categoryIconName) { m_categoryIconName = categoryIconName; }
   QString m_displayName;
   QString m_categoryIconName = "setting";
   QVector<OptionsPage*> m_pages;
};

/***********************************************************
   Drivers
************************************************************/
class DriversCategory : public OptionsCategory
{
public:
   DriversCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class DriversPage : public OptionsPage
{
   Q_OBJECT
public:
   DriversPage(QObject *parent = nullptr);
   QWidget *widget();
};

/***********************************************************
   AI Service 
************************************************************/
class AIServiceCategory : public OptionsCategory
{
public:
   AIServiceCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AIServicePage : public OptionsPage
{
   Q_OBJECT
public:
   AIServicePage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Video
************************************************************/
class VideoCategory : public OptionsCategory
{
public:
   VideoCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AspectRatioRadioButton : public QRadioButton
{
   Q_OBJECT
public:
   AspectRatioRadioButton(unsigned min, unsigned max, QWidget *parent = 0);
private:
   unsigned m_min;
   unsigned m_max;
};

class AspectRatioGroup : public SettingsGroup
{
   Q_OBJECT
public:
   AspectRatioGroup(const QString &title, QWidget *parent = 0);
private slots:
   void paintEvent(QPaintEvent *event);
   void onAspectRadioToggled(bool checked);
   void onAspectRadioClicked(bool checked);
private:
   AspectRatioRadioButton *m_radioButton;
   UIntComboBox *m_comboBox;
};

class VideoPage : public OptionsPage
{
   Q_OBJECT
public:
   VideoPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onResolutionComboIndexChanged(const QString& value);
private:
   QComboBox *m_resolutionCombo;
};

class CrtSwitchresPage : public OptionsPage
{
   Q_OBJECT
public:
   CrtSwitchresPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onCrtSuperResolutionComboIndexChanged(int index);
private:
   QComboBox *m_crtSuperResolutionCombo;
};

/************************************************************
   Audio
************************************************************/
class AudioCategory : public OptionsCategory
{
public:
   AudioCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AudioPage : public OptionsPage
{
   Q_OBJECT
public:
   AudioPage(QObject *parent = nullptr);
   QWidget *widget();
};

class MenuSoundsPage : public OptionsPage
{
   Q_OBJECT
public:
   MenuSoundsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Input
************************************************************/
class InputCategory : public OptionsCategory
{
public:
   InputCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class InputPage : public OptionsPage
{
   Q_OBJECT
public:
   InputPage(QObject *parent = nullptr);
   QWidget *widget();
};

class HotkeyBindsPage : public OptionsPage
{
   Q_OBJECT
public:
   HotkeyBindsPage(QObject *parent = nullptr);
   QWidget *widget();
};

class UserBindsPage : public OptionsPage
{
   Q_OBJECT
public:
   UserBindsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Latency
************************************************************/
class LatencyCategory : public OptionsCategory
{
public:
   LatencyCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class LatencyPage : public OptionsPage
{
   Q_OBJECT
public:
   LatencyPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Core
************************************************************/
class CoreCategory : public OptionsCategory
{
public:
   CoreCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class CorePage : public OptionsPage
{
   Q_OBJECT
public:
   CorePage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Configuration
************************************************************/
class ConfigurationCategory : public OptionsCategory
{
public:
   ConfigurationCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class ConfigurationPage : public OptionsPage
{
   Q_OBJECT
public:
   ConfigurationPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Saving
************************************************************/
class SavingCategory : public OptionsCategory
{
public:
   SavingCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class SavingPage : public OptionsPage
{
   Q_OBJECT
public:
   SavingPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Logging
************************************************************/
class LoggingCategory : public OptionsCategory
{
public:
   LoggingCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class LoggingPage : public OptionsPage
{
   Q_OBJECT
public:
   LoggingPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Frame Throttle
************************************************************/
class FrameThrottleCategory : public OptionsCategory
{
public:
   FrameThrottleCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class FrameThrottlePage : public OptionsPage
{
   Q_OBJECT
public:
   FrameThrottlePage(QObject *parent = nullptr);
   QWidget *widget();
};

class RewindPage : public OptionsPage
{
   Q_OBJECT
public:
   RewindPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Recording
************************************************************/
class RecordingCategory : public OptionsCategory
{
public:
   RecordingCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class RecordingPage : public OptionsPage
{
   Q_OBJECT
public:
   RecordingPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   User Interface
************************************************************/
class UserInterfaceCategory : public OptionsCategory
{
public:
   UserInterfaceCategory(QWidget *parent);
   UserInterfaceCategory(MainWindow *mainwindow, QWidget *parent);
   QVector<OptionsPage*> pages();
private:
   MainWindow *m_mainwindow;
};

class UserInterfacePage : public OptionsPage
{
   Q_OBJECT
public:
   UserInterfacePage(QObject *parent = nullptr);
   QWidget *widget();
};

class ViewsPage : public OptionsPage
{
   Q_OBJECT
public:
   ViewsPage(QObject *parent = nullptr);
   QWidget *widget();
};

class QuickMenuPage : public OptionsPage
{
   Q_OBJECT
public:
   QuickMenuPage(QObject *parent = nullptr);
   QWidget *widget();
};

class AppearancePage : public OptionsPage
{
   Q_OBJECT
public:
   AppearancePage(QObject *parent = nullptr);
   QWidget *widget();
};

class DesktopMenuPage : public OptionsPage
{
   Q_OBJECT
public:
   DesktopMenuPage(MainWindow *mainwindow, QObject *parent = nullptr);
   QWidget *widget();
   void load();
   void apply();
private:
   ViewOptionsWidget *m_widget;
};

/************************************************************
   Onscreen Display
************************************************************/
class OnscreenDisplayCategory : public OptionsCategory
{
public:
   OnscreenDisplayCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class OverlayPage : public OptionsPage
{
   Q_OBJECT
public:
   OverlayPage(QObject *parent = nullptr);
   QWidget *widget();
};

class NotificationsPage : public OptionsPage
{
   Q_OBJECT
public:
   NotificationsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Achievements
************************************************************/
class AchievementsCategory : public OptionsCategory
{
public:
   AchievementsCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AchievementsPage : public OptionsPage
{
   Q_OBJECT
public:
   AchievementsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Network
************************************************************/
class NetworkCategory : public OptionsCategory
{
public:
   NetworkCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class NetplayPage : public OptionsPage
{
   Q_OBJECT
public:
   NetplayPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onRadioButtonClicked(int);
private:
   QGroupBox* createMitmServerGroup();
};

class UpdaterPage : public OptionsPage
{
   Q_OBJECT
public:
   UpdaterPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Playlists
************************************************************/
class PlaylistsCategory : public OptionsCategory
{
public:
   PlaylistsCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class PlaylistsPage : public OptionsPage
{
   Q_OBJECT
public:
   PlaylistsPage(QObject *parent = nullptr);
   QWidget *widget();
};

class AccountsPage : public OptionsPage
{
   Q_OBJECT
public:
   AccountsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   User
************************************************************/
class UserCategory : public OptionsCategory
{
public:
   UserCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class UserPage : public OptionsPage
{
   Q_OBJECT
public:
   UserPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Directory
************************************************************/
class DirectoryCategory : public OptionsCategory
{
public:
   DirectoryCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class DirectoryPage : public OptionsPage
{
   Q_OBJECT
public:
   DirectoryPage(QObject *parent = nullptr);
   QWidget *widget();
};

static inline QWidget *create_widget(enum menu_displaylist_ctl_state name)
{
   unsigned i;
   QWidget             *widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(list, name, true);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      layout->add(cbs->enum_idx);
   }

   file_list_free(list);

   widget->setLayout(layout);

   return widget;
}

#endif
