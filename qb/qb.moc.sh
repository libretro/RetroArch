. qb/config.moc.sh

TEMP_MOC=.moc.h
TEMP_CPP=.moc.cpp

MOC="${MOC:-}"

# Checking for working moc
cat << EOF > "$TEMP_MOC"
#include <QTimeZone>
class Test : public QObject
{
public:
   Q_OBJECT
   QTimeZone tz;
};
EOF

add_opt MOC no
if [ "$HAVE_QT" = "yes" ]; then
	printf %s 'Checking for moc ... '

	moc_works=0
	if [ "$MOC" ]; then
		QT_SELECT="$QT_VERSION" \
		"$MOC" -o "$TEMP_CPP" "$TEMP_MOC" >/dev/null 2>&1 &&
			$(printf %s "$CXX") -o "$TEMP_EXE" \
			$(printf %s "$QT_FLAGS") -fPIC -c "$TEMP_CPP" \
			>/dev/null 2>&1 &&
		moc_works=1
	else
		for moc in "moc-$QT_VERSION" moc; do
			MOC="$(exists "$moc")" || MOC=""
			if [ "$MOC" ]; then
				QT_SELECT="$QT_VERSION" \
				"$MOC" -o "$TEMP_CPP" "$TEMP_MOC" >/dev/null 2>&1 ||
					continue
				if $(printf %s "$CXX") -o "$TEMP_EXE" \
						$(printf %s "$QT_FLAGS") -fPIC -c \
						"$TEMP_CPP" >/dev/null 2>&1; then
					moc_works=1
					break
				fi
			fi
		done
	fi

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

rm -f -- "$TEMP_CPP" "$TEMP_EXE" "$TEMP_MOC"
