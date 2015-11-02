// Copyright (C) 2011 Red Hat, Inc.

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

using GLib;
using Custom;
using Win32;
using Spice;
using SpiceProtocol;

namespace SpiceCtrl {

public errordomain Error {
	VALUE,
}

public class Controller: Object {
	public string host { private set; get; }
	public uint32 port { private set; get; }
	public uint32 sport { private set; get; }
	public string password { private set; get; }
	public SpiceProtocol.Controller.Display display_flags { private set; get; }
	public string tls_ciphers { private set; get; }
	public string host_subject { private set; get; }
	public string ca_file { private set; get; }
	public string title { private set; get; }
	public string hotkeys { private set; get; }
	public string[] secure_channels { private set; get; }
	public string[] disable_channels { private set; get; }
	public SpiceCtrl.Menu? menu  { private set; get; }
	public bool enable_smartcard { private set; get; }
	public bool send_cad { private set; get; }
	public string[] disable_effects {private set; get; }
	public uint32 color_depth {private set; get; }
	public bool enable_usbredir { private set; get; }
	public bool enable_usb_autoshare { private set; get; }
	public string usb_filter { private set; get; }
	public string proxy { private set; get; }

	public signal void do_connect ();
	public signal void show ();
	public signal void hide ();

	public signal void client_connected ();

	public void menu_item_click_msg (int32 item_id) {
		var msg = SpiceProtocol.Controller.MsgValue ();
		msg.base.size = (uint32)sizeof (SpiceProtocol.Controller.MsgValue);
		msg.base.id = SpiceProtocol.Controller.MsgId.MENU_ITEM_CLICK;
		msg.value = item_id;
		unowned uint8[] p = ((uint8[])(&msg))[0:msg.base.size];
		send_msg.begin (p);
	}

	public async bool send_msg (uint8[] p) throws GLib.Error {
		// vala FIXME: pass Controller.Msg instead
		// vala doesn't keep reference on the struct in async methods
		// it copies only base, which is not enough to transmit the whole
		// message.
		try {
			if (excl_connection != null) {
				yield output_stream_write (excl_connection.output_stream, p);
			} else {
				foreach (var c in clients)
					yield output_stream_write (c.output_stream, p);
			}
		} catch (GLib.Error e) {
			warning (e.message);
		}

		return true;
	}

	private GLib.IOStream? excl_connection;
	private int nclients;
	List<IOStream> clients;

