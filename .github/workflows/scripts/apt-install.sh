#!/bin/sh
# Installs Ubuntu packages for a CI job, from the job's apt cache when it
# has them.
#
#   apt-install.sh [--no-install-recommends] PACKAGE...
#
# APT_CACHE (default ~/.apt-cache) holds the package lists and the .deb
# files of the last install; actions/cache restores it before this runs
# and saves it after (see apt-cache-key.sh for the key). With it warm the
# packages install without the network: no apt-get update, no download,
# only the unpacking. Cold, or when the cached lists no longer resolve
# (a new runner image), the lists are refreshed and the packages fetched
# into it, under a bound, with the main archive tried when the runner's
# mirror stalls - and the cache is marked to be saved again.
#
# Packages already on the runner are left as they are, and when all of
# them are, nothing is done at all.
set -eu

opts=
while [ $# -gt 0 ]; do
   case $1 in
      --no-install-recommends) opts="$opts $1"; shift ;;
      -*) echo "apt-install.sh: unknown option $1"; exit 2 ;;
      *) break ;;
   esac
done
[ $# -gt 0 ] || { echo "apt-install.sh: no packages"; exit 2; }
pkgs="$*"
cache=${APT_CACHE:-$HOME/.apt-cache}

if dpkg -s $pkgs >/dev/null 2>&1; then
   echo "apt-install.sh: all present on the runner; nothing to install."
   exit 0
fi

mkdir -p "$cache/archives/partial" "$cache/lists/partial"
dirs="-o Dir::Cache::Archives=$cache/archives -o Dir::State::Lists=$cache/lists"
net="-o Acquire::Retries=3 -o Acquire::http::Timeout=30"
# No package may stop the install to ask a question
apt="sudo env DEBIAN_FRONTEND=noninteractive apt-get"

saved() {
   # A later plain apt-get install in the job resolves from the same
   # lists, and the cache action runs as the runner's user
   sudo cp -a "$cache/lists/." /var/lib/apt/lists/
   sudo rm -f /var/lib/apt/lists/.complete
   sudo chown -R "$(id -u):$(id -g)" "$cache"
}

# Warm: everything from the cache
if [ -f "$cache/lists/.complete" ] \
      && $apt install -y --no-download $dirs $opts $pkgs; then
   echo "apt-install.sh: installed from the cache."
   saved
   exit 0
fi

echo "apt-install.sh: refreshing the cache."
rm -f "$cache/lists/.complete"
upd="$net -o Acquire::Languages=none
   -o Acquire::IndexTargets::deb::CNF::DefaultEnabled=false"
for i in 1 2 3; do
   sudo timeout 120 apt-get update -y $dirs $upd && break
   [ $i = 3 ] && exit 1
   echo "apt-get update failed (attempt $i); retrying in 10s"
   sleep 10
done

# Fetch first, under a bound: a download is safe to stop and resumes, an
# install is not. A mirror that stalls is left for the main archive.
main=archive.ubuntu.com/ubuntu
case $(dpkg --print-architecture) in
   amd64|i386) ;;
   *) main=ports.ubuntu.com/ubuntu-ports ;;
esac
for i in 1 2 3; do
   sudo timeout 240 apt-get install -y --download-only $dirs $net $opts $pkgs \
      && break
   [ $i = 3 ] && exit 1
   echo "package download failed or stalled (attempt $i); retrying from ${main%%/*}"
   for f in /etc/apt/sources.list /etc/apt/sources.list.d/*; do
      [ -f "$f" ] || continue
      sudo sed -i -e "s#mirror+file:/etc/apt/apt-mirrors.txt#http://$main/#g" \
         -e "s#http://azure\.$main#http://$main#g" "$f"
   done
   sudo timeout 120 apt-get update -y $dirs $upd || true
done

$apt install -y $dirs $net $opts $pkgs
# Only what this job installs is kept: the lists, and its own packages
sudo touch "$cache/lists/.complete"
sudo rm -rf "$cache/archives/partial" "$cache/lists/partial"
saved
if [ -n "${GITHUB_ENV:-}" ]; then
   echo "APT_CACHE_SAVE=1" >> "$GITHUB_ENV"
fi
