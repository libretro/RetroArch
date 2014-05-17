#include "RetroArch-Cascades.h"
#include <Qt/qdeclarativedebug.h>

using ::bb::cascades::Application;

Q_DECL_EXPORT int main(int argc, char **argv)
{
    RetroArch mainApp;

	// Instantiate the main application constructor.
    Application app(argc, argv);

    // Initialize our application.
    QObject::connect(&app, SIGNAL( aboutToQuit() ), &mainApp, SLOT( aboutToQuit() ));

    // We complete the transaction started in the main application constructor and start the
    // client event loop here. When loop is exited the Application deletes the scene which
    // deletes all its children.
    return Application::exec();
}

