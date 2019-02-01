--[[
netlower.lua - trace network I/O slower than a given threshold.

USAGE: sysdig -c netlower min_ms
   eg,

   sysdig -c netlower 1000				  # show network I/O slower than 1000 ms.
   sysdig -c netlower "1 disable_colors"	# show network I/O slower than 1 ms. w/ no colors
   sysdig -c netlower 1000				  # show network I/O slower than 1000 ms and container output

Copyright (C) 2013-2014 Draios inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
--]]

-- Chisel description
description = "Trace network I/O slower than a threshold, or all network I/O. This chisel is compatible with containers using the sysdig -pc or -pcontainer argument, otherwise no container information will be shown. (Blue represents a process running within a container, and Green represents a host process)"
short_description = "Trace slow network I/0"
category = "Performance"

-- Chisel argument list
args =
{
	{
		name = "min_ms",
		description = "Minimum millisecond threshold for showing network I/O",
		argtype = "int",
		optional = false
	},
	{
		name = "disable_color",
		description = "Set to 'disable_colors' if you want to disable color output",
		argtype = "string",
		optional = true
	},
}

require "common"
terminal = require "ansiterminal"
terminal.enable_color(true)

-- Argument notification callback
function on_set_arg(name, val)

	if name == "disable_color" and val == "disable_color" then
	   terminal.enable_color(false)
	elseif name == "min_ms" then
	   min_ms = parse_numeric_input(val, name)
	end

	return true
end

-- Initialization callback
function on_init()
	-- set the following fields on_event()
	etype = chisel.request_field("evt.type")
	dir = chisel.request_field("evt.dir")
	datetime = chisel.request_field("evt.datetime")
	fname = chisel.request_field("fd.name")
	pname = chisel.request_field("proc.name")
	latency = chisel.request_field("evt.latency")
	fcontainername = chisel.request_field("container.name")
	fcontainerid = chisel.request_field("container.id")

	-- The -pc or -pcontainer options was supplied on the cmd line
	print_container = sysdig.is_print_container_data()

	-- filter for network I/O
	chisel.set_filter("evt.is_io=true and (fd.type=ipv4 or fd.type=ipv6)")

	-- The -pc or -pcontainer options was supplied on the cmd line
	if print_container then
		print(string.format("%-23.23s %-20.20s %-20.20s %-12.12s %-8s %-12s %s",
							"evt.datetime",
							"container.id",
							"container.name",
							"proc.name",
							"evt.type",
							"LATENCY(ms)",
							"fd.name"))
		print(string.format("%-23.23s %-20.20s %-20.20s %-12.12s %-8s %-12s %s",
							"-----------------------",
							"------------------------------",
							"------------------------------",
							"------------",
							"--------",
							"------------",
							"-----------------------------------------"))
	else
		print(string.format("%-23.23s %-12.12s %-8s %-12s %s",
							"evt.datetime",
							"proc.name",
							"evt.type",
							"LATENCY(ms)",
							"fd.name"))
		print(string.format("%-23.23s %-12.12s %-8s %-12s %s",
							"-----------------------",
							"------------",
							"--------",
							"------------",
							"-----------------------------------------"))
	end

	return true
end

-- Event callback
function on_event()

	local color = terminal.green

	lat = evt.field(latency) / 1000000
	fn = evt.field(fname)

	if evt.field(dir) == "<" and lat > min_ms then

		-- If this is a container modify the output color
		if evt.field(fcontainername) ~= "host" then
			color = terminal.blue
		end

		 -- The -pc or -pcontainer options was supplied on the cmd line
			 if print_container then
				 print(color .. string.format("%-23.23s %-20.20s %-20.20s %-12.12s %-8s %12d %s",
											  evt.field(datetime),
											  evt.field(fcontainerid),
											  evt.field(fcontainername),
											  evt.field(pname),
											  evt.field(etype),
											  lat,
											  fn ))
			 else
				 print(color .. string.format("%-23.23s %-12.12s %-8s %12d %s",
											  evt.field(datetime),
											  evt.field(pname),
											  evt.field(etype),
											  lat,
											  fn ))
			 end
	end

	return true
end
