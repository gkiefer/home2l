#!/usr/bin/python3

# This file is part of the Home2L project.
#
# (C) 2015-2021 Gundolf Kiefer
#
# Home2L is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Home2L is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Home2L. If not, see <https://www.gnu.org/licenses/>.


import sys

verMajor = 0
verMinor = 0
revision = 0

try:
  ver = sys.argv[1].replace('-', '.').strip ('v').split ('.')
  verMajor = int (ver[0])
  verMinor = int (ver[1])
  revision = 2 * int (ver[2])
  if len(ver) >= 4:
    if ver[3][-1] == '*': revision += 1
except:
  print ("  warning: Incomplete version string, setting feature record entry to v{}.{}.{}.".format (verMajor, verMinor, revision), file=sys.stderr)

print (("#ifndef _VERSION_\n" +
        "#define _VERSION_\n" +
        "\n" +
        "#define VERSION_MAJOR {}\n" +
        "#define VERSION_MINOR {}\n" +
        "#define VERSION_REVISION {}\n" +
        "\n" +
        "#endif").format (verMajor, verMinor, revision))
