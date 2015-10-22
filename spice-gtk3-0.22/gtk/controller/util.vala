// Copyright (C) 2012 Red Hat, Inc.

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, see <http://www.gnu.org/licenses/>.

namespace SpiceCtrl {

	public async void input_stream_read (InputStream stream, uint8[] buffer) throws GLib.IOError {
		var length = buffer.length;
		ssize_t i = 0;

		while (i < length) {
			var n = yield stream.read_async (buffer[i:length]);
			if (n == 0)
				throw new GLib.IOError.CLOSED ("closed stream") ;
			i += n;
		}
	}

	public async void output_stream_write (OutputStream stream, owned uint8[] buffer) throws GLib.IOError {
		var length = buffer.length;
		ssize_t i = 0;

		while (i < length) {
			var n = yield stream.write_async (buffer[i:length]);
			if (n == 0)
				throw new GLib.IOError.CLOSED ("closed stream") ;
			i += n;
		}
	}

}