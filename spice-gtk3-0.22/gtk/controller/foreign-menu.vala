// Copyright (C) 2012 Red Hat, Inc.

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, see <http://www.gnu.org/licenses/>.

using Custom;

namespace SpiceCtrl {

public class ForeignMenu: Object {

	public Menu menu { get; private set; }
    public string title { get; private set; }

	public signal void client_connected ();

	private int nclients;
	private List<IOStream> clients;

	public ForeignMenu() {
		menu = new Menu ();
	}

	public void menu_item_click_msg (int32 item_id) {
		debug ("clicked id: %d".printf (item_id));

		var msg = SpiceProtocol.ForeignMenu.Event ();
		msg.base.size = (uint32)sizeof (SpiceProtocol.ForeignMenu.Event);
		msg.base.id = SpiceProtocol.ForeignMenu.MsgId.ITEM_EVENT;
		msg.id = item_id;
		msg.action = SpiceProtocol.ForeignMenu.EventType.CLICK;

		unowned uint8[] p = ((uint8[])(&msg))[0:msg.base.size];
		send_msg.begin (p);
	}

	public void menu_item_checked_msg (int32 item_id, bool checked = true) {
		debug ("%schecked id: %d".printf (checked ? "" : "un", item_id));

		var msg = SpiceProtocol.ForeignMenu.Event ();
		msg.base.size = (uint32)sizeof (SpiceProtocol.ForeignMenu.Event);
		msg.base.id = SpiceProtocol.ForeignMenu.MsgId.ITEM_EVENT;
		msg.id = item_id;
		msg.action = checked ?
			SpiceProtocol.ForeignMenu.EventType.CHECKED :
			SpiceProtocol.ForeignMenu.EventType.UNCHECKED;

		unowned uint8[] p = ((uint8[])(&msg))[0:msg.base.size];
		send_msg.begin (p);
	}

	public void app_activated_msg (bool activated = true) {
		var msg = SpiceProtocol.ForeignMenu.Msg ();
		msg.size = (uint32)sizeof (SpiceProtocol.ForeignMenu.Event);
		msg.id = activated ?
			SpiceProtocol.ForeignMenu.MsgId.APP_ACTIVATED :
			SpiceProtocol.ForeignMenu.MsgId.APP_DEACTIVATED;

		unowned uint8[] p = ((uint8[])(&msg))[0:msg.size];
		send_msg.begin (p);
	}

	public async bool send_msg (owned uint8[] p) throws GLib.Error {
		// vala FIXME: pass Controller.Msg instead
		// vala doesn't keep reference on the struct in async methods
		// it copies only base, which is not enough to transmit the whole
		// message.
		try {
			foreach (var c in clients) {
				yield output_stream_write (c.output_stream, p);
			}
		} catch (GLib.Error e) {
			warning (e.message);
		}

		return true;
	}

	SpiceProtocol.Controller.MenuFlags get_menu_flags (uint32 type) {
		SpiceProtocol.Controller.MenuFlags flags = 0;

		if ((SpiceProtocol.ForeignMenu.MenuFlags.CHECKED & type) != 0)
			flags |= SpiceProtocol.Controller.MenuFlags.CHECKED;
		if ((SpiceProtocol.ForeignMenu.MenuFlags.DIM & type) != 0)
			flags |= SpiceProtocol.Controller.MenuFlags.GRAYED;

		return flags;
	}

	private bool handle_message (SpiceProtocol.ForeignMenu.Msg* msg) {
		switch (msg.id) {
		case SpiceProtocol.ForeignMenu.MsgId.SET_TITLE:
			var t = (SpiceProtocol.ForeignMenu.SetTitle*)(msg);
			title = t.string;
			break;
		case SpiceProtocol.ForeignMenu.MsgId.ADD_ITEM:
			var i = (SpiceProtocol.ForeignMenu.AddItem*)(msg);
			debug ("add id:%u type:%u position:%u title:%s", i.id, i.type, i.position, i.string);
			menu.items.append (new MenuItem ((int)i.id, i.string, get_menu_flags (i.type)));
			notify_property ("menu");
			break;
		case SpiceProtocol.ForeignMenu.MsgId.MODIFY_ITEM:
			debug ("deprecated: modify item");
			break;
		case SpiceProtocol.ForeignMenu.MsgId.REMOVE_ITEM:
			var i = (SpiceProtocol.ForeignMenu.RmItem*)(msg);
			debug ("not implemented: remove id:%u".printf (i.id));
			break;
		case SpiceProtocol.ForeignMenu.MsgId.CLEAR:
			menu = new Menu ();
			break;
		default:
			warn_if_reached ();
			return false;
		}
		return true;
	}

	private async void handle_client (IOStream c) throws GLib.Error {
		debug ("new socket client, reading init header");

		var p = new uint8[sizeof(SpiceProtocol.ForeignMenu.InitHeader)];
		var header = (SpiceProtocol.ForeignMenu.InitHeader*)p;
		yield input_stream_read (c.input_stream, p);
		if (warn_if (header.magic != SpiceProtocol.ForeignMenu.MAGIC))
			return;
		if (warn_if (header.version != SpiceProtocol.ForeignMenu.VERSION))
			return;
		if (warn_if (header.size < sizeof (SpiceProtocol.ForeignMenu.Init)))
			return;

		var cp = new uint8[sizeof(uint64)];
		yield input_stream_read (c.input_stream, cp);
		uint64 credentials = *(uint64*)cp;
		if (warn_if (credentials != 0))
			return;

		var title_size = header.size - sizeof(SpiceProtocol.ForeignMenu.Init);
		var title = new uint8[title_size + 1];
		yield c.input_stream.read_async (title[0:title_size]);
		this.title = (string)title;

		client_connected ();

		for (;;) {
			var t = new uint8[sizeof(SpiceProtocol.ForeignMenu.Msg)];
			yield input_stream_read (c.input_stream, t);
			var msg = (SpiceProtocol.ForeignMenu.Msg*)t;
			debug ("new message " + msg.id.to_string () + "size " + msg.size.to_string ());

			if (warn_if (msg.size < sizeof (SpiceProtocol.ForeignMenu.Msg)))
				break;

			if (msg.size > sizeof (SpiceProtocol.ForeignMenu.Msg)) {
				t.resize ((int)msg.size);
				msg = (SpiceProtocol.ForeignMenu.Msg*)t;

				yield input_stream_read (c.input_stream, t[sizeof(SpiceProtocol.ForeignMenu.Msg):msg.size]);
			}

			handle_message (msg);
		}

	}

	public async void listen (string? addr = null) throws GLib.Error, SpiceCtrl.Error
	{
		var listener = Spice.ForeignMenuListener.new_listener (addr);

		for (;;) {
			var c = yield listener.accept_async ();
			nclients += 1;
			clients.append (c);
			try {
				yield handle_client (c);
			} catch (GLib.Error e) {
				warning (e.message);
			}
			c.close ();
			clients.remove (c);
			nclients -= 1;
		}
	}

}

} // SpiceCtrl