	private bool handle_message (SpiceProtocol.Controller.Msg* msg) {
		var v = (SpiceProtocol.Controller.MsgValue*)(msg);
		var d = (SpiceProtocol.Controller.MsgData*)(msg);
		unowned string str = (string)(&d.data);

		switch (msg.id) {
		case SpiceProtocol.Controller.MsgId.HOST:
			host = str;
			debug ("got HOST: %s".printf (str));
			break;
		case SpiceProtocol.Controller.MsgId.PORT:
			port = v.value;
			debug ("got PORT: %u".printf (port));
			break;
		case SpiceProtocol.Controller.MsgId.SPORT:
			sport = v.value;
			debug ("got SPORT: %u".printf (sport));
			break;
		case SpiceProtocol.Controller.MsgId.PASSWORD:
			password = str;
			debug ("got PASSWORD");
			break;

		case SpiceProtocol.Controller.MsgId.SECURE_CHANNELS:
			secure_channels = str.split(",");
			debug ("got SECURE_CHANNELS %s".printf (str));
			break;

		case SpiceProtocol.Controller.MsgId.DISABLE_CHANNELS:
			disable_channels = str.split(",");
			debug ("got DISABLE_CHANNELS %s".printf (str));
			break;

		case SpiceProtocol.Controller.MsgId.TLS_CIPHERS:
			tls_ciphers = str;
			debug ("got TLS_CIPHERS %s".printf (str));
			break;
		case SpiceProtocol.Controller.MsgId.CA_FILE:
			ca_file = str;
			debug ("got CA_FILE %s".printf (str));
			break;
		case SpiceProtocol.Controller.MsgId.HOST_SUBJECT:
			host_subject = str;
			debug ("got HOST_SUBJECT %s".printf (str));
			break;

		case SpiceProtocol.Controller.MsgId.FULL_SCREEN:
			display_flags = (SpiceProtocol.Controller.Display)v.value;
			debug ("got FULL_SCREEN 0x%x".printf (v.value));
			break;
		case SpiceProtocol.Controller.MsgId.SET_TITLE:
			title = str;
			debug ("got TITLE %s".printf (str));
			break;
		case SpiceProtocol.Controller.MsgId.ENABLE_SMARTCARD:
			enable_smartcard = (bool)v.value;
			debug ("got ENABLE_SMARTCARD 0x%x".printf (v.value));
			break;

		case SpiceProtocol.Controller.MsgId.CREATE_MENU:
			menu = new SpiceCtrl.Menu.from_string (str);
			debug ("got CREATE_MENU %s".printf (str));
			break;
		case SpiceProtocol.Controller.MsgId.DELETE_MENU:
			menu = null;
			debug ("got DELETE_MENU request");
			break;

		case SpiceProtocol.Controller.MsgId.SEND_CAD:
			send_cad = (bool)v.value;
			debug ("got SEND_CAD %u".printf (v.value));
			break;

		case SpiceProtocol.Controller.MsgId.HOTKEYS:
			hotkeys = str;
			debug ("got HOTKEYS %s".printf (str));
			break;

		case SpiceProtocol.Controller.MsgId.COLOR_DEPTH:
			color_depth = v.value;
			debug ("got COLOR_DEPTH %u".printf (v.value));
			break;
		case SpiceProtocol.Controller.MsgId.DISABLE_EFFECTS:
			disable_effects = str.split(",");
			debug ("got DISABLE_EFFECTS %s".printf (str));
			break;

		case SpiceProtocol.Controller.MsgId.CONNECT:
			do_connect ();
			debug ("got CONNECT request");
			break;
		case SpiceProtocol.Controller.MsgId.SHOW:
			show ();
			debug ("got SHOW request");
			break;
		case SpiceProtocol.Controller.MsgId.HIDE:
			hide ();
			debug ("got HIDE request");
			break;
		case SpiceProtocol.Controller.MsgId.ENABLE_USB:
			enable_usbredir = (bool)v.value;
			debug ("got ENABLE_USB %u".printf (v.value));
			break;
		case SpiceProtocol.Controller.MsgId.ENABLE_USB_AUTOSHARE:
			enable_usb_autoshare = (bool)v.value;
			debug ("got ENABLE_USB_AUTOSHARE %u".printf (v.value));
			break;
		case SpiceProtocol.Controller.MsgId.USB_FILTER:
			usb_filter = str;
			debug ("got USB_FILTER %s".printf (str));
			break;
		case SpiceProtocol.Controller.MsgId.PROXY:
			proxy = str;
			debug ("got PROXY %s".printf (str));
			break;
		default:
			debug ("got unknown msg.id %u".printf (msg.id));
			warn_if_reached ();
			return false;
		}
		return true;
	}

	private async void handle_client (IOStream c) throws GLib.Error {
		var excl = false;

		debug ("new socket client, reading init header");

		var p = new uint8[sizeof(SpiceProtocol.Controller.Init)];
		var init = (SpiceProtocol.Controller.Init*)p;
		yield input_stream_read (c.input_stream, p);
		if (warn_if (init.base.magic != SpiceProtocol.Controller.MAGIC))
			return;
		if (warn_if (init.base.version != SpiceProtocol.Controller.VERSION))
			return;
		if (warn_if (init.base.size < sizeof (SpiceProtocol.Controller.Init)))
			return;
		if (warn_if (init.credentials != 0))
			return;
		if (warn_if (excl_connection != null))
			return;

		excl = (bool)(init.flags & SpiceProtocol.Controller.Flag.EXCLUSIVE);
		if (excl) {
			if (nclients > 1) {
				warning (@"Can't make the client exclusive, there is already $nclients connected clients");
				return;
			}
			excl_connection = c;
		}

		client_connected ();

		for (;;) {
			var t = new uint8[sizeof(SpiceProtocol.Controller.Msg)];
			yield input_stream_read (c.input_stream, t);
			var msg = (SpiceProtocol.Controller.Msg*)t;
			debug ("new message " + msg.id.to_string () + "size " + msg.size.to_string ());
			if (warn_if (msg.size < sizeof (SpiceProtocol.Controller.Msg)))
				break;

			if (msg.size > sizeof (SpiceProtocol.Controller.Msg)) {
				t.resize ((int)msg.size);
				msg = (SpiceProtocol.Controller.Msg*)t;
				yield input_stream_read (c.input_stream, t[sizeof(SpiceProtocol.Controller.Msg):msg.size]);
			}

			handle_message (msg);
		}

		if (excl)
			excl_connection = null;
	}

	public Controller() {
	}

	public async void listen (string? addr = null) throws GLib.Error, SpiceCtrl.Error
	{
		var listener = ControllerListener.new_listener (addr);

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
