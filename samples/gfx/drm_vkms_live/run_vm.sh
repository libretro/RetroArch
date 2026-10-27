#!/bin/sh
# Runs a test binary in a qemu VM whose kernel has vkms loaded with its
# overlay planes and writeback connector, and exits as the test did.
#
#   run_vm.sh ./drm_vkms_live_test [args]
#   run_vm.sh --kernel-version     (prints the kernel it would fetch)
#
# KERNEL_DIR, when set, names a directory holding vmlinuz and vkms.ko;
# otherwise Ubuntu's current HWE kernel is fetched with apt-get download
# into VKMS_CACHE (default .kernel here) and taken from there. Needs
# qemu-system-x86_64, a static busybox, zstd and cpio; uses KVM where
# /dev/kvm can be opened, and plain emulation otherwise.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
. "$here/fetch_debs.sh"

hwe_kernel() {
   apt-cache depends linux-image-generic-hwe-24.04 2>/dev/null \
      | sed -n 's/.*Depends: linux-image-\([0-9][^ ]*-generic\)$/\1/p' \
      | head -n 1
}
# Lists the runner never refreshed name no kernel: refreshed once
if [ -z "$(hwe_kernel)" ] && command -v sudo >/dev/null 2>&1; then
   sudo timeout 120 apt-get update -qq >/dev/null 2>&1 || true
fi
if [ "${1:-}" = --kernel-version ]; then
   hwe_kernel
   exit 0
fi

test_bin=$1
shift
cache=${VKMS_CACHE:-$here/.kernel}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for t in qemu-system-x86_64 busybox cpio gzip; do
   command -v $t >/dev/null || { echo "run_vm.sh: $t is missing"; exit 1; }
done

if [ -z "${KERNEL_DIR:-}" ]; then
   KERNEL_DIR=$cache
   if [ ! -f "$KERNEL_DIR/vmlinuz" ] || [ ! -f "$KERNEL_DIR/vkms.ko" ]; then
      kver=$(hwe_kernel)
      [ -n "$kver" ] || { echo "run_vm.sh: no HWE kernel in apt"; exit 1; }
      echo "run_vm.sh: fetching $kver"
      mkdir -p "$KERNEL_DIR" "$work/deb"
      download_debs "$work/deb" "linux-image-unsigned-$kver" \
         "linux-modules-$kver" \
         || { echo "run_vm.sh: could not fetch $kver"; exit 1; }
      # vkms is in the modules, or in the extra ones, by kernel
      download_debs "$work/deb" "linux-modules-extra-$kver" || true
      for d in "$work"/deb/*.deb; do
         dpkg-deb -x "$d" "$work/root"
      done
      cp "$work/root/boot/vmlinuz-$kver" "$KERNEL_DIR/vmlinuz"
      ko=$(find "$work/root/lib/modules" -name 'vkms.ko*' | head -n 1)
      [ -n "$ko" ] || { echo "run_vm.sh: $kver has no vkms"; exit 1; }
      case $ko in
         *.zst) zstd -dqf "$ko" -o "$KERNEL_DIR/vkms.ko" ;;
         *.xz)  xz -dc "$ko" > "$KERNEL_DIR/vkms.ko" ;;
         *)     cp "$ko" "$KERNEL_DIR/vkms.ko" ;;
      esac
      # vkms must need nothing the kernel lacks built in
      if strings "$KERNEL_DIR/vkms.ko" | grep -q '^depends=.\+'; then
         echo "run_vm.sh: vkms in $kver depends on modules:"
         strings "$KERNEL_DIR/vkms.ko" | grep '^depends='
         rm -f "$KERNEL_DIR/vkms.ko"
         exit 1
      fi
   fi
fi

# The initramfs: busybox, vkms, the test and the libraries it links
ir=$work/ir
mkdir -p "$ir/bin" "$ir/proc" "$ir/sys" "$ir/dev" "$ir/tmp"
cp "$(command -v busybox)" "$ir/bin/busybox"
cp "$KERNEL_DIR/vkms.ko" "$ir/vkms.ko"
cp "$test_bin" "$ir/test"
for lib in $(ldd "$test_bin" | sed -n 's/.*=> \(\/[^ ]*\).*/\1/p; s/^[[:space:]]*\(\/[^ ]*\).*/\1/p'); do
   mkdir -p "$ir$(dirname "$lib")"
   cp -L "$lib" "$ir$lib"
done
cat > "$ir/init" <<EOF
#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev 2>/dev/null
export ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
if insmod /vkms.ko enable_overlay=1 enable_writeback=1; then
   /test $*
   echo "VKMS-RC=\$?"
else
   echo "VKMS-RC=1 (vkms did not load)"
fi
poweroff -f
EOF
chmod +x "$ir/init"
(cd "$ir" && find . | cpio -o -H newc 2>/dev/null | gzip -1 > "$work/initrd.gz")

accel="-cpu max"
if [ -w /dev/kvm ]; then
   accel="-enable-kvm -cpu host"
fi
timeout 900 qemu-system-x86_64 $accel -m 1024 -smp 2 -nographic -vga none \
   -no-reboot -kernel "$KERNEL_DIR/vmlinuz" -initrd "$work/initrd.gz" \
   -append "console=ttyS0 panic=-1 quiet" > "$work/log" 2>&1 || true

sed -n '/^\(   \|PASS\|FAIL\|==\|SUMMARY\|\[skip\]\|\[INFO\]\|\[WARN\]\|\[ERROR\]\)/p; /runtime error/p' "$work/log" \
   | tr -d '\r'
rc=$(sed -n 's/.*VKMS-RC=\([0-9]*\).*/\1/p' "$work/log" | tail -n 1)
if [ -z "$rc" ]; then
   echo "run_vm.sh: the VM did not finish; its console:"
   tail -n 40 "$work/log"
   exit 1
fi
exit "$rc"
