# RetroArch for iOS

RetroArch for iOS can be run directly on your device without the need for jailbreaking. To do this, you will need a few things:

* Your own iOS Apple developer account
* Your developer account set up on your computer (your certs, etc.)
* A Distribution provisioning profile for RetroArch (a wildcard profile is fine and suggested)

Once you have all of this stuff, getting RetroArch on a non-jailbroken device is pretty simple.

## Config

RetroArch needs to know a couple things when building the app. You can configure these under `ios/script/build.config` inside the RetroArch repo. Once you initially clone down the repo, go into this file and make the changes.

### CODE_SIGN_IDENTITY

This is the identity that will be used when signing the app after it is built. Under normal circumstance, you shouldn't have to change this. But if you have multiple Apple dev accounts on your computer, you will have to be more specific.

```
CODE_SIGN_IDENTITY="iPhone Distribution: Bill Cosby"
```

Adding the name of the account will make sure it uses the exact codesigning identity.

### PROVISIONING

Before you build the app, you'll need to download the provisioning profile you want to use for the app into the `ios` directory. Just drop it in there, and add the file name to the config.

When RetroArch is built into an IPA, it will embed this provisioning profile into the IPA so you can just install the IPA right onto your phone.


## Building

After you've configured the right things, you're ready for building. You'll want to run these from the `ios` directory of the RetroArch project.

### Everything

To build everything at once, just run:

```sh
script/build
```

This will build the emulator cores, and the RetroArch iOS app.

### Emulator Cores

If you'd like to build just the emulator cores, run:

```sh
script/build_cores
```

This will clone down their repos, build them, and copy them into the RetroArch project.


### RetroArch iOS app

If you'd like to just build the RetroArch iOS app, run:

```sh
script/build_app
```

This will build the iOS app, codesign everything that needs to, and package it into a distributable IPA.

## Cleaning

If you want to wipe everything out and start from scratch, run:

```sh
script/clean
```

This will delete all the emulator core repos. The next time you build these, they will do a fresh clone on their repos.

## Roms

iTunes File Sharing is enabled on RetroArch. You can simply drag your rom files into the RetroArch app. They will be available the next time you launch the app.

Alternatively, you can use something like [iExplorer](http://www.macroplant.com/iexplorer) to manually copy files over. Doing this will give you the benefit of being able to use diretories, since iTunes File Sharing does not support directories.

