#!/bin/sh

DAT_dir=dat
lua_RDB_outdir=rdb_lua
c_RDB_outdir=rdb_c

echo
echo "==========================================================="
echo "================== running LUA converter =================="
echo "==========================================================="
echo

rm -rf $lua_RDB_outdir
mkdir -p $lua_RDB_outdir

for dat_file in $DAT_dir/*.dat ; do
   name=`echo "$dat_file" | sed "s/${DAT_dir}\/*//"`
   name=`echo "$name" | sed "s/\.dat//"`
   ./lua_converter "$lua_RDB_outdir/$name.rdb" dat_converter.lua "$dat_file"
done
./lua_converter "$lua_RDB_outdir/merged.rdb" dat_converter.lua rom.sha1 $DAT_dir/N*.dat

echo
echo "==========================================================="
echo "=================== running C converter ==================="
echo "==========================================================="
echo

rm -rf $c_RDB_outdir
mkdir -p $c_RDB_outdir

for dat_file in $DAT_dir/*.dat ; do
   name=`echo "$dat_file" | sed "s/${DAT_dir}\/*//"`
   name=`echo "$name" | sed "s/\.dat//"`
   ./c_converter "$c_RDB_outdir/$name.rdb" "$dat_file"
done
./c_converter "$c_RDB_outdir/merged.rdb" rom.sha1 $DAT_dir/N*.dat

echo
echo "==========================================================="
echo "==================== comparing files ====================="
echo "==========================================================="
echo

matches=0
failed=0

for lua_rdb_file in $lua_RDB_outdir/*.rdb ; do
   name=`echo "$lua_rdb_file" | sed "s/${lua_RDB_outdir}\/*//"`
   name=`echo "$name" | sed "s/\.rdb//"`

   files_differ=0
   diff "$c_RDB_outdir/$name.rdb" "$lua_RDB_outdir/$name.rdb" && files_differ=1
   if [ $files_differ = 0 ]; then
      failed=$(( $failed + 1 ))
      ls -la "$c_RDB_outdir/$name.rdb"
      ls -la "$lua_RDB_outdir/$name.rdb"
   else
      matches=$(( $matches + 1 ))
   fi
done

echo
echo "==========================================================="
echo
echo "tested $(( $matches + $failed )) files: $matches match and $failed differ"
echo
if [ $failed = 0 ]; then
   echo "test successful !!"
else
   echo "test failed !!"
fi
echo
echo "==========================================================="
echo
