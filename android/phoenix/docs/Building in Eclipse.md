## How to import the project:

   1. - Open Eclipse, do: File->Import->Existing Projects Into Workspace.

   2. - Choose "Select root directory", and also check "Search for nested projects"

   3. - Browse to the location of the folder named "phoenix" in the RetroArch repository and select it as the root dir. (as of writing, it is /android/phoenix).

   4. - You should see two projects have been found, "RetroArch" and "android-support-v7-appcompat". Import both of these.

   5. - Let Eclipse finish building the workspace, or whatever.

   6. - You should now be able to build it normally like any application.



## Where do I place the built cores?

Simply place all built cores (and the libretro activity) within the directory [phoenix root]/libs/armeabi.
After placing your cores there, building the front-end, and sending it off to your Android device, they should show up within the core selection screen of the front-end.