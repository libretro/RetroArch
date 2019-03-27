#include "options.h"

RecordingCategory::RecordingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS);
   setCategoryIcon("menu_record");
}

QVector<OptionsPage*> RecordingCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new RecordingPage(this);

   return pages;
}

RecordingPage::RecordingPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *RecordingPage::widget()
{
   QWidget * widget = new QWidget;
   QVBoxLayout *layout = new QVBoxLayout;
   SettingsGroup *recordingGroup = new SettingsGroup("Recording");
   SettingsGroup *streamingGroup = new SettingsGroup("Streaming");
   QHBoxLayout *hl = new QHBoxLayout;

   recordingGroup->addUIntComboBox(MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY);
   recordingGroup->addFileSelector(MENU_ENUM_LABEL_RECORD_CONFIG);
   recordingGroup->addUIntComboBox(MENU_ENUM_LABEL_VIDEO_RECORD_THREADS);
   recordingGroup->addDirectorySelector(MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY);
   recordingGroup->addCheckBox(MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD);
   recordingGroup->addCheckBox(MENU_ENUM_LABEL_VIDEO_GPU_RECORD);

   hl->addWidget(new UIntRadioButtons(MENU_ENUM_LABEL_STREAMING_MODE));
   hl->addWidget(new UIntRadioButtons(MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY));

   streamingGroup->addRow(hl);

   streamingGroup->addFileSelector(MENU_ENUM_LABEL_STREAM_CONFIG);
   streamingGroup->addStringLineEdit(MENU_ENUM_LABEL_STREAMING_TITLE);
   streamingGroup->addStringLineEdit(MENU_ENUM_LABEL_STREAMING_URL);
   streamingGroup->addUIntSpinBox(MENU_ENUM_LABEL_UDP_STREAM_PORT);

   layout->addWidget(recordingGroup);
   layout->addWidget(streamingGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}
