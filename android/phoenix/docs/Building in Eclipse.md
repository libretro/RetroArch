## Building the JNI-related files for Phoenix.

   1. If you haven't already, install the Android NDK (http://developer.android.com/tools/sdk/ndk/index.html)

   2. In the Phoenix root directory you should notice the folder named "jni".

   3. Open Command Prompt/Terminal and cd into this directory.

   4. Run ndk-build within this directory (just type "ndk-build" and hit Enter) and wait for everything to finish building.

   5. All built libraries should now reside within the "libs" directory.

   6. Continue to the next section of this document.



## How to import the project into Eclipse and build the front-end:

   1. Open Eclipse, do: File->Import->Existing Projects Into Workspace.

   2. Choose "Select root directory", and also check "Search for nested projects"

   3. Browse to the location of the folder named "phoenix" in the RetroArch repository and select it as the root dir. (as of writing, it is /android/phoenix).

   4. You should see two projects have been found, "RetroArch" and "android-support-v7-appcompat". Import both of these.

   5. Let Eclipse finish building the workspace, or whatever.

   6. You should now be able to build it normally like any application.



## Where do I place the built cores?

Simply place all built cores (and the libretro activity) within the directory [phoenix root]/libs/armeabi-v7a.
After placing your cores there, building the front-end, and sending it off to your Android device, they should show up within the core selection screen of the front-end.