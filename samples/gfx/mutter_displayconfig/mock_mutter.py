#!/usr/bin/env python3
# A stand-in for GNOME's org.gnome.Mutter.DisplayConfig, with the
# signatures of Mutter's own interface XML and the checks
# ApplyMonitorsConfig makes before it touches a monitor: the serial
# must be the current one, the method known, every connector known,
# every mode one of that connector's, every scale one of that mode's,
# and every lit monitor configured.  A config that passes becomes the
# current state (serial bumped), so a second call sees the first.
#
# Two monitors side by side: DP-1, primary, a 2560x1440 panel at
# 240/120/60 Hz (plus 1920x1080) in HDR colour mode, and HDMI-1 to its
# right with underscanning on.  DP-1 lists its 240 Hz mode twice, fixed
# and variable rate, the way Mutter 47+ does.
#
# Test-only methods on the same interface: LastApply (what the last
# accepted ApplyMonitorsConfig asked for, as one line), Calls (how many
# GetCurrentState / ApplyMonitorsConfig calls came in) and Reset.

import sys
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

NAME = "org.gnome.Mutter.DisplayConfig"
PATH = "/org/gnome/Mutter/DisplayConfig"

def mode(mid, w, h, hz, scales, current=False):
    return {"id": mid, "w": w, "h": h, "hz": hz, "scales": scales,
            "current": current}

def initial():
    return {
        "serial": 7,
        "monitors": [
            {"connector": "DP-1", "props": {"color-mode": dbus.UInt32(1),
                                            "supported-color-modes":
                                                dbus.Array([dbus.UInt32(0), dbus.UInt32(1)], signature="u")},
             "modes": [
                 mode("2560x1440@239.913", 2560, 1440, 239.913, [1.0, 1.25, 1.5, 2.0], True),
                 mode("2560x1440@239.913+vrr", 2560, 1440, 239.913, [1.0, 1.25, 1.5, 2.0]),
                 mode("2560x1440@119.877", 2560, 1440, 119.877, [1.0, 1.25, 1.5, 2.0]),
                 mode("2560x1440@59.951", 2560, 1440, 59.951, [1.0, 1.25, 1.5, 2.0]),
                 mode("1920x1080@60.000", 1920, 1080, 60.0, [1.0, 1.25]),
             ]},
            {"connector": "HDMI-1", "props": {"is-underscanning": True},
             "modes": [
                 mode("1920x1080@60.000", 1920, 1080, 60.0, [1.0], True),
                 mode("1280x720@60.000", 1280, 720, 60.0, [1.0]),
             ]},
        ],
        # x, y, scale, transform, primary, connectors
        "logical": [
            [0, 0, 1.0, 0, True, ["DP-1"]],
            [2560, 0, 1.0, 0, False, ["HDMI-1"]],
        ],
    }

