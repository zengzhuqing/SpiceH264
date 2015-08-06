#!/usr/bin/env python

import os
import sys
from optparse import OptionParser
import traceback
from python_modules import spice_parser
from python_modules import ptypes
from python_modules import codegen
from python_modules import demarshal
from python_modules import marshal

def write_channel_enums(writer, channel, client, describe):
    messages = filter(lambda m : m.channel == channel, \
                          channel.client_messages if client else channel.server_messages)
    if len(messages) == 0:
        return
    if client:
        prefix = [ "MSGC" ]
    else:
        prefix = [ "MSG" ]
    if channel.member_name:
        prefix.append(channel.member_name.upper())
    if not describe:
        writer.begin_block("enum")
    else:
        writer.begin_block("static const value_string %s_vs[] = " % (codegen.prefix_underscore_lower(*[x.lower() for x in prefix])))
    i = 0
    prefix.append(None) # To be replaced with name
    for m in messages:
        prefix[-1] = m.name.upper()
        enum = codegen.prefix_underscore_upper(*prefix)
        if describe:
            writer.writeln("{ %s, \"%s %s\" }," % (enum, "Client" if client else "Server", m.name.upper()))
        else:
            if m.value == i:
                writer.writeln("%s," % enum)
                i = i + 1
            else:
                writer.writeln("%s = %s," % (enum, m.value))
                i = m.value + 1
    if describe:
        writer.writeln("{ 0, NULL }");
    else:
        if channel.member_name:
            prefix[-1] = prefix[-2]
            prefix[-2] = "END"
            writer.newline()
            writer.writeln("%s" % (codegen.prefix_underscore_upper(*prefix)))
    writer.end_block(semicolon=True)
    writer.newline()

def write_channel_type_enum(writer, describe=False):
    i = 0
    if describe:
        writer.begin_block("static const value_string channel_types_vs[] =")
    else:
        writer.begin_block("enum")
    for c in proto.channels:
        enum = codegen.prefix_underscore_upper("CHANNEL", c.name.upper())
        if describe:
            writer.writeln("{ %s, \"%s\" }," % (enum, c.name.upper()))
        else:
            if c.value == i:
                writer.writeln("%s," % enum)
                i = i + 1
            else:
                writer.writeln("%s = %s," % (enum, c.value))
                i = c.value + 1
    writer.newline()
    if describe:
        writer.writeln("{ 0, NULL }")
    else:
        writer.writeln("SPICE_END_CHANNEL")
    writer.end_block(semicolon=True)
    writer.newline()


def write_enums(writer, describe=False):
    writer.writeln("#ifndef _H_SPICE_ENUMS")
    writer.writeln("#define _H_SPICE_ENUMS")
    writer.newline()

    # Define enums
    for t in ptypes.get_named_types():
        if isinstance(t, ptypes.EnumBaseType):
            t.c_define(writer)
            if describe:
                t.c_describe(writer)

    write_channel_type_enum(writer)
    if (describe):
        write_channel_type_enum(writer, True)

    for c in ptypes.get_named_types():
        if not isinstance(c, ptypes.ChannelType):
            continue
        write_channel_enums(writer, c, False, False)
        if describe:
            write_channel_enums(writer, c, False, describe)
        write_channel_enums(writer, c, True, False)
        if describe:
            write_channel_enums(writer, c, True, describe)

    writer.writeln("#endif /* _H_SPICE_ENUMS */")

parser = OptionParser(usage="usage: %prog [options] <protocol_file> <destination file>")
parser.add_option("-e", "--generate-enums",
                  action="store_true", dest="generate_enums", default=False,
                  help="Generate enums")
parser.add_option("-w", "--generate-wireshark-dissector",
                  action="store_true", dest="generate_dissector", default=False,
                  help="Generate Wireshark dissector definitions")
parser.add_option("-d", "--generate-demarshallers",
                  action="store_true", dest="generate_demarshallers", default=False,
                  help="Generate demarshallers")
parser.add_option("-m", "--generate-marshallers",
                  action="store_true", dest="generate_marshallers", default=False,
                  help="Generate message marshallers")
parser.add_option("-P", "--private-marshallers",
                  action="store_true", dest="private_marshallers", default=False,
                  help="Generate private message marshallers")
