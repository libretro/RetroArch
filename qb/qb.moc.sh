. qb/config.moc.sh

add_define MAKEFILE QT_VERSION "$QT_VERSION"

MOC="${MOC:-}"

add_opt MOC no
if [ "$HAVE_QT" = "yes" ]; then
	printf %s 'Checking for moc ... '

	# moc_start ran the check in the background once Qt was settled; it
	# runs again here if nothing started it or Qt changed after it did.
	if [ -n "$MOC_PID" ] && [ "$MOC_SIG" != "$CXX|$QT_VERSION|$QT_FLAGS" ]; then
		wait "$MOC_PID"
		MOC_PID=''
	fi
	[ -n "$MOC_PID" ] || moc_start

	moc_works=0
	wait "$MOC_PID" && moc_works=1
	MOC_PID=''
	read -r MOC < "$TEMP_MOC_RES"
	cat "$TEMP_MOC_LOG" >> config.log

	moc_status='does not work'
	if [ "$moc_works" = '1' ]; then
		moc_status='works'
		HAVE_MOC='yes'
	elif [ -z "$MOC" ]; then
		moc_status='not found'
	fi

	printf %s\\n "$MOC $moc_status"

	if [ "$HAVE_MOC" != 'yes' ]; then
		HAVE_QT='no'
		die : 'Warning: moc not found, Qt companion support will be disabled.'
	fi
fi
