#ifndef _RETROARCHCASCADES_H_
#define _RETROARCHCASCADES_H_

#include <bb/cascades/Application>
#include <bb/cascades/DropDown>
#include <bb/cascades/OrientationSupport>
#include <QThread>

#include <screen/screen.h>
#include <sys/neutrino.h>
#include "ButtonMap.h"
#include "../../../frontend/info/core_info.h"

using namespace bb::cascades;

namespace bb
{
    namespace cascades
    {
        class Page;
    }
}

class RetroArch : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString content  READ getContent  WRITE setContent  NOTIFY contentChanged)
    Q_PROPERTY(QString core READ getCore WRITE setCore NOTIFY coreChanged)
    Q_PROPERTY(QString contentExtensions READ getContentExtensions NOTIFY contentExtensionsChanged)

public:
    RetroArch();
    ~ RetroArch();

    Q_INVOKABLE void start();
    Q_INVOKABLE void findDevices();
    Q_INVOKABLE void discoverController(int player);
    void populateCores(core_info_list_t * info);

signals:
	void contentChanged(QString);
	void coreChanged(QString);
	void contentExtensionsChanged(QString);

public slots:
	void onRotationCompleted();
	void onCoreSelected(QVariant);

private:
    /**
     * This QThread-run function runs the custom window rendering in a separate thread to avoid lag
     * in the rest of the Cascades UI.
     */
    void run();

    QString content;
    QString getContent();
    void setContent(QString content);

    QString core;
    QString getCore();
	void setCore(QString core);

	QString contentExtensions;
	QString getContentExtensions();

	void initRASettings();

	int chid, coid;
	int state;
	DropDown *coreSelection;
	DropDown *deviceSelection;
	QVariantMap coreInfo;
	char **coreList;
	int coreSelectedIndex;
	core_info_list_t *core_info_list;

	ButtonMap *buttonMap;
};

enum {
	RETROARCH_RUNNING,
	RETROARCH_START_REQUESTED,
	RETROARCH_BUTTON_MAP,
	RETROARCH_EXIT
};


typedef union {
	_pulse pulse;
   int code;
} recv_msg;

#endif
