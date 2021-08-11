## Building
```sh
make -f Makefile.webos clean
make -f Makefile.webos -j$(ncpu --all) ipk
```

## Testing
```sh
# Install and launch via ares-launch
make -f Makefile.webos launch

# Start installed application via SSH
XDG_RUNTIME_DIR=/tmp/xdg /usr/bin/jailer -t native_devmode -i com.retroarch -p /media/developer/apps/usr/palm/applications/com.retroarch /media/developer/apps/usr/palm/applications/com.retroarch/retroarch --verbose --verbose
```
