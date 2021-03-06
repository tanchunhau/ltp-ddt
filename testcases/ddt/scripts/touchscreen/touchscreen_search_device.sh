#! /bin/sh 
#
# Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation version 2.
#
# This program is distributed "as is" WITHOUT ANY WARRANTY of any
# kind, whether express or implied; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

# search for Touchscreen device

source "common.sh"
#
# Search for a Touchscreen device
#

device=`ls /dev/input/touchscreen0`
if [ -n "$device" ];
then 
	echo ""
else
	test_print_trc " ::"
	test_print_trc " :: No Touchscreen device found. Exiting Touchscreen tests..."
	exit 2	
fi
