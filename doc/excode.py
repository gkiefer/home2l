#!/usr/bin/python3

# This file is part of the Home2L project.
#
# (C) 2015-2024 Gundolf Kiefer
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


# Usage: excode.py <e|r> <prefix list> <file1> <file2> ...

import sys


# Read arguments...
if sys.argv[1] == 'e': doEnv = True
else: doEnv = False
if sys.argv[1] == 'r': doRc = True
else: doRc = False

prefixList = sys.argv[2].split (':')
while "" in prefixList: prefixList.remove ("")

fileList = sys.argv[3:]

if not (doEnv or doRc) or not fileList:
  raise ValueError ("ERROR: Invalid arguments!")


# Translate text to tex...
def toTex (s):
  if not s: return None
  s = s.replace ('\\', '\\\\')
  for c in ('_', '#', '$', '%'):
    s = s.replace (c, '\\' + c)
    #~ print ("### s = '" + c + "'; s -> '" + s + "'")
  return s.replace ('"', "''")


# Read all files ...
itemList = []
for fileName in fileList:
  with open (fileName, 'r') as fh:
    lineList = fh.readlines ()
    lineNo = 0
    try:
      while lineNo < len(lineList):
        line = lineList[lineNo].strip ()
        line = line.lstrip ("# ")     # Strip away Python/shell comments to allow C-like declartions in scripts (e.g. drv-weather)
        srcOut = fileName.replace ("../", "") + ":" + str(lineNo+1)

        # Handle environment parameter ...
        if doEnv:
          if line.startswith ("ENV_PARA_"):
            # Split macro declaration ...
            macroType, dummy, argsRaw = line[9:].partition ('(')
            macroType = macroType.strip ()
            args = argsRaw.partition (");") [0].split (", ")
            if not ");" in line: raise ValueError ("Missing ';' at end of line")

            #~ print ("INFO: line = '" + str (line) + "'", file=sys.stderr, flush=True);
            #~ print ("INFO: args = " + str (args), file=sys.stderr, flush=True);
            # ... determine (key, type, default) ...
            keyOut = args[0].strip ('"')
            if macroType == "VAR" or macroType == "NOVAR":
              typeRaw, defaultOut = args[1], args[3]
            elif macroType == "SPECIAL":
              typeRaw, defaultOut = args[1], args[2]
            else:
              typeRaw, defaultOut = macroType, args[2]
            # ... sanitize (key, type, default) ...
            typeOut = typeRaw.rpartition (" ")[2].lower ()
            if typeOut == "*": typeOut = "string"
            if defaultOut:
              if defaultOut == "NULL" or "NODEFAULT" in line: defaultOut = None
            # Read brief description ...
            lineNo += 1
            briefOut = lineList[lineNo].partition ("/*")[2].strip () + "."
            if not briefOut: raise ValueError ("Brief description not detected")
            # Read full description ...
            descOut = ""
            lineNo += 1
            try:
              while not lineList[lineNo].strip ().endswith ("*/"):
                descOut += lineList[lineNo].partition ("*")[2].strip () + "\n"
                lineNo += 1
              descOut = descOut.strip ()
            except IndexError as e:
              raise ValueError ("Unexpected end of file")
            # Store item ...
            item = { "key": toTex (keyOut), "type": typeOut, "default": toTex (defaultOut), "brief": briefOut, "desc": descOut, "src": toTex (srcOut) }
            itemList.append (item)

        # Handle resource definition ...
        if doRc:

          # Check for a resource registering C command...
          haveRegister, skipArgs = False, 0
          if "Register" in line:    # quick pre-check
            if "CResource::Register" in line or "RcRegisterResource" in line: haveRegister, skipArgs = True, 1
            elif "->RegisterResource" in line: haveRegister, skipArgs = True, 0
            if "[RC:-]" in line: haveRegister = False
          if haveRegister:

            # Get the 'Register' command arguments ...
            args = line.partition ("(") [2].rpartition (");") [0].strip ().split (", ") [skipArgs:]
            # ~ print ("INFO: line = '" + str (line) + "'", file=sys.stderr, flush=True);
            # ~ print ("INFO: args = " + str (args), file=sys.stderr, flush=True);
            keyOut = args[0].strip ('"')    # LID
            # ~ print ("INFO: keyOut = " + str (keyOut), file=sys.stderr, flush=True);
            if args[1].strip () [0] != '"':   # type/mode arguments?
              typeOut = args[1][3:].lower ()
              modeOut = { "true" : "wr", "false":"ro" } [args[2]]
            else:                             # assume 'rcDef' argument
              typeOut, modeOut = args[1].strip ('"').split ()

            # Check for a default ...
            line = lineList[lineNo + 1]
            if "SetDefault" in line:
              lineNo += 1
              defaultOut = line.partition("(")[2].partition(")")[0].strip ()
            else:
              defaultOut = None

            # Check for brief description line ...
            line = lineList[lineNo + 1]
            if not "/* [RC:" in line:
              raise ValueError ("Brief description not detected (undocumented resource?)")
            lineNo += 1
            args, briefOut = line.partition("[RC:")[2].split ("] ", 1)
            driver, keyDef, defaultDef = (args.split (':') + [ "", "" ]) [:3]
            briefOut = briefOut.strip () + "."
            if driver != "-":   # skip explicitly undocumented resource
              if keyDef: keyOut = keyDef
              if defaultDef: defaultOut = defaultDef
              keyOut = driver + "/" + keyOut

              # Read full description ...
              descOut = ""
              lineNo += 1
              try:
                while not lineList[lineNo].strip ().endswith ("*/"):
                  descOut += lineList[lineNo].partition ("*")[2].strip () + "\n"
                  lineNo += 1
                descOut = descOut.strip ()
              except IndexError as e:
                raise ValueError ("Unexpected end of file")

              # Store item ...
              item = { "key": toTex (keyOut), "type": typeOut, "mode": modeOut, "default": defaultOut, "brief": briefOut, "desc": descOut, "src": toTex (srcOut) }
              itemList.append (item)
        # End of file reading loop...
        lineNo += 1
    except ValueError as e:
      print (fileName + ":" + str(lineNo+1) + ": ERROR: " + str(e.args), file=sys.stderr, flush=True)
      raise


