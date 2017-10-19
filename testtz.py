#!/usr/bin/python3

import gi
gi.require_version("TimezoneMap", "1.0")
gi.require_version("Gtk", "3.0")
from gi.repository import TimezoneMap, Gtk

def on_location_change(map, location, user_data=None):
    print("Location changed to %s (%s, %s)" % (location.get_en_name(), location.get_latitude(), location.get_longitude()))

window = Gtk.Window(default_width=1024, default_height=476)
map = TimezoneMap.TimezoneMap()
map.set_watermark('yo wassup man')
window.add(map)
window.connect("delete-event", Gtk.main_quit)
window.show_all()
#map.set_location(911, 373)
#map.simulate()
map.set_timezone("Australia/Brisbane")
map.connect("location-changed", on_location_change)
Gtk.main()

