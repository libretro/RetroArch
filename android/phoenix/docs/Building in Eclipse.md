## Building the JNI-related files for Phoenix.

   1. If you haven't already, install the Android NDK (http://developer.android.com/tools/sdk/ndk/index.html)

   2. In the Phoenix root directory you should notice the folder named "jni".

   3. Open Command Prompt/Terminal and cd into this directory.

   4. Run ndk-build within this directory (just type "ndk-build" and hit Enter) and wait for everything to finish building.

   5. All built libraries should now reside within the "libs" directory.

   6. Continue to the next section of this document.



## How to import the project into Eclipse and build the front-end:

   1. Install the Android ADT plugin for Eclipse if you haven't. (http://developer.android.com/sdk/installing/installing-adt.html)

   2. In Eclipse, do: File->Import->Existing Android Code Into Workspace.

   3. Browse to the location of the folder named "phoenix" in the RetroArch repository and select it as the root dir. (as of writing, it is /android/phoenix).

   4. You should see two projects have been found, "RetroArch" and "android-support-v7-appcompat". Import both of these.

   5. Let Eclipse finish building the workspace, or whatever.

   6. You should now be able to build it normally like any application.



## Where do I place the built libretro cores?

Simply place all built libretro cores within the directory [phoenix root]/assets/cores. Create this directory if it doesn't exist already.
After placing your cores there, they should show up within the core selection screen of the front-end.

## Notes

1. If you’re running into an issue where adding an existing Android project results in “Invalid project description”, please select a workspace location that doesn’t contain the Android projects.
2. If Eclipse still complains about missing appcompat, right-click on RetroArch->Properties->Android->Library->Add “android-support-v7-appcompat”, and then remove the old appcompat reference.
