#!/usr/bin/python
import time


from xml.dom.minidom import parse
dom1 = parse("AndroidManifest.xml")
oldVersion = dom1.documentElement.getAttribute("android:versionCode")
versionNumbers = oldVersion.split('.')

versionName = dom1.documentElement.getAttribute("android:versionName")
versionName = versionName + "_GIT"

versionNumbers[-1] = unicode(int(time.time()))
dom1.documentElement.setAttribute("android:versionCode", u'.'.join(versionNumbers))
dom1.documentElement.setAttribute("android:versionName", versionName)

with open("AndroidManifest.xml", 'wb') as f:
    for line in dom1.toxml("utf-8"):
        f.write(line)