parser.add_option("-M", "--generate-struct-marshaller",
                  action="append", dest="struct_marshallers",
                  help="Generate struct marshallers")
parser.add_option("-a", "--assert-on-error",
                  action="store_true", dest="assert_on_error", default=False,
                  help="Assert on error")
parser.add_option("-H", "--header",
                  action="store_true", dest="header", default=False,
                  help="Generate header")
parser.add_option("-p", "--print-error",
                  action="store_true", dest="print_error", default=False,
                  help="Print errors")
parser.add_option("-s", "--server",
                  action="store_true", dest="server", default=False,
                  help="Print errors")
parser.add_option("-c", "--client",
                  action="store_true", dest="client", default=False,
                  help="Print errors")
parser.add_option("-k", "--keep-identical-file",
                  action="store_true", dest="keep_identical_file", default=False,
                  help="Print errors")
parser.add_option("-i", "--include",
                  action="append", dest="includes", metavar="FILE",
                  help="Include FILE in generated code")
parser.add_option("--prefix", dest="prefix",
                  help="set public symbol prefix", default="")
parser.add_option("--ptrsize", dest="ptrsize",
                  help="set default pointer size", default="4")

(options, args) = parser.parse_args()

if len(args) == 0:
    parser.error("No protocol file specified")

if len(args) == 1:
    parser.error("No destination file specified")

ptypes.default_pointer_size = int(options.ptrsize)

proto_file = args[0]
dest_file = args[1]
proto = spice_parser.parse(proto_file)

if proto == None:
    exit(1)

codegen.set_prefix(proto.name)
writer = codegen.CodeWriter()
writer.header = codegen.CodeWriter()
writer.set_option("source", os.path.basename(proto_file))

license = """/*
  Copyright (C) 2013 Red Hat, Inc.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

"""

writer.public_prefix = options.prefix

writer.writeln("/* this is a file autogenerated by spice_codegen.py */")
writer.write(license)
writer.header.writeln("/* this is a file autogenerated by spice_codegen.py */")
writer.header.write(license)
if not options.header and not options.generate_enums:
    writer.writeln("#ifdef HAVE_CONFIG_H")
    writer.writeln("#include <config.h>")
    writer.writeln("#endif")

if options.assert_on_error:
    writer.set_option("assert_on_error")

if options.print_error:
    writer.set_option("print_error")

if options.includes:
    for i in options.includes:
        writer.header.writeln('#include <%s>' % i)
        writer.writeln('#include <%s>' % i)

if options.generate_enums or options.generate_dissector:
    write_enums(writer, options.generate_dissector)

if options.generate_demarshallers:
    if not options.server and not options.client:
        print >> sys.stderr, "Must specify client and/or server"
        sys.exit(1)
    demarshal.write_includes(writer)

    if options.server:
        demarshal.write_protocol_parser(writer, proto, False)
    if options.client:
        demarshal.write_protocol_parser(writer, proto, True)

if options.generate_marshallers or (options.struct_marshallers and len(options.struct_marshallers) > 0):
    marshal.write_includes(writer)

if options.generate_marshallers:
    if not options.server and not options.client:
        print >> sys.stderr, "Must specify client and/or server"
        sys.exit(1)
    if options.server:
        marshal.write_protocol_marshaller(writer, proto, False, options.private_marshallers)
    if options.client:
        marshal.write_protocol_marshaller(writer, proto, True, options.private_marshallers)

if options.struct_marshallers:
    for structname in options.struct_marshallers:
        t = ptypes.lookup_type(structname)
        marshal.write_marshal_ptr_function(writer, t, False)

if options.generate_marshallers or (options.struct_marshallers and len(options.struct_marshallers) > 0):
    marshal.write_trailer(writer)

if options.header:
    content = writer.header.getvalue()
else:
    content = writer.getvalue()
if options.keep_identical_file:
    try:
        f = open(dest_file, 'rb')
        old_content = f.read()
        f.close()

        if content == old_content:
            print "No changes to %s" % dest_file
            sys.exit(0)

    except IOError:
        pass

f = open(dest_file, 'wb')
f.write(content)
f.close()

print "Wrote %s" % dest_file
sys.exit(0)