# Output ...
for prefix in prefixList + [ None ]:

  # Write out prefix heading ...
  if doEnv and len (prefixList) > 0:
    if prefix:
      #~ print ("\n\n### Environment parameters of domain '" + prefix + "':\n")
      print ("\\subsection{Parameters of Domain \\texttt{" + prefix + "}}\n")
    elif len (itemList):
      #~ print ("\n\n### Environment parameters of other domains:\n")
      print ("\\subsection{Parameters of Other Domains}\n")

  if doRc and len (prefixList) > 0:
    if prefix:
      #~ print ("\n\n### Resources of driver '" + prefix + "':\n")
      print ("\\subsection{Resources of Driver \\texttt{" + prefix + "}}")
    elif len (itemList):
      #~ print ("\n\n### Resources of other drivers:\n")
      print ("\\subsection{Resources of Other Drivers}")

  # Write out matching items ...
  for item in itemList:
    if not prefix or item["key"].startswith (prefix):

      if doEnv:
        if item["default"]: defaultStr = " [ = " + item["default"] + " ]"
        else: defaultStr = ""

        print ("\n\n\n\\subsubsection*{\\texttt{" + item["key"] + "} (" + item["type"] + ") \\textnormal{" + defaultStr + " \\hfill {\\scriptsize \\texttt{" + item["src"] + "}}}}")
        print ("\\labelenv{" + item["key"] + "}")

        print ("\\begin{addmargin}[0.8cm]{0cm}")
        print (item["brief"])
        if item ["desc"]: print ("\\medbreak\n" + item ["desc"])
        print ("\\end{addmargin}")

      if doRc:
        if item["default"]: defaultStr = " [ = " + item["default"] + " ]"
        else: defaultStr = ""
        driver = item["key"].partition("/") [0]

        print ("\n\n\n\\subsubsection*{\\texttt{" + item["key"] + "} (" + item["type"] + "," + item["mode"] + ") \\textnormal{" + defaultStr + " \\hfill {\\scriptsize \\texttt{" + item["src"] + "}}}}")
        print ("\\labelrc{" + driver + "}{" + item["key"] + "}")

        print ("\\begin{addmargin}[0.8cm]{0cm}")
        print (item["brief"])
        if item ["desc"]: print ("\\medbreak\n" + item ["desc"])
        print ("\\end{addmargin}")

  # Write out prefix closing ...
  #~ if prefix or len (itemList):
    #~ print ("\end{description}\n\n\n")

  # Delete matching items from list ...
  if prefix:
    n = len (itemList) - 1
    while n >= 0:
      if itemList[n]["key"].startswith (prefix):
        itemList.pop (n)
      n -= 1