class DisplayConfig(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, PATH)
        self.st = initial()
        self.last = ""
        self.n_get = 0
        self.n_apply = 0

    @dbus.service.method(NAME, in_signature="",
                         out_signature="ua((ssss)a(siiddada{sv})a{sv})a(iiduba(ssss)a{sv})a{sv}")
    def GetCurrentState(self):
        self.n_get += 1
        mons = []
        for m in self.st["monitors"]:
            modes = []
            for md in m["modes"]:
                props = {"is-current": dbus.Boolean(md["current"])}
                if md["current"]:
                    props["is-preferred"] = dbus.Boolean(True)
                modes.append((md["id"], dbus.Int32(md["w"]), dbus.Int32(md["h"]),
                              dbus.Double(md["hz"]), dbus.Double(1.0),
                              dbus.Array([dbus.Double(s) for s in md["scales"]], signature="d"),
                              dbus.Dictionary(props, signature="sv")))
            spec = (m["connector"], "VND", "Panel", "0001")
            mprops = dict(m["props"])
            mprops["display-name"] = m["connector"]
            mons.append((spec, dbus.Array(modes, signature="(siiddada{sv})"),
                         dbus.Dictionary(mprops, signature="sv")))
        lms = []
        for x, y, sc, tr, pr, cons in self.st["logical"]:
            specs = [(c, "VND", "Panel", "0001") for c in cons]
            lms.append((dbus.Int32(x), dbus.Int32(y), dbus.Double(sc), dbus.UInt32(tr),
                        dbus.Boolean(pr), dbus.Array(specs, signature="(ssss)"),
                        dbus.Dictionary({}, signature="sv")))
        props = {"layout-mode": dbus.UInt32(1),
                 "supports-changing-layout-mode": dbus.Boolean(True)}
        return (dbus.UInt32(self.st["serial"]),
                dbus.Array(mons, signature="((ssss)a(siiddada{sv})a{sv})"),
                dbus.Array(lms, signature="(iiduba(ssss)a{sv})"),
                dbus.Dictionary(props, signature="sv"))

    @dbus.service.method(NAME, in_signature="uua(iiduba(ssa{sv}))a{sv}",
                         out_signature="")
    def ApplyMonitorsConfig(self, serial, method, logical, props):
        self.n_apply += 1
        err = "org.freedesktop.DBus.Error.AccessDenied"
        if serial != self.st["serial"]:
            raise dbus.exceptions.DBusException("The requested configuration is based on stale information", name=err)
        if method not in (0, 1, 2):
            raise dbus.exceptions.DBusException("Invalid method", name="org.freedesktop.DBus.Error.InvalidArgs")
        mons = {m["connector"]: m for m in self.st["monitors"]}
        seen = set()
        new_logical = []
        chosen = {}
        parts = []
        for x, y, sc, tr, pr, monspecs in logical:
            cons = []
            mparts = []
            for con, mid, mprops in monspecs:
                if con not in mons:
                    raise dbus.exceptions.DBusException("Invalid connector " + con, name="org.freedesktop.DBus.Error.InvalidArgs")
                md = [md for md in mons[con]["modes"] if md["id"] == mid]
                if not md:
                    raise dbus.exceptions.DBusException("Invalid mode " + mid, name="org.freedesktop.DBus.Error.InvalidArgs")
                if not any(abs(s - sc) < 0.0001 for s in md[0]["scales"]):
                    raise dbus.exceptions.DBusException("Scale not supported", name="org.freedesktop.DBus.Error.InvalidArgs")
                seen.add(con)
                cons.append(con)
                chosen[con] = mid
                mparts.append("%s=%s{%s}" % (con, mid, ",".join(
                    "%s:%s" % (k, (str(bool(v)).lower() if isinstance(v, dbus.Boolean) else int(v)))
                    for k, v in sorted(mprops.items()))))
            new_logical.append([int(x), int(y), float(sc), int(tr), bool(pr), cons])
            parts.append("%d,%d,%.2f,%d,%s[%s]" % (x, y, sc, tr, "p" if pr else "-", ";".join(mparts)))
        for m in self.st["monitors"]:
            if m["connector"] not in seen:
                raise dbus.exceptions.DBusException("Monitor " + m["connector"] + " not configured", name="org.freedesktop.DBus.Error.InvalidArgs")
        gp = ",".join("%s:%d" % (k, int(v)) for k, v in sorted(props.items()))
        self.last = "method=%d serial=%d lm=%s props={%s}" % (method, serial, " ".join(parts), gp)
        if method == 0:
            return
        for con, mid in chosen.items():
            for md in mons[con]["modes"]:
                md["current"] = (md["id"] == mid)
        self.st["logical"] = new_logical
        self.st["serial"] += 1
        # Mutter announces every change of configuration
        self.MonitorsChanged()

    @dbus.service.signal(NAME, signature="")
    def MonitorsChanged(self):
        pass

    @dbus.service.method(NAME, in_signature="", out_signature="s")
    def LastApply(self):
        return self.last

    @dbus.service.method(NAME, in_signature="", out_signature="uu")
    def Calls(self):
        return (dbus.UInt32(self.n_get), dbus.UInt32(self.n_apply))

    @dbus.service.method(NAME, in_signature="", out_signature="")
    def Reset(self):
        self.st = initial()
        self.last = ""
        self.n_get = 0
        self.n_apply = 0
        self.MonitorsChanged()

def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    obj = DisplayConfig(bus)
    # The BusName object owns the name; dropped, it releases it
    name = dbus.service.BusName(NAME, bus)
    print("mock mutter ready", flush=True)
    GLib.MainLoop().run()
    del obj, name

if __name__ == "__main__":
    sys.exit(main())
