# AltKit

AltKit allows apps to communicate with AltServers on the same WiFi network and enable features such as JIT compilation.

## Installation

To use AltKit in your app, add the following to your `Package.swift` file's dependencies:

```swift
.package(url: "https://github.com/rileytestut/AltKit.git", .upToNextMajor(from: "0.0.1")),
```

Next, add the AltKit package as a dependency for your target:

```swift
.product(name: "AltKit", package: "AltKit"),
```

Finally, right-click on your app's `Info.plist`, select "Open As > Source Code", then add the following entries:

```xml
<key>NSBonjourServices</key>
<array>
    <string>_altserver._tcp</string>
</array>
<key>NSLocalNetworkUsageDescription</key>
<string>[Your app] uses the local network to find and communicate with AltServer.</string>
<key>ALTDeviceID</key>
<string></string>
```

⚠️ The `ALTDeviceID` key must be present in your app's `Info.plist` to let AltStore know it should replace that entry with the user's device's UDID when sideloading your app, which is required for AltKit to work. For local development with AltKit, we recommend setting `ALTDeviceID` to your development device's UDID to ensure everything works as expected. Otherwise, the exact value doesn't matter as long as the entry exists, since it will be replaced by AltStore during installation.

### CMake Integration

Note: CMake 3.15 is required for the integration. The integration only works with the Xcode generator.

Steps:
- Add the AltKit CMake project to your CMake project using `add_subdirectory`.
- Add Swift to your project's supported languages. (ex.: `project(projName LANGUAGES C Swift)`)

If you're using `add_compile_options` or `target_compile_options` and the Swift compiler complains about the option not being supported, it's possible to use CMake's generator expressions to limit the options to non-Swift source files.

Example:
```
add_compile_options($<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fPIC>)
```

## Usage

### Swift
```swift
import AltKit

ServerManager.shared.startDiscovering()

ServerManager.shared.autoconnect { result in
    switch result
    {
    case .failure(let error): print("Could not auto-connect to server.", error)
    case .success(let connection):
        connection.enableUnsignedCodeExecution { result in
            switch result
            {
            case .failure(let error): print("Could not enable JIT compilation.", error)
            case .success: 
                print("Successfully enabled JIT compilation!")
                ServerManager.shared.stopDiscovering()
            }
            
            connection.disconnect()
        }
    }
}
```

### Objective-C
```objc
@import AltKit;

[[ALTServerManager sharedManager] startDiscovering];

[[ALTServerManager sharedManager] autoconnectWithCompletionHandler:^(ALTServerConnection *connection, NSError *error) {
    if (error)
    {
        return NSLog(@"Could not auto-connect to server. %@", error);
    }
    
    [connection enableUnsignedCodeExecutionWithCompletionHandler:^(BOOL success, NSError *error) {
        if (success)
        {
            NSLog(@"Successfully enabled JIT compilation!");
            [[ALTServerManager sharedManager] stopDiscovering];
        }
        else
        {
            NSLog(@"Could not enable JIT compilation. %@", error);
        }
        
        [connection disconnect];
    }];
}];
```
