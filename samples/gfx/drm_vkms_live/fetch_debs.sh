#!/bin/sh

download_debs() {
   dest=$1
   shift
   if (cd "$dest" && timeout -k 5 180 apt-get download \
         -o Acquire::Retries=1 -o Acquire::http::Timeout=30 \
         -o Acquire::https::Timeout=30 "$@"); then
      return 0
   fi
   echo "run_vm.sh: mirror failed; fetching verified packages from archive.ubuntu.com"
   timeout -k 5 30 apt-get --print-uris download "$@" > "$dest/uris" || return 1
   [ -s "$dest/uris" ] || return 1
   while read -r uri filename size checksum; do
      uri=${uri#\'}
      uri=${uri%\'}
      case $uri in
         http://*/pool/*|https://*/pool/*|mirror+file:*/pool/*) ;;
         *) echo "run_vm.sh: unsupported package URI: $uri"; return 1 ;;
      esac
      case $filename in
         ''|*[!a-zA-Z0-9.+_~:-]*) return 1 ;;
      esac
      case $checksum in
         SHA256:*|SHA512:*) ;;
         *) echo "run_vm.sh: missing strong package checksum"; return 1 ;;
      esac
      timeout -k 5 180 /usr/lib/apt/apt-helper \
         -o Acquire::Retries=1 -o Acquire::https::Timeout=30 download-file \
         "https://archive.ubuntu.com/ubuntu/pool/${uri#*/pool/}" \
         "$dest/$filename" "$checksum" || return 1
   done < "$dest/uris"
}
