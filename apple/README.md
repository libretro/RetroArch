# RetroArch for iOS

RetroArch for iOS can be run directly on your device without the need for jailbreaking. To do this, you will need a few things:

* Your own iOS Apple developer account
* Your developer account set up on your computer (your certs, etc.)
* A Distribution provisioning profile for RetroArch (a wildcard profile is fine and suggested)

Once you have all of this stuff, getting RetroArch on a non-jailbroken device is pretty simple.

## Config

RetroArch needs to know a couple things when building the app. You can configure these under `apple/script/build.config` inside the RetroArch repo. Once you initially clone down the repo, go into this file and make the changes.

### CODE\_SIGN\_IDENTITY

This is the identity that will be used when signing the app after it is built. Under normal circumstance, you shouldn't have to change this. But if you have multiple Apple dev accounts on your computer, you will have to be more specific.

```
CODE_SIGN_IDENTITY="iPhone Distribution: Bill Cosby"
```

Adding the name of the account will make sure it uses the exact codesigning identity.

### PROVISIONING

Before you build the app, you'll need to download the provisioning profile you want to use for the app into the `apple` directory. Just drop it in there, and add the file name to the config.

When RetroArch is built into an IPA, it will embed this provisioning profile into the IPA so you can just install the IPA right onto your phone.


## Building

After you've configured the right things, you're ready for building. You'll want to run these from the `apple` directory of the RetroArch project.

### Emulator Cores

Before building the app, you'll need to build the libretro cores.

You'll need to clone down the [libretro/libretro-super](https://github.com/libretro/libretro-super) repo into the same directory where you cloned this repo.

Your directories should look like this:

```
your-repos-dir/libretro-super
your-repos-dir/RetroArch
```

Run the libretro-super fetch script to first get the libretro core repos.

```sh
./libretro-fetch.sh
```

Run the libretro-super iOS build script to build the libretro cores.

```sh
./libretro-build-ios.sh
```

This will clone down their repos, build them into the `libretro-super/dist/ios` directory. Once this is complete, you need to copy them into the appropriate directory for RetroArch iOS. The path is:

`RetroArch/apple/iOS/modules`

### Build RetroArch iOS app

Now just run:

```sh
script/build
```

This will build the iOS app, codesign everything that needs to, and package it into a distributable IPA.

Once completed, you can find the IPA inside the `apple/build/Release-iphoneos` directory. Excecute the IPA file and then open iTunes, with you device connected, click in your device, then Applications, find de RetroArch in the list, click install and syncronize with you device.

## Roms

iTunes File Sharing is enabled on RetroArch. You can simply drag your rom files into the RetroArch app. They will be available the next time you launch the app.

Alternatively, you can use something like [iExplorer](http://www.macroplant.com/iexplorer) to manually copy files over. Doing this will give you the benefit of being able to use directories, since iTunes File Sharing does not support directories.

## BTstack (Jailbreak only)

With the BTstack cydia package installed both WiiMote and PS3 Pads are usable. To enable BTstack usage tap the Settings button in the upper left corner and turn on the 'Enable BTstack' option.

### Connecting a WiiMote

Press both the 1 and 2 buttons on the WiiMote at the same time. The WiiMote is successfully completed when the 4 lights stop flashing.

### Connecting PS3

Before a PS3 pad can be used it must be manually paired with your device using a PC, USB cable, and a special piece of software.

The pairing software will likely ask for the bluetooth address of your device, this can be found under the 'Diagnostic Log' option in the settings menu.

The line will appear as:
```
BTpad: Local address is XX:XX:XX:XX:XX:XX
```
