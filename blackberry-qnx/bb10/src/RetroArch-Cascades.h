#ifndef _RETROARCHCASCADES_H_
#define _RETROARCHCASCADES_H_

#include <bb/cascades/Application>
#include <screen/screen.h>
#include <QThread>

using namespace bb::cascades;

namespace bb
{
    namespace cascades
    {
        class Page;
    }
}

class RetroArch: public QThread
{
    Q_OBJECT

    Q_PROPERTY(QString rom  READ getRom  WRITE setRom  NOTIFY romChanged)
    Q_PROPERTY(QString core READ getCore WRITE setCore NOTIFY coreChanged)

public:
    RetroArch();
    ~ RetroArch();

signals:
	void romChanged(QString);
	void coreChanged(QString);

private:
    /**
     * This QThread-run function runs the custom window rendering in a separate thread to avoid lag
     * in the rest of the Cascades UI.
     */
    void run();

    QString rom;
    QString getRom();
    void setRom(QString rom);

    QString core;
    QString getCore();
	void setCore(QString core);

};

#endif
