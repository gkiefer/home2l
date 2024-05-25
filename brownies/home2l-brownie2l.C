/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2024 Gundolf Kiefer
 *
 *  Home2L is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Home2L is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Home2L. If not, see <https://www.gnu.org/licenses/>.
 *
 */


#include <env.H>

#include "brownies.H"

#include <fcntl.h>
#include <elf.h>
#include <fnmatch.h>
#include <errno.h>

#if WITH_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

extern "C" {
#include "avr/shades.h"     // for calculations related to shades configuration parameters
}





// *************************** Environment settings ****************************


ENV_PARA_STRING ("brownie2l.historyFile", envBrownie2lHistFile, ".brownie2l_history");
  /* Name of the history file for home2l-brownie2l, relative to the user's home directory
   */
ENV_PARA_INT ("brownie2l.historyLines", envBrownie2lHistLines, 64);
  /* Maximum number of lines to be stored in the history file
   *
   * If set to 0, no history file is written or read.
   */
ENV_PARA_STRING ("brownie2l.init.cmd", envBrownie2lInitCmd, "avrdude -c %2$s -p %1$s -U hfuse:w:%3$s.%1$s.elf -U efuse:w:%3$s.%1$s.elf -U eeprom:w:%3$s.%1$s.elf -U flash:w:%3$s.%1$s.elf");
  /* Shell command to initialize a new Brownie
   *
   * This shell is executed if the user issues an "init" command. In the string,
   * each occurence of '%1$s' is replaced by the given MCU type (e.g. 't85', 't84', 't861').
   * Each occurence of '%2$s' is replaced by the \refenv{brownie2l.init.programmer} setting.
   * Each occurence of '%3$s' is replaced by base path name of the ELF file
   * (e.g. 'opt/home2l/share/brownies/init').
   *
   * If you use avrdude(1) to program devices, it is usually not necessary to change this
   * setting.
   */
ENV_PARA_STRING ("brownie2l.init.programmer", envBrownie2lInitProgrammer, "avrisp2");
  /* Programmer device to be used for initializing a new Brownie
   */


#define BROWNIE_ELF_DIR "share/brownies"    // location of the .elf family relativ to HOME2L_ROOT





// ***************** Register names and help strings ***************************


static const struct {
  const char *name, *help;
} brRegDesc[0x40] = {
  /* BR_REG_CHANGED       0x00 */ { "changed",        "Change indicator register"
                                                      "  (Bit 0: Child; 1: GPIO; 2: Matrix; 3: UART; 4: Shades; 5: temp)" },
                                  { NULL, NULL },
  /* BR_REG_GPIO_0        0x02 */ { "gpio_0",         "GPIOs (0..7), one bit per GPIO" },
  /* BR_REG_GPIO_1        0x03 */ { "gpio_1",         "GPIOs (8..15, if present), one bit per GPIO" },
  /* BR_REG_TICKS_LO      0x04 */ { "ticks_lo",       "Ticks timer ..." },
  /* BR_REG_TICKS_HI      0x05 */ { "ticks_hi",       "  ... reading low latches high" },
  /* BR_REG_TEMP_LO       0x06 */ { "temp_lo",        "Temperature (Bits 12..1: raw temperature value, 0: valid bit) ..." },
  /* BR_REG_TEMP_HI       0x07 */ { "temp_hi",        "  ... reading low latches high" },
  /* BR_REG_ADC_0_LO      0x08 */ { "adc_0_lo",       "ADC #0 ..." },
  /* BR_REG_ADC_0_HI      0x09 */ { "adc_0_hi",       "  ... reading low latches high" },
  /* BR_REG_ADC_1_LO      0x0a */ { "adc_1_lo",       "ADC #1 ..." },
  /* BR_REG_ADC_1_HI      0x0b */ { "adc_1_hi",       "  ... reading low latches high" },
  /* BR_REG_UART_CTRL     0x0c */ { "uart_ctrl",      "UART control register"
                                                      "  (Bit 0: Reset RX buffer; 1: Reset TX buffer)" },
  /* BR_REG_UART_STATUS   0x0d */ { "uart_status",    "UART status register"
                                                      "  (Bit 7: Error, Bit 6: RX Overflow, Bits 5..3: TX buffer free; 2..0: RX buffer occupied)" },
  /* BR_REG_UART_RX       0x0e */ { "uart_rx",        "UART receive register" },
  /* BR_REG_UART_TX       0x0f */ { "uart_tx",        "UART transfer register" },

  /* BR_REG_MATRIX_0      0x10 */ { "matrix-0",       "Raw sensor matrix data ..." },
  /* BR_REG_MATRIX_1      0x11 */ { "matrix-1",       "  ... one byte per row, up to 8x8 = 64 bits ..." },
  /* BR_REG_MATRIX_2      0x12 */ { "matrix-2",       "" },
  /* BR_REG_MATRIX_3      0x13 */ { "matrix-3",       "" },
  /* BR_REG_MATRIX_4      0x14 */ { "matrix-4",       "" },
  /* BR_REG_MATRIX_5      0x15 */ { "matrix-5",       "" },
  /* BR_REG_MATRIX_6      0x16 */ { "matrix-6",       "" },
  /* BR_REG_MATRIX_7      0x17 */ { "matrix-7",       "" },
  /* BR_REG_MATRIX_EVENT  0x18 */ { "matrix-event",   "Next matrix event (bits 2:0 = col, 5:3 = row, 6 = value);"
                                                      " 0x80 = empty, 0x81 = overflow" },
  /* BR_REG_MATRIX_ECYCLE 0x19 */ { "matrix-ecycle",  "Cycle counter of last read matrix event" },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },

  /* BR_REG_SHADES_STATUS 0x20 */ { "shades-status",  "Shades status (bit 0:3 = actUp/actDn/btnUp/btnDn, 4:7 = same for shades #1)" },
                                  { NULL, NULL },
  /* BR_REG_SHADES_0_POS  0x22 */ { "shades-0-pos",   "Shades #0: Current position (0..100);  0xff = 'unknown'" },
  /* BR_REG_SHADES_0_RINT 0x23 */ { "shades-0-rint",  "Shades #0: Internal request (0..100 or 0xff = 'none')" },
  /* BR_REG_SHADES_0_REXT 0x24 */ { "shades-0-rext",  "Shades #0: External request (0..100 or 0xff = 'none')" },
  /* BR_REG_SHADES_1_POS  0x25 */ { "shades-1-pos",   "Shades #1: Current position (0..100);  0xff = 'unknown'" },
  /* BR_REG_SHADES_1_RINT 0x26 */ { "shades-1-rint",  "Shades #1: Internal request (0..100 or 0xff = 'none')" },
  /* BR_REG_SHADES_1_REXT 0x27 */ { "shades-1-rext",  "Shades #1: External request (0..100 or 0xff = 'none')" },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },

                       /* 0x30 */ { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
                                  { NULL, NULL },
  /* BR_REG_DEBUG_0       0x38 */ { "debug-0", "Debug register 0 (for debugging purposes only)" },
  /* BR_REG_DEBUG_1       0x39 */ { "debug-1", "Debug register 1 (for debugging purposes only)" },
  /* BR_REG_DEBUG_2       0x3a */ { "debug-2", "Debug register 2 (for debugging purposes only)" },
  /* BR_REG_DEBUG_3       0x3b */ { "debug-3", "Debug register 3 (for debugging purposes only)" },

                                  { NULL, NULL },
  /* BR_REG_FWBASE        0x3d */ { "fwbase", "Base address of the active firmware in units of BR_FLASH_PAGESIZE (0x40) bytes" },
  /* BR_REG_CTRL          0x3e */ { "ctrl",   "Control register"
                                              " (Bit 0 = unlock EEPROM, 1 = unlock flash, 2 = TWI hub to resurrection;"
                                              " 0xe0 = reboot, 0xa0 = reboot into new firmware)" },
  /* BR_REG_MAGIC         0x3f */ { "magic",  "Magic value, always returns BR_MAGIC (0xb1) after reset" }
};


static const char *BrRegName (int reg) {
  ///< Get a readable register name (or NULL, if the register does not exist).
  if (reg < 0 || reg >= 0x40) return NULL;
  return brRegDesc[reg].name;
}


static const char *BrRegHelp (int reg) {
  ///< Get a help string of a register (or NULL, if no help string exists).
  if (reg < 0 || reg >= 0x40) return NULL;
  return brRegDesc[reg].help;
}


static int BrRegFromStr (const char *str) {
  ///< Get register number for a string (or -1, if the register does not exist).
  const char *regStr;
  int n;

  for (n = 0; n < 0x40; n++)
    if ( (regStr = brRegDesc[n].name) ) if (strcmp (regStr, str) == 0) return n;
  return -1;
}





// ********************** Database, Link and Resources *************************


static CBrownieSet shellDatabase;
static CBrownieLink shellLink;
static int shellAdr = -1;





// ********************** Interpreter: Declarations ****************************


static bool doQuit = false;
static volatile bool interrupted = false;


// Forward declarations (implemented in section "Command interpreter") ...
static bool ExecuteCmd (int argc, const char **argv);
static bool ExecuteCmd (const char *cmd);

static bool CmdHelp (int argc, const char **argv);
static bool CmdQuit (int argc, const char **argv);





// ********************** Command Helpers **************************************


static bool ArgError (const char **argv, const char *msg = NULL) {
  const char *helpArgs[2] = { "help", argv ? argv[0] : NULL };

  printf (msg ? "Error: %s!\n" : "Syntax error!\n", msg);
  if (argv) CmdHelp (2, helpArgs);
  return false;
}


static bool AreYouSure () {
  char ans[4];
  printf (" (y/N) ");
  fflush (stdout);
  fgets (ans, sizeof (ans), stdin);
  return (ans[0] == 'y' || ans[0] == 'Y');
}


static bool CheckLegalTwiAdr () {
  if (shellLink.Status () == brNoBus) {
    printf ("No interface available.\n");
    return false;
  }
  if (shellAdr < 0 || shellAdr > 127) {
    printf ("No legal TWI address specified.\n");
    return false;
  }
  return true;
}


static inline const char *BrownieIdStr (CBrownie *brownie) {
  const char *id = brownie->Id ();
  return id[0] ? id : "(no ID)";
}


static bool PrintOnError (EBrStatus status) {
  if (status != brOk) {
    printf ("Error accessing device %03i: %s\n", shellAdr, BrStatusStr (status));
    return true;
  }
  return false;
}


static const char *McuFriendlyStr (int mcuType) {
  switch (mcuType) {
    case BR_MCU_ATTINY84:   return "ATtiny84 (t84)";
    case BR_MCU_ATTINY85:   return "ATtiny85 (t85)";
    case BR_MCU_ATTINY861:  return "ATtiny861 (t861)";
    case BR_MCU_NONE:       return "(none)";
    default:                return "(unknown)";
  }
}


static void PrintDeviceInfo (CBrownie *brownie, bool withFirmware = true) {
  TBrFeatureRecord *ver = brownie->FeatureRecord ();
  CString s;
  const char *key;
  uint16_t features, mask;
  int n;

  // Print VROM info ...
  if (brownie->HasDeviceFeatures ()) {

    // Print device and firmware version...
    if (withFirmware)
      printf ("        Device:   %s\n"
              "        Firmware: %s v%s\n",
              McuFriendlyStr (ver->mcuType),
              ver->fwName, BrVersionGetAsStr (&s, ver));
    else
      printf ("        Device:   %s\n",
              McuFriendlyStr (ver->mcuType));

    // Print features...
    printf ("        Features:");
    features = ver->features;
    for (mask = 1; mask; mask <<= 1) if (features & mask) {
      key = NULL;
      switch (mask) {
        case BR_FEATURE_MAINTENANCE:  key = "maintenance";  break;
        case BR_FEATURE_TIMER:        key = "timer";        break;
        case BR_FEATURE_NOTIFY:       key = "notify";       break;
        case BR_FEATURE_TWIHUB:       key = "twihub";       break;
        case BR_FEATURE_ADC_0:        key = "adc_0";        break;
        case BR_FEATURE_ADC_1:        key = "adc_1";        break;
        case BR_FEATURE_UART:         key = "uart";         break;
        case BR_FEATURE_TEMP:         key = "temperature";  break;
        case BR_FEATURE_SHADES_0:     key = "shades_0";     break;
        case BR_FEATURE_SHADES_1:     key = "shades_1";     break;
        default: key = "?";
      }
      printf (" %s", key);
    }

    // Print matrix configuration (if applicable) ...
    if (ver->matDim)
      printf (" matrix(%ix%i)", BR_MATDIM_ROWS (ver->matDim), BR_MATDIM_COLS (ver->matDim));
    putchar ('\n');

    // Print GPIO configuration (if applicable) ...
    if (ver->gpiPresence || ver->gpoPresence) {
      printf ("        GPIOs:    ");
      for (n = 0; n < 16 && ((ver->gpiPresence | ver->gpoPresence) >> n); n++) {
        if (ver->gpiPresence & (1 << n))
          putchar (ver->gpiPullup & (1 << n) ? 'p' : 'i');
        else if (ver->gpoPresence & (1 << n))
          putchar (ver->gpoPreset & (1 << n) ? '1' : '0');
        else putchar ('-');
      }
      putchar ('\n');
    }

    // Print EEPROM configuration (if available) ...
    if (brownie->HasDeviceConfig ())
      printf ("        Config:   %s\n", brownie->ToStr (&s, false, false));

  } // if (brownie->HasDeviceFeatures ())
}


static CBrownie *GetDbBrownie (const char *arg) {
  CBrownie *dbBrownie;
  int adr;
  char *endPtr;

  dbBrownie = NULL;
  if (arg[0] >= '0' && arg[0] <= '9') {
    adr = strtol (arg, &endPtr, 10);    // get by adr...
    if (*endPtr != '\0') adr = -1;
    dbBrownie = shellDatabase.Get (adr);
  }
  else
    dbBrownie = shellDatabase.Get (arg);     // get by ID

  if (!dbBrownie) ArgError (NULL, "No such brownie in database");
  return dbBrownie;
}





// *************************** ELF Reader **************************************


#define ELF_MAX_SEGMENTS 8


class CElfReader {
  public:
    CElfReader () { segments = 0; }
    ~CElfReader () { Clear (); }
    void Clear ();

    // Read file...
    const char *ReadFile (const char *fileName);

    // Retrieving data...
    int Segments () { return segments; }
    unsigned SegAdr (int seg) { return segAdr[seg]; }
    int SegSize (int seg) { return segSize[seg]; }
    uint8_t *SegData (int seg) { return segData[seg]; }

  protected:
    int segments;
    unsigned segAdr[ELF_MAX_SEGMENTS];
    int segSize[ELF_MAX_SEGMENTS];
    uint8_t *segData[ELF_MAX_SEGMENTS];
};


void CElfReader::Clear () {
  int n;

  for (n = 0; n < segments; n++) FREEP (segData[n]);
  segments = 0;
}


const char *CElfReader::ReadFile (const char *fileName) {
  const char *msg;
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdr;
  int fd, n;

  Clear ();

  // Open file ...
  fd = open (fileName, O_RDONLY);
  if (fd < 0) return "Failed to open file";

  // Read and validate ELF header...
  msg = NULL;
  if (read (fd, &ehdr, sizeof (ehdr)) != sizeof (ehdr))
    msg = "Failed to read file header";
  else if (ehdr.e_ident[EI_MAG0] != 0x7f || ehdr.e_ident[EI_MAG1] != 'E' ||
           ehdr.e_ident[EI_MAG2] != 'L'  || ehdr.e_ident[EI_MAG3] != 'F')
    msg = "No ELF file.";
  else if (ehdr.e_ident[EI_CLASS] != ELFCLASS32 || ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
           ehdr.e_type != ET_EXEC || ehdr.e_machine != EM_AVR)
    msg = "Wrong ELF format (no AVR executable, 32 bit little endian)";
  if (msg) {
    close (fd);
    return msg;
  }

  // Process all program headers...
  for (n = 0; n < ehdr.e_phnum; n++) {
    if (lseek (fd, ehdr.e_phoff + n * ehdr.e_phentsize, SEEK_SET) < 0)
      msg = "Seek error";
    else if (read (fd, &phdr, sizeof (phdr)) != sizeof (phdr))
      msg = "Read error";
    else {
      if (phdr.p_type == PT_LOAD && phdr.p_memsz > 0) {

        // Store the segment...
        if (phdr.p_filesz > phdr.p_memsz) phdr.p_filesz = phdr.p_memsz;
          // usually, this should never happen; clip anyway to avoid buffer overflow
        segAdr[segments] = phdr.p_vaddr;
        segSize[segments] = phdr.p_memsz;
        segData[segments] = MALLOC (uint8_t, phdr.p_memsz);
        if (lseek (fd, phdr.p_offset, SEEK_SET) < 0) msg = "Seek error";
        else if (read (fd, segData[segments], phdr.p_filesz) != (int) phdr.p_filesz) msg = "Read error";
        else {
          if (phdr.p_filesz < phdr.p_memsz) memset (segData[segments] + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
          segments++;
        }
        if (msg) FREEO (segData[segments]);
      }
    }

    // Error handling...
    if (msg) {
      close (fd);
      return msg;
    }
  }

  // Done...
  close (fd);
  return NULL;
}





// *************************** CmdOpen *****************************************


#define CMD_OPEN \
  { "o", CmdOpen, "[-s] [<adr>|<id>]", "Select a device for upcoming commands, contact it and print its info", CmdOpenExtraHelp }, \
  { "open", CmdOpen, NULL, NULL, NULL }


static const char *CmdOpenExtraHelp () {
  return  "Options:\n"
          "-s : Be silent (only set address, do not access the device)\n";
}


static bool CmdOpen (int argc, const char **argv) {
  CBrownie brownie, *dbBrownie;
  EBrStatus status;
  int n, adr;
  bool silent;

  // Parse options ...
  adr = shellAdr;
  silent = false;
  for (n = 1; n < argc; n++)
    if (argv[n][0] == '-')
      switch (argv[n][1]) {
        case 's':
          silent = true;
          break;
        default:
          return ArgError (argv);
      }
    else {
      if (!IntFromString (argv[n], &adr)) {
        dbBrownie = GetDbBrownie (argv[n]);
        if (!dbBrownie) return false;
        adr = dbBrownie->Adr ();
      }
    }

  // Set address ...
  if (adr != shellAdr) {
    if (adr >= 1 && adr <= 127) shellAdr = adr;
    else return ArgError (NULL, "Illegal address (must be 1..127)");
  }

  // Check node and print info ...
  if (!silent) {
    if (!CheckLegalTwiAdr ()) return false;
    printf ("%03i ", shellAdr);
    fflush (stdout);
    status = shellLink.CheckDevice (shellAdr, &brownie);
    if (status != brOk)
      printf ("? Error: %s\n", BrStatusStr (status));
    else {
      printf ("%s\n", BrownieIdStr (&brownie));
      PrintDeviceInfo (&brownie);
    }
  }

  // Success...
  return true;
}





// ************************* CmdFor ****************************************


#define CMD_FOR \
  { "for", CmdFor, "<selection> <cmd> <cmdArg1> ...", "Run another command for multiple Brownies", CmdForExtraHelp } \


static const char *CmdForExtraHelp () {
  return  "The selection <selection> is a comma-separated list of addresses or IDs.\n"
          "With addresses, ranges like '1-5' are allowed. With IDs, wildcards (*, ?)\n"
          "are allowed.\n"
          "\n"
          "Example: for 3-5,7,win-* boot -m\n"
          "  (boots multiple devices into maintenance system)\n";
}


static bool CmdFor (int argc, const char **argv) {
  bool selection[128];
  CSplitString sel;
  CBrownie *brownie;
  const char *item;
  int n, k, adr0, adr1, lastShellAdr;
  bool ok;

  // Sanity ...
  if (argc < 3) return ArgError (argv);

  // Determine selection ...
  for (n = 0; n < 128; n++) selection[n] = false;
  sel.Set (argv[1], INT_MAX, ",");
  for (n = 0; n < sel.Entries (); n++) {
    item = sel.Get (n);
    if (item[0] >= '0' && item[0] <= '9') {
      // Adress or address range ...
      k = sscanf (item, "%d-%d", &adr0, &adr1);
      ok = (k >= 1);
      if (adr0 < 1 || adr0 > 127) ok = false;
      if (k == 2 && (adr1 < 1 || adr1 > 127 || adr1 < adr0)) ok = false;
      if (!ok) {
        printf ("Error: Illegal address or range specification: '%s'\n", item);
        return false;
      }
      if (k == 1) selection[adr0] = true;
      else for (k = adr0; k <= adr1; k++) selection[k] = true;
    }
    else {
      // ID or ID pattern ...
      ok = false;
      for (k = 1; k < 128; k++) {
        brownie = shellDatabase.Get (k);
        if (brownie) if (brownie->IsValid ())
          if (fnmatch (item, brownie->Id (), 0) == 0) selection[k] = ok = true;
      }
      if (!ok) printf ("Warning: No known Brownie matches '%s'.\n", item);
    }
  }

  // Run sub-commands ...
  lastShellAdr = shellAdr;
  ok = true;
  for (n = 1; n < 128 /* && ok */; n++) if (selection[n]) {
    printf ("%03i: ", n);
    fflush (stdout);
    shellAdr = n;
    ok = ExecuteCmd (argc - 2, argv + 2);
  }
  shellAdr = lastShellAdr;

  // Success ...
  return ok;
}





// *************************** CmdScan *****************************************


#define CMD_SCAN \
  { "s", CmdScan, "[<options>]", "Scan all possible addresses (1..127) and list all brownie nodes found", CmdScanExtraHelp }, \
  { "scan", CmdScan, NULL, NULL, NULL }


static const char *CmdScanExtraHelp () {
  return  "Options:\n"
          "-v          : Print detailed node info\n"
          "-c          : Check consistency with database\n"
          "-d [<file>] : Write a database template file [Default: <stdout>]";
}


static bool CmdScan (int argc, const char **argv) {
  CBrownie brownie, *dbBrownie;
  CString s;
  int adr;
  EBrStatus status;
  FILE *outFile = NULL;
  const char *outFileName, *stateStr;
  int n;
  bool verbose, withCheck;

  // Parse options ...
  verbose = withCheck = false;
  outFile = NULL;
  outFileName = NULL;
  for (n = 1; n < argc; n++) if (argv[n][0] == '-') {
    switch (argv[n][1]) {
      case 'v':
        verbose = true;
        break;
      case 'c':
        withCheck = true;
        break;
      case 'd':
        outFile = stdout;   // default
        if (n < argc-1) if (argv[n+1][0] != '-') {
          n++;
          outFileName = argv[n];
          outFile = fopen (outFileName, "wt");
          if (!outFile) {
            printf ("Error: Failed to open '%s' for writing!\n", outFileName);
            return false;
          }
        }
        break;
      default:
        return ArgError (argv);
    }
  }

  // Main scan loop ...
  if (outFile) {
    if (outFileName)
      printf ("Writing scan results as a database template to '%s'...\n", outFileName);
    else if (verbose || withCheck) {
      printf ("Scan results are in database syntax, options '-c' and '-v' are ignored.\n\n");
      verbose = withCheck = false;
    }
  }
  for (adr = 1; adr < 128 && !interrupted; adr++) {

    // Test connect ...
    printf ("\r%03i ", (int) adr);
    fflush (stdout);
    //~ usleep (100000);
    status = shellLink.CheckDevice (adr, &brownie);

    // Print database status ...
    if (withCheck) {
      stateStr = NULL;
      dbBrownie = shellDatabase.Get (adr);
      if (!dbBrownie) {
        if (status == brOk) stateStr = "NEW";
      }
      else {
        if (status != brOk) stateStr = "UNREACHABLE";
        else {
          if (brownie.IsCompatible (dbBrownie->DatabaseString ())) stateStr = "OK";
          else stateStr = "INCONSISTENT";
        }
      }
      if (stateStr) printf (status == brOk ? "[%s] " : "[%s]\n", stateStr);
        // If the status is not 'brOk', we generate a linefeed sine otherwise not output is generated.
    }

    // Print ID and info ...
    if (status == brOk) {
      if (!outFile || outFileName) {      // conventional output only if not dumping for database to <stdout>
        if (brownie.HasDeviceFeatures ()) {
          printf ("%-16s %12s v%-12s (%s)\n",
                  BrownieIdStr (&brownie),
                  brownie.FeatureRecord ()->fwName,
                  BrVersionGetAsStr (&s, brownie.FeatureRecord ()),
                  BrMcuStr (brownie.FeatureRecord ()->mcuType, "?"));
        }
        else
          printf ("%s\n", BrownieIdStr (&brownie));
        if (verbose) PrintDeviceInfo (&brownie, false);
      }
      if (outFile) fprintf (outFile, "id=%-12s %s\n", brownie.Id (), brownie.ToStr (&s, false));
    }
    else if (status != brNoDevice) {
      printf ("? %s\n", BrStatusStr (status));
    }
  }

  // Done ...
  putchar ('\r');
  fflush (stdout);
  if (outFileName) fclose (outFile);
  interrupted = false;
  return true;
}





// *************************** CmdRegRead/CmdRegWrite **************************


#define CMD_REG_READ \
  { "r", CmdRegRead, "<first> [<last>]", "Read register(s)", CmdRegExtraHelp }, \
  { "read", CmdRegRead, NULL, NULL, NULL }


static const char *CmdRegExtraHelp () {
  static CString s;
  const char *name, *help;
  int n;

  if (s.IsEmpty ()) {
    s.SetC ("Registers:\n");
    for (n = 0; n < 0x40; n++) {
      if ( (n & 0xf) == 0 ) s.Append ('\n');
      name = BrRegName (n);
      help = BrRegHelp (n);
      if (name) s.AppendF ("  0x%02x: %-15s %s\n", n, name, help ? help : CString::emptyStr);
    }
  }
  return s.Get ();
}


static bool CmdRegRead (int argc, const char **argv) {
  EBrStatus status;
  int reg0 = 0, reg1, val;
  char *endPtr;
  int n;
  bool ok;

  // Arguments...
  ok = (argc >= 2);
  if (ok) {
    reg0 = (int) strtol (argv[1], &endPtr, 0);
    if (*endPtr != '\0') reg0 = BrRegFromStr (argv[1]);
  }
  if (argc >= 3) {
    reg1 = (int) strtol (argv[2], &endPtr, 0);
    if (*endPtr != '\0')  reg1 = BrRegFromStr (argv[2]);
  }
  else reg1 = reg0;
  if (reg0 < 0 || reg1 >= BR_REGISTERS || reg1 < reg0) ok = false;
  if (!ok) return ArgError (argv);

  // Sanity...
  if (!CheckLegalTwiAdr ()) return false;

  // Read register(s)...
  for (n = reg0; n <= reg1; n++) {
    status = brOk;
    val = shellLink.RegReadNext (&status, shellAdr, (uint8_t) n);
    if (status == brOk) printf ("reg(0x%02x) = 0x%02x\n", n, val);
    else printf ("Failed to read reg(0x%02x): %s\n", n, BrStatusStr (status));
  }

  // Success...
  return true;
}


#define CMD_REG_WRITE \
  { "w", CmdRegWrite, "<reg> <value>", "Write register", CmdRegExtraHelp }, \
  { "write", CmdRegWrite, NULL, NULL, NULL }


static bool CmdRegWrite (int argc, const char **argv) {
  EBrStatus status;
  int reg = 0, val;
  char *endPtr;
  bool ok;

  // Arguments...
  ok = (argc >= 3);
  if (ok) {
    reg = (int) strtol (argv[1], &endPtr, 0);
    if (*endPtr != '\0') reg = BrRegFromStr (argv[1]);
    val = (int) strtol (argv[2], &endPtr, 0);
    if (*endPtr != '\0') ok = false;
  }
  if (reg < 0 || reg >= BR_REGISTERS || val < 0x00 || val > 0xff) ok = false;
  if (!ok) return ArgError (argv);

  // Sanity...
  if (!CheckLegalTwiAdr ()) return false;

  // Write register...
  status = shellLink.RegWrite (shellAdr, (uint8_t) reg, (uint8_t) val);
  if (status == brOk) printf ("reg(0x%02x) <- 0x%02x\n", reg, val);
  else printf ("Failed to write reg(0x%02x): %s\n", reg, BrStatusStr (status));

  // Success...
  return true;
}





// *************************** CmdMemRead **************************************


#define CMD_MEM_READ \
  { "m", CmdMemRead, "<first adr> [<last adr>]", "Read memory", CmdMemReadExtraHelp }, \
  { "memory", CmdMemRead, NULL, NULL, NULL }


static const char *CmdMemReadExtraHelp () {
  return  "Memory adress areas:\n"
          "  0x0000 - 0x0fff: SRAM\n"
          "  0x1000 - 0x1fff: EEPROM\n"
          "  0x2000 - 0x2fff: Version ROM (VROM)\n"
          "  0x8000 - 0xffff: FLASH\n";
}

static bool CmdMemRead (int argc, const char **argv) {
  EBrStatus status;
  uint8_t data[BR_MEM_BLOCKSIZE];
  int adr0 = 0, adr1;
  char *endPtr;
  int n;
  bool ok;

  // Arguments...
  ok = (argc >= 2);
  if (ok) {
    adr0 = (int) strtol (argv[1], &endPtr, 0);
    if (*endPtr != '\0') ok = false;
  }
  if (argc >= 3) {
    adr1 = (int) strtol (argv[2], &endPtr, 0);
    if (*endPtr != '\0') ok = false;
  }
  else adr1 = adr0;
  if (adr0 < 0 || adr1 >= 0xffff || adr1 < adr0) ok = false;
  if (!ok) return ArgError (argv);

  // Sanity...
  if (!CheckLegalTwiAdr ()) return false;

  // Align addresses to blocks...
  adr0 &= ~(BR_MEM_BLOCKSIZE - 1);
  adr1 = (adr1 | (BR_MEM_BLOCKSIZE - 1)) + 1;    // adr1 now becomes the first address not to display

  // Dump memory...
  while (adr0 < adr1 && !interrupted) {
    printf ("0x%04x:", adr0);
    status = shellLink.MemRead (shellAdr, adr0, BR_MEM_BLOCKSIZE, data);
    if (status == brOk) {
      for (n = 0; n < BR_MEM_BLOCKSIZE; n++) printf (" %02x", (int) data[n]);
      putchar ('\n');
    }
    else printf (" Error: %s\n", BrStatusStr (status));
    adr0 += BR_MEM_BLOCKSIZE;
  }

  // Success ...
  return true;
}





// *************************** CmdConfig ***************************************


#define CMD_CONFIG \
  { "c", CmdConfig, "[<options>] [ <name>[=<val>] ] ...", "Configure the device / query configuration", CmdConfigExtraHelp }, \
  { "config", CmdConfig, NULL, NULL, NULL }


static const char *CmdConfigExtraHelp () {
  static CString s;
  int n;

  s.SetC ("Options:\n"
          "-y              : Do not ask for confirmation\n"
          "-d [<adr>|<id>] : Configure device according to a database entry,\n"
          "                  optionally identified by an address <adr>\n"
          "                  or a brownie ID <id>\n"
          "\n"
          "Possible configuration variables are:\n\n");
  for (n = 0; n < brCfgDescs; n++)
    s.AppendF ("  %-14s: %s\n", brCfgDescList[n].key, brCfgDescList[n].help);
  s.AppendF ("\nPossible values for the shades timing parameters are:\n"
             "  delay: %.2f ... %.2f (seconds)\n"
             "  speed: %.1f ... %.1f (seconds)\n",
             ShadesDelayFromByte (0x00), ShadesDelayFromByte (0xff),
             ShadesSpeedFromByte (0xff), ShadesSpeedFromByte (0x01));
  return s.Get ();
}


static bool CmdConfig (int argc, const char **argv) {
  CBrownie brownie, *dbBrownie;
  TBrIdRecord savedId;
  TBrConfigRecord savedConfig;
  CString optStr, reportStr;
  int n;
  bool yesSure, changedId, changedCfg;

  // Sanity ...
  if (!CheckLegalTwiAdr ()) return false;

  // Parse options ...
  yesSure = false;
  dbBrownie = NULL;
  optStr.Clear ();
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') {
      switch (argv[n][1]) {
        case 'y':
          yesSure = true;
          break;
        case 'd':
          dbBrownie = shellDatabase.Get (shellAdr);
          if (n < argc-1) if (argv[n+1][0] != '-') {
            n++;
            dbBrownie = GetDbBrownie (argv[n]);
            if (!dbBrownie) return false;
          }
          break;
        default:
          return ArgError (argv);
      }
    }
    else {  // if (argv[n][0] == '-')
      optStr.Append (argv[n]);
      optStr.Append (" ");
    }
  }
  optStr.Strip ();

  // Read out brownie ...
  if (PrintOnError (
    shellLink.CheckDevice (shellAdr, &brownie))
  ) return false;

  // Apply changes and prepare report string ...
  memcpy (&savedId, brownie.Id (), BR_EEPROM_ID_SIZE);
  memcpy (&savedConfig, brownie.ConfigRecord (), BR_EEPROM_CFG_SIZE);
  if (dbBrownie) {
    if (!brownie.SetFromStr (dbBrownie->DatabaseString (), &reportStr))
      return ArgError (argv, "Illegal assignment(s) in database entry");
  }
  if (!optStr.IsEmpty ()) {
    if (!brownie.SetFromStr (optStr.Get (), &reportStr))
      return ArgError (argv, "Illegal option assignment(s)");
  }
  if ( (!dbBrownie && optStr.IsEmpty ()) || (dbBrownie && !optStr.IsEmpty ()) )
    // If nothing is assigned or if both the database and manual assignments are
    // given, we read the complete configuration (not just the selected keys).
    brownie.ToStr (&reportStr, true, false);

  // Print report ...
  if (!yesSure) putchar ('\n');
  if (!yesSure || (optStr.IsEmpty () && !dbBrownie))
    printf ("%03i %s\n  %s\n", shellAdr, BrownieIdStr (&brownie), reportStr.Get ());

  // Check changes and ask for confirmation ...
  changedId = bcmp (&savedId, brownie.Id (), BR_EEPROM_ID_SIZE) != 0;
  changedCfg = bcmp (&savedConfig, brownie.ConfigRecord (), BR_EEPROM_CFG_SIZE) != 0;
  if (!changedId && !changedCfg) {
    if (!yesSure) putchar ('\n');
    if (!optStr.IsEmpty () || dbBrownie) puts ("No need to write ID or config record.");
    return true;    // nothing requested => return silently
  }
  if (!yesSure) {
    printf ("\nWrite back this configuration and reboot node %03i?", shellAdr);
    if (!AreYouSure ()) return false;
  }

  // On address change: Make sure that the new address is unused ...
  if (brownie.ConfigRecord ()->adr != savedConfig.adr) {
    if (shellLink.CheckDevice (brownie.ConfigRecord ()->adr) != brNoDevice) {
      printf ("\nFatal: Appearently, the new address is already in use.\n"
              "       Writing this configuration would result in a bus conflict!\n");
      return false;
    }
  }

  // Write ...
  printf ("Writing %s ... ", changedId ? (changedCfg ? "ID and config" : "ID") : "config");
  fflush (stdout);
  if (PrintOnError (
    shellLink.RegWrite (shellAdr, BR_REG_CTRL, BR_CTRL_UNLOCK_EEPROM)   // unlock EEPROM
  )) return false;
  if (changedId) if (PrintOnError (
    shellLink.MemWrite (shellAdr, BR_MEM_ADR_EEPROM(BR_EEPROM_ID_BASE), BR_EEPROM_ID_SIZE, (uint8_t *) brownie.Id ())
  )) return false;
  if (changedCfg) if (PrintOnError (
    shellLink.MemWrite (shellAdr, BR_MEM_ADR_EEPROM(BR_EEPROM_CFG_BASE), BR_EEPROM_CFG_SIZE, (uint8_t *) brownie.ConfigRecord ())
  )) return false;
  if (PrintOnError (
    shellLink.RegWrite (shellAdr, BR_REG_CTRL, 0)   // lock EEPROM again
  )) return false;

  // Verify ...
  if (changedId) {
    printf ("verifying ID ... ");
    fflush (stdout);
    if (changedId) if (PrintOnError (
      shellLink.MemRead (shellAdr, BR_MEM_ADR_EEPROM(BR_EEPROM_ID_BASE), BR_EEPROM_ID_SIZE, (uint8_t *) &savedId)
    )) return false;
    if (bcmp (&savedId, brownie.Id (), BR_EEPROM_ID_SIZE) != 0) {
      puts ("ERROR - data may be corrupt!");
      return false;
    }
  }
  if (changedCfg) {
    printf ("verifying config ... ");
    fflush (stdout);
    if (changedCfg) if (PrintOnError (
      shellLink.MemRead (shellAdr, BR_MEM_ADR_EEPROM(BR_EEPROM_CFG_BASE), BR_EEPROM_CFG_SIZE, (uint8_t *) &savedConfig)
    )) return false;
    if (bcmp (&savedConfig, brownie.ConfigRecord (), BR_EEPROM_CFG_SIZE) != 0) {
      puts ("ERROR - data may be corrupt!");
      return false;
    }
  }
  puts ("OK");

  // Reboot ...
  printf ("Rebooting ... ");
  fflush (stdout);
  if (PrintOnError (
    shellLink.RegWrite (shellAdr, BR_REG_CTRL, BR_CTRL_REBOOT)   // reboot
  )) return false;
  puts ("OK");

  // Change shell address to follow the current brownie...
  shellAdr = brownie.Adr ();

  // Success ...
  return true;
}





// ************************* CmdBoot *******************************************


#define CMD_BOOT \
  { "boot", CmdBoot, "<options>", "Reboot brownie and/or switch firmware", CmdBootExtraHelp } \


static const char *CmdBootExtraHelp () {
  return  "Options:\n"
          "-i : Ask for confirmation when changing system\n"
          "-s : Only boot if the selected system is not already active\n"
          "-m : Activate and boot into maintenance system\n"
          "-o : Activate and boot into operational (main) system\n";
}


static bool CmdBoot (int argc, const char **argv) {
  CBrownie brownie;
  uint8_t val;
  int n, fwBaseBlock;
  bool yesSure, soft, intoMaintenance, intoOperational;

  // Sanity ...
  if (!CheckLegalTwiAdr ()) return false;

  // Parse options ...
  intoMaintenance = intoOperational = false;
  yesSure = true;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] != '-') return ArgError (argv);
    switch (argv[n][1]) {
      case 'i': yesSure = false; break;
      case 's': soft = true; break;
      case 'm': intoMaintenance = true; break;
      case 'o': intoOperational = true; break;
      default:
        return ArgError (argv);
    }
  }
  if (intoOperational && intoMaintenance) return ArgError (argv);

  // Check node...
  if (PrintOnError (
    shellLink.CheckDevice (shellAdr)
  )) return false;

  if (!intoMaintenance && !intoOperational) {
    if (soft) {
      puts ("Nothing to do!");
      return true;
    }
    else {

      // Normal reboot ...
      printf ("Rebooting device %03i ... ", shellAdr);
      fflush (stdout);
      if (PrintOnError (
        shellLink.RegWrite (shellAdr, BR_REG_CTRL, BR_CTRL_REBOOT)
      )) return false;
      puts ("OK");
    }
  }
  else {
    fwBaseBlock = intoOperational ? (BR_FLASH_BASE_OPERATIONAL / BR_FLASH_PAGESIZE) :
                             (BR_FLASH_BASE_MAINTENANCE / BR_FLASH_PAGESIZE);
    if (soft) {

      // Check present base block ...
      if (PrintOnError (
        shellLink.RegRead (shellAdr, BR_REG_FWBASE, &val)
      )) return false;
      if (fwBaseBlock == (int) val) {
        puts ("Selected firmware is already active - nothing to do.");
        return true;
      }
    }

    // Ask for confirmation ...
    if (!yesSure) {
      printf ("\nActivate %s firmware for device %03i?",
              intoMaintenance ? "MAINTENANCE" : "OPERATIONAL", shellAdr);
      if (!AreYouSure ()) return false;
      putchar ('\n');
    }

    // Activate new firmware and reboot into it ...
    printf ("Switching device %03i to %s firmware (block 0x%02x, adr=0x%04x) ... ",
            shellAdr, intoMaintenance ? "MAINTENANCE" : "OPERATIONAL",
            fwBaseBlock, fwBaseBlock * BR_FLASH_PAGESIZE);
    fflush (stdout);
    if (PrintOnError (
      shellLink.RegWrite (shellAdr, BR_REG_FWBASE, fwBaseBlock)
    )) return false;
    if (PrintOnError (
      shellLink.RegRead (shellAdr, BR_REG_FWBASE, &val)
    )) return false;
    if (fwBaseBlock != (int) val) {
      puts ("Verification failure! - Aborted.");
      return false;
    }
    fflush (stdout);
    //~ puts ("OK");

    printf ("Activating and rebooting ... ");
    fflush (stdout);
    if (PrintOnError (
      shellLink.RegWrite (shellAdr, BR_REG_CTRL, BR_CTRL_REBOOT_NEWFW)
    )) return false;

    puts ("OK");
  }

  // Give the new firmware time to come up (for scripted use or 'CmdUpgrade()') ...
  Sleep (100);

  // Success...
  return true;
}





// ************************* CmdInit *******************************************


#define CMD_INIT \
  { "init", CmdInit, "<mcu>", "Initialize a new Brownie and install the maintenance firmware", CmdInitExtraHelp } \


static const char *CmdInitExtraHelp () {
  return  "This runs avrdude(1) program the device. An i2c link is not required.\n"
          "<mcu> can be 't85', 't84' or 't861'.\n";
}


static bool CmdInit (int argc, const char **argv) {
  CString cmd, elfBaseName;
  int code;

  // Sanity ...
  if (argc < 2) return ArgError (argv);

  // Prepare command ...
  elfBaseName.SetF ("%s/" BROWNIE_ELF_DIR "/init", EnvHome2lRoot ());
  cmd.SetF (envBrownie2lInitCmd, argv[1], envBrownie2lInitProgrammer, elfBaseName.Get ());
  printf ("Initialize the brownie by running:\n\n$ %s\n\nContinue?", cmd.Get ());
  if (!AreYouSure ()) return false;

  // Run the command and finish ...
  code = system (cmd.Get ());
  if (code < 0) {
    printf ("Failed: %s\n", strerror (errno));
    return false;
  }
  if (code > 0) {
    printf ("Failed with exit code %i.\n", code);
    return false;
  }
  return true;
}





// ************************* CmdProgram ****************************************


#define CMD_PROGRAM \
  { "program", CmdProgram, "[ <options> ] [ <ELF file> ]", "Program the device", CmdProgramExtraHelp } \


static const char *CmdProgramExtraHelp () {
  return  "Options:\n"
          "-v              : (without -y) Show hex dump of loadable ELF file segments\n"
          "-y              : Do not ask for confirmation and do not show ELF file contents\n"
          "-d [<adr>|<id>] : Select the ELF file based on a database entry,\n"
          "                  optionally identified by an address <adr>\n"
          "                  or a brownie ID <id>\n"
          "\n"
          "If <ELF file> contains a '/' character, the file is searched in the working\n"
          "directory ($PWD) or global directory as specified. If it does not contain a '/',\n"
          "the file is searched inside the Home2L installation directory only (typically\n"
          "'$HOME2L_ROOT/share/brownies').\n"
          "\n"
          "ELF files derived by the '-d' option are first search in the current working\n"
          "directory, then in the Home2L installation directory.\n";
}


static bool CmdProgram (int argc, const char **argv) {
  CElfReader elfReader;
  CString s, elfFileName;
  CBrownie brownie, *dbBrownie;
  uint8_t *buf;
  const char *msg, *mcuModel;
  int adrHi, adrLo, size, n, k;
  bool verbose, yesSure;

  // Parse options ...
  if (argc < 2) return ArgError (argv);
  verbose = yesSure = false;
  dbBrownie = NULL;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') {
      switch (argv[n][1]) {

        case 'v':
          verbose = true;
          break;

        case 'y':
          yesSure = true;
          break;

        case 'd':
          dbBrownie = shellDatabase.Get (shellAdr);
          if (n < argc-1) if (argv[n+1][0] != '-') {
            // Address/ID is given on the command line ...
            n++;
            dbBrownie = GetDbBrownie (argv[n]);
            if (!dbBrownie) return false;   // argument given, but invalid
          }
          if (!dbBrownie)
            // Use current address ...
            dbBrownie = shellDatabase.Get (shellAdr);
          if (!dbBrownie) return ArgError (argv, "No Brownie defined");
          mcuModel = BrMcuStr (dbBrownie->FeatureRecord ()->mcuType);
          if (!mcuModel) return ArgError (argv, "No MCU model defined for Brownie");

          // Search for appropriate ELF file in local dir ("./"), then revert to standard search path ...
          elfFileName.SetF ("./%s.%s.elf", dbBrownie->FeatureRecord ()->fwName, mcuModel); // try local dir first
          if (access (elfFileName.Get (), R_OK) != 0)
            elfFileName.Del (0, 2);   // assume Home2L installation: Remove leading "./"
          break;

        default:
          return ArgError (argv);
      }
    }
    else {  // if (argv[n][0] == '-')
      elfFileName.SetC (argv[n]);
    }
  }

  // Complete 'elfFileName' ...
  if (!strchr (elfFileName.Get (), '/'))
    elfFileName.InsertF (0, "%s/" BROWNIE_ELF_DIR "/", EnvHome2lRoot ());

  // Read ELF file and print information...
  msg = elfReader.ReadFile (elfFileName.Get ());
  if (msg) {
    printf ("Error reading '%s': %s\n", elfFileName.Get (), msg);
    return false;
  }
  if (verbose || !yesSure) {
    printf ("\nSegments in '%s':\n", elfFileName.Get ());
    for (n = 0; n < elfReader.Segments (); n++) {
      adrLo = elfReader.SegAdr (n);
      adrHi = adrLo >> 16;
      if (1 || adrHi == 0x0000 || adrHi == 0x0081) {   // [[ TBD: only print FLASH and EEPROM segments ]]
        adrLo &= 0xffff;
        size = elfReader.SegSize (n);
        printf ("  %s: %04x - %04x (%i bytes)",
                adrHi == 0x0000 ? (adrLo >= BR_FLASH_BASE_MAINTENANCE ? " FLASH  " : "(FLASH) ")
                : adrHi == 0x0080 ? "(SRAM)  "
                : adrHi == 0x0081 ? "(EEPROM)"
                : adrHi == 0x0082 ? "(Fuses) "
                : "(?)     ",
                adrLo, adrLo + size, size);
        if (verbose) for (k = 0; k < size; k++) {
          if ((k & 0x0f) == 0) printf ("\n    %04x:", adrLo + k);
          printf (" %02x", elfReader.SegData (n) [k]);
        }
        putchar ('\n');
      }
    }
    putchar ('\n');
  }

  // Check device and MCU type...
  if (!CheckLegalTwiAdr ()) return false;
  if (PrintOnError (
    shellLink.CheckDevice (shellAdr, &brownie)
  )) return false;
  s.SetF (".%s.", mcuModel = BrMcuStr (brownie.FeatureRecord ()->mcuType));
  if (!mcuModel || strstr (elfFileName.Get (), s.Get ()) == NULL) {
    printf ("WARNING: According to its name, the ELF file '%s'\n"
            "         is not compatible with the current MCU type (%s).\n"
            "\n"
            "         Think twice before you proceed!\n\n",
            elfFileName.Get (), mcuModel);
    yesSure = false;
  }

  // Confirm...
  if (!yesSure) {
    printf ("(Re-)program FLASH of device %03i with this?", shellAdr);
    if (!AreYouSure ()) return false;
    putchar ('\n');
  }

  // Go ahead...
  printf ("Flashing device %03i with '%s' ... ", shellAdr, elfFileName.Get ());
  fflush (stdout);
  if (PrintOnError (
    shellLink.RegWrite (shellAdr, BR_REG_CTRL, BR_CTRL_UNLOCK_FLASH)   // unlock FLASH
  )) return false;

  for (n = 0; n < elfReader.Segments () && !interrupted; n++) {
    adrLo = elfReader.SegAdr (n);
    adrHi = adrLo >> 16;
    adrLo &= 0xffff;
    if (adrHi == 0x0000 && adrLo >= BR_FLASH_BASE_MAINTENANCE) {   // only handle FLASH segments; ignore .boot segments
      size = elfReader.SegSize (n);

      printf ("\n  %04x - %04x (%i bytes) ... ", adrLo, adrLo + size, size);
      fflush (stdout);
      if (PrintOnError (
        shellLink.MemWrite (shellAdr, BR_MEM_ADR_FLASH(adrLo), size, elfReader.SegData (n), true)
      )) return false;

      printf ("verifying ... ");
      fflush (stdout);
      buf = MALLOC(uint8_t, size);
      if (PrintOnError (
        shellLink.MemRead (shellAdr, BR_MEM_ADR_FLASH(adrLo), size, buf, true)
      )) return false;
      if (bcmp (buf, elfReader.SegData (n), size) != 0) {
        puts ("ERROR - area may be corrupt!");
        for (k = 0; k < size; k++) if (buf[k] != elfReader.SegData (n) [k])
          INFOF (("%04x: correct %02x, got %02x", k + elfReader.SegAdr (n), (int) elfReader.SegData (n) [k], (int) buf[k]));
        return false;
      }
      free (buf);
      puts ("OK");
    }
  }

  // Lock FLASH again ...
  if (PrintOnError (
    shellLink.RegWrite (shellAdr, BR_REG_CTRL, 0)
  )) return false;

  // Success...
  return true;
}





// ************************* CmdUpgrade ****************************************


#define CMD_UPGRADE \
  { "upgrade", CmdUpgrade, "[ <options> ] [ <ELF file> ]", "Upgrade the operational firmware from a running operational firmware", CmdUpgradeExtraHelp } \


static const char *CmdUpgradeExtraHelp () {
  return  "Options:\n"
          "-y   : Do not ask for confirmation\n";
}


static bool CmdUpgrade (int argc, const char **argv) {
  CString s;
  const char *elfName;
  bool ok, yesSure;
  int n;

  // Sanity ...
  if (!CheckLegalTwiAdr ()) return false;

  // Parse options ...
  yesSure = false;
  elfName = NULL;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') {
      switch (argv[n][1]) {
        case 'y':
          yesSure = true;
          break;
        default:
          return ArgError (argv);
      }
    }
    else {
      if (elfName) return ArgError (argv);
      elfName = argv[n];
    }
  }

  // Run upgrade procedure ...
  if (!ExecuteCmd ("boot -m")) return false;      // boot into maintenance
  ok = ExecuteCmd (StringF (&s, "program%s %s", yesSure ? " -y" : "", elfName ? elfName : "-d"));
                                                  // program
  if (!ExecuteCmd ("boot -o")) ok = false;        // boot operational system
  if (!ok) return false;
  if (!elfName) if (!ExecuteCmd (StringF (&s, "config%s -d", yesSure ? " -y" : ""))) return false;
                                                  // write config and test

  // Success ...
  putchar ('\n');     // for the case of a "for" loop
  return true;
}





// ************************* CmdHub ********************************************


#define CMD_HUB \
  { "hub", CmdHub, "<operation>", "Perform a hub maintenance operation", CmdHubExtraHelp } \


static const char *CmdHubExtraHelp () {
  return  "Operations:\n"
          "-0   : Switch off subnet\n"
          "-1   : Switch on subnet\n"
          "-m   : Put all subnet devices into maintenance mode by resurrection\n"
          "\n"
          "These commands require a hub with the subnet power line being\n"
          "controlled by GPIO #0, active off (0 = power on, 1 = power off).\n";
}


static bool CmdHub (int argc, const char **argv) {
  CBrownie brownie;
  uint8_t gpioVal;

  // Sanity ...
  if (argc != 2) return ArgError (argv);
  if (argv[1][0] != '-' || argv[1][2] != '\0') return ArgError (argv);
  if (!CheckLegalTwiAdr ()) return false;

  // Check device ...
  if (PrintOnError (
    shellLink.CheckDevice (shellAdr, &brownie)
  )) return false;
  if (!(brownie.FeatureRecord ()->features & BR_FEATURE_TWIHUB) ||
      !(brownie.FeatureRecord ()->gpoPresence & 1) ) {
  //~ if (!strstr (brownie.FeatureRecord ()->fwName, "hub.") ||
      //~ (brownie.FeatureRecord ()->gpoPresence & 1) == 0) {
    puts ("Error: This device does not appear to be a suitable hub.");
    return false;
  }

  // Execute operation ...
  switch (argv[1][1]) {
    case '0':
      printf ("Hub %03i: Powering off subnet.\n", shellAdr);
      fflush (stdout);
      if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_GPIO_0, &gpioVal))) return false;
      if (PrintOnError (shellLink.RegWrite (shellAdr, BR_REG_GPIO_0, gpioVal | 1))) return false;
      break;
    case '1':
      printf ("Hub %03i: Powering on subnet.\n", shellAdr);
      fflush (stdout);
      if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_GPIO_0, &gpioVal))) return false;
      if (PrintOnError (shellLink.RegWrite (shellAdr, BR_REG_GPIO_0, gpioVal & ~1))) return false;
      break;
    case 'm':
      printf ("Hub %03i: Resurrecting all subnet devices into maintenance mode.\n", shellAdr);
      fflush (stdout);
      if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_GPIO_0, &gpioVal))) return false;
      // Power off subnet ...
      if (PrintOnError (shellLink.RegWrite (shellAdr, BR_REG_GPIO_0, gpioVal | 1))) return false;
      // Set TWI master to resurrection mode ...
      if (PrintOnError (shellLink.RegWrite (shellAdr, BR_REG_CTRL, BR_CTRL_HUB_RESURRECTION))) return false;
      // Power on subnet ...
      if (PrintOnError (shellLink.RegWrite (shellAdr, BR_REG_GPIO_0, gpioVal & ~1))) return false;
      // Wait a second ...
      Sleep (1000);
      // Stop TWI master from pulling SCL & SDA ...
      if (PrintOnError (shellLink.RegWrite (shellAdr, BR_REG_CTRL, 0))) return false;
      break;
    default:
      return ArgError (argv);
  }

  // Success ...
  return true;
}





// ************************* CmdStatistics *************************************


#define CMD_STATS \
  { "statistics", CmdStatistics, "<options>", "Print link statistics", CmdStatisticsExtraHelp } \


static const char *CmdStatisticsExtraHelp () {
  return  "Options:\n"
          "-l : Select local counters, i.e. in case of a socket link: statistics of commands issued by the Brownie2L\n"
          "-r : Reset all counters\n";
}


static bool CmdStatistics (int argc, const char **argv) {
  CString s;
  int n;
  bool reset, local;

  // Parse args ...
  reset = local = false;
  for (n = 1; n < argc; n++) {
    if (argv[n][0] == '-') switch (argv[n][1]) {
      case 'l': local = true; break;
      case 'r': reset = true; break;
      default: return ArgError (argv);
    }
    else return ArgError (argv);
  }

  // Go ahead ...
  if (reset) shellLink.StatisticsReset ();
  printf ("\n%s\n", shellLink.StatisticsStr (&s, local));

  // Success...
  return true;
}





// ************************* CmdTimer *******************************************


#define CMD_TIMER \
  { "timer", CmdTimer, "[ <delay> ]", "Measure the timer accuracy; <delay> is passed in ms, default = 1000", NULL }


static bool CmdTimer (int argc, const char **argv) {
  CBrownie brownie;
  TTicks t0before, t0after, t1before, t1after, delay;
  int i, brT0, brT1;
  float msLocal, msBrownie, msCom0, msCom1;
  uint8_t byte;

  // Arguments & sanity ...
  delay = 1000;    // default
  if (argc >= 2) {
    if (IntFromString (argv[1], &i)) delay = i;
    else {
      printf ("Invalid delay value: %s\n", argv[1]);
      return false;
    }
  }
  if (!CheckLegalTwiAdr ()) return false;

  // Read out brownie ...
  if (PrintOnError (
    shellLink.CheckDevice (shellAdr, &brownie))
  ) return false;
  if ((brownie.FeatureRecord ()->features & BR_FEATURE_TIMER) == 0) {
    printf ("This Brownie firmware does not have a timer!\n");
    return false;
  }

  // Read ticks from brownie ...
  printf ("Testing timer (delay = %i ms)... ", (int) delay);
  fflush (stdout);
  t0before = TicksNowMonotonic ();
  if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_TICKS_LO, &byte))) return false;
  brT0 = byte;
  if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_TICKS_HI, &byte))) return false;
  t0after = TicksNowMonotonic ();
  brT0 |= (byte << 8);

  // Wait and read again ...
  Sleep (delay);
  t1before = TicksNowMonotonic ();
  if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_TICKS_LO, &byte))) return false;
  brT1 = byte;
  if (PrintOnError (shellLink.RegRead (shellAdr, BR_REG_TICKS_HI, &byte))) return false;
  t1after = TicksNowMonotonic ();
  brT1 |= (byte << 8);

  // Print result ...
  //~ INFOF (("### brT0 = %i, brT1 = %i", brT0, brT1));
  msBrownie = BR_MS_OF_TICKS(brT1 - brT0);
  //~ INFOF (("### BR_MS_OF_TICKS(...) = %.1f", BR_TICKS_PER_MS));
  msLocal = 0.5 * (t1before - t0before + t1after - t0after);
  msCom0 = t0after - t0before;
  msCom1 = t1after - t1before;
  printf ("  local delay = %.1f ms, brownie delay = %.1f ms (*** %.1f%% ***); "
          "  communication time: %.1f ms (%.1f%%) and %.1f ms (%.1f%%)\n",
          msLocal, msBrownie, 100.0 * msBrownie / msLocal,
          msCom0, 100.0 * msCom0 / msLocal, msCom1, 100.0 * msCom1 / msLocal);

  // Success...
  return true;
}




// ************************* CmdTest *******************************************


#define CMD_TEST \
  { "test", CmdTest, "[ -l ]", "Run a communication test and print statistics. Statistics are reset before. Add '-l' to perform an endless loop.", NULL }


static bool CmdTest (int argc, const char **argv) {
  CString s;
  int i;
  uint8_t val, oldVal;
  TTicks t0, t1;
  EBrStatus status;
  bool endless;

  // TBD: Sanity: Check address, check node...
  endless = false;
  if (argc >= 2)
    if (argv[1][0] == '-' && argv[1][1] == 'l') endless = true;

  status = shellLink.RegRead (shellAdr, BR_REG_FWBASE, &oldVal);
  if (status != brOk) {
    printf ("Error: %s\n", BrStatusStr (status));
    return false;
  }

  shellLink.StatisticsReset (true);
  if (endless) puts ("Push Ctrl-C to stop the test.");
  interrupted = false;
  t0 = TicksNowMonotonic ();
  for (i = 0; (i <= 0xff || endless) && !interrupted; i++) {
    shellLink.RegWrite (shellAdr, BR_REG_FWBASE, endless ? 0x55 : (uint8_t) i);
    shellLink.RegRead (shellAdr, BR_REG_FWBASE, &val);
    if (!endless) putchar ((val == (uint8_t) i) ? '.' : '!');
    fflush (stdout);
  }
  t1 = TicksNowMonotonic ();
  printf ("\n\n%s", shellLink.StatisticsStr (&s, true));
  printf ("\nElapsed time: %i.%03i secs (%.2f kbit/s).\n\n",
          (int) ((t1 - t0) / 1000), (int) ((t1 - t0) % 1000),
          (256 * (BrRequestSize (BR_OP_REG_WRITE(0)) + BrReplySize (BR_OP_REG_WRITE(0)) +
                  BrRequestSize (BR_OP_REG_READ(0)) + BrReplySize (BR_OP_REG_READ(0)))
               * 8) / 1024.0 / (float) (t1 - t0) * 1000.0
          );

  shellLink.RegWrite (shellAdr, BR_REG_FWBASE, oldVal);

  // Success...
  return true;
}





// ************************* CmdResources **************************************


#define CMD_RESOURCES \
  { "resources", CmdResources, "", "Run the Resources driver and monitor all events (includes server, if enable via 'rc.enableServer').", NULL }


static CBrownieSet rcDatabase;              // Use local database to not linter with the shell's one
static CRcSubscriber *rcSubscriber = NULL;  // link to active subscriber; if set, will be interrupted by signal handler


static bool CmdResources (int argc, const char **argv) {
  CRcEventDriver *drv;
  CRcSubscriber subscriber;
  CRcEvent ev;
  CString s1, s2;
  int n, count;
  bool haveSocketClient, linkFailure;

  // Sanity...
  ASSERT (envBrDatabaseFile != NULL);
  if (!envBrDatabaseFile[0]) {
    puts ("No database file!");
    return false;
  }

  puts ("Initializing Resources ...");
  RcInit (true);

  puts ("Initializing and registering driver ...");
  if (!rcDatabase.ReadDatabase ())
    WARNINGF (("Failed to read database '%s': No resources.", envBrDatabaseFile));
  drv = RcRegisterDriver ("brownies", rcsBusy);   // Register an event-based driver
  rcDatabase.ResourcesInit (drv, &shellLink);

  puts ("\nRunning Resources (press Ctrl-C to stop) ...");
  RcStart ();

  subscriber.Register ("brownie2l");
  count = drv->LockResources ();
  for (n = 0; n < count; n++) subscriber.AddResource (drv->GetResource (n));
  drv->UnlockResources ();
  subscriber.GetInfo (&s1);
  s2.SetFByLine ("  %s\n", s1.Get ());
  printf ("%s", s2.Get ());
  fflush (stdout);

  rcSubscriber = &subscriber;   // let signal handler interrupt us
  shellLink.ServerStart ();
  linkFailure = false;
  while (!interrupted) {
    RcIterate ();
    haveSocketClient = shellLink.ServerIterate (64);
    rcDatabase.ResourcesIterate (haveSocketClient);
    while (subscriber.PollEvent (&ev)) {
      printf (": %s\n", ev.ToStr (&s1));
      fflush (stdout);
    }
    if (shellLink.Status () == brNoBus) {
      if (!linkFailure) INFO ("Link failure - trying to re-open");
      linkFailure = true;
      Sleep (64);
      if (shellLink.Reopen () != brNoBus) {
        INFO ("Link successfully re-openend");
        linkFailure = false;
      }
    }
  }
  shellLink.ServerStop ();
  rcSubscriber = NULL;          // disable signal handler interrupt
  subscriber.Clear ();

  puts ("\nShutting down Resources.");
  RcDone ();
  putchar ('\n');

  // Success...
  return true;
}





// ************************* Interpreter / Main ********************************


typedef bool (*TCmdFunc) (int argc, const char **argv);


struct TCmd {
  const char *name;     // command to type
  TCmdFunc func;
  const char *helpArgs, *helpText;    // help: arguments, text
  const char * (*getExtraText) (void);  // help: Generator for extra text
};


static TCmd commandList[] = {
  { "h", CmdHelp, "[<command>]", "Print help [on <command>]", NULL },
  { "help", CmdHelp, NULL, NULL, NULL },

  { "q", CmdQuit, "", "Quit", NULL },
  { "quit", CmdQuit, NULL, NULL, NULL },

  CMD_OPEN,
  CMD_FOR,
  CMD_SCAN,
  CMD_REG_READ,
  CMD_REG_WRITE,
  CMD_MEM_READ,
  CMD_CONFIG,
  CMD_BOOT,
  CMD_INIT,
  CMD_PROGRAM,
  CMD_UPGRADE,
  CMD_HUB,
  CMD_STATS,
  CMD_TIMER,
  CMD_TEST,
  CMD_RESOURCES
};


#define commands ((int) (sizeof (commandList) / sizeof (TCmd)))


static void SignalHandler (int) {
  //~ INFOF (("### SignalHandler: Entry"));
  interrupted = true;
  if (rcSubscriber) rcSubscriber->Interrupt ();
  //~ INFOF (("### SignalHandler: Exit"));
}


static TCmdFunc GetCmdFunc (const char *arg) {
  int n;

  for (n = 0; n < commands; n++)
    if (strcmp (arg, commandList[n].name) == 0) return commandList[n].func;
  return NULL;
}


static bool ExecuteCmd (int argc, const char **argv) {
  TCmdFunc cmdFunc;

  ASSERT (argc >= 1);
  cmdFunc = GetCmdFunc (argv[0]);
  if (!cmdFunc) {
    printf ("Error: Unknown command '%s'\n", argv[0]);
    return false;
  }
  return cmdFunc (argc, (const char **) argv);
}


static bool ExecuteCmd (const char *cmd) {
  char **cmdArgv;
  int cmdArgc;
  bool ret;

  //~ INFOF (("### ExecuteCmd ('%s')", cmd));
  StringSplit (cmd, &cmdArgc, &cmdArgv);
  ret = ExecuteCmd (cmdArgc, (const char **) cmdArgv);
  if (cmdArgv) {
    free (cmdArgv[0]);
    free (cmdArgv);
  }
  return ret;
}




// ***** Special commands *****


static bool CmdHelp (int argc, const char **argv) {
  CString part;
  const char *cmd, *altCmd, *helpArgs, *helpText, *extraText;
  int n, k;
  bool selected, haveOutput;

  putchar ('\n');
  haveOutput = false;
  for (n = 0; n < commands; n++) {
    cmd = commandList[n].name;
    helpArgs = commandList[n].helpArgs;
    helpText = commandList[n].helpText;
    extraText = NULL;
    if (commandList[n].getExtraText) extraText = commandList[n].getExtraText ();

    if (helpArgs && helpText) {
      altCmd = NULL;
      if (n < commands-1) if (!commandList[n+1].helpText) altCmd = commandList[n+1].name;

      if (argc == 1) selected = true;
      else {
        selected = false;
        for (k = 1; k < argc; k++) {
          if (strcmp (argv[k], cmd) == 0) { selected = true; break; }
          if (altCmd) if (strcmp (argv[k], altCmd) == 0) { selected = true; break; }
        }
      }

      //~ INFOF (("### cmd = '%s'/'%s', helpArgs = '%s', helpText = '%s'", cmd, altCmd, helpArgs, helpText));

      if (selected) {
        haveOutput = true;
        part.SetF (altCmd ? "%s|%s %s" : "%s %3$s", cmd, altCmd, helpArgs);
        printf ("%s\n%s    %s\n\n", part.Get (), argc == 1 ? "" : "\n", helpText);
        if (extraText && argc > 1) {
          part.SetFByLine ("    %s\n", extraText);
          printf ("%s\n", part.Get ());
        }
      }
    }
  }

  // Done ...
  if (!haveOutput) ArgError (argv);
  return haveOutput;
}


static bool CmdQuit (int argc, const char **argv) {
  doQuit = true;
  return true;
}





// ***** Readline hooks *****


#if WITH_READLINE


static CKeySet rlCompletions;     // current completion candidates
static int rlCompletionsIdx;      // next completion from 'rlCompletions' to be considered
static int rlCompletionsTextLen;

static int rlCompleteOffset;      // number of characters to skip in the completion display


static void RlDisplayMatchList (char **matches, int len, int max) {
  char **shortMatches;
  int n;

  shortMatches = MALLOC (char *, len + 1);
  for (n = 0; n <= len; n++) shortMatches[n] = matches[n] + rlCompleteOffset;
  rl_display_match_list (shortMatches, len, max - rlCompleteOffset);
  FREEP (shortMatches);
  rl_forced_update_display ();      // force to redisplay the prompt
}


static void RlInitCompletions (int textLen, int offset = 0) {
  rlCompletions.Clear ();
  rlCompletionsIdx = 0;
  rlCompletionsTextLen = textLen;
  rlCompleteOffset = offset;
}


static char *RlNextCompletion (const char *text) {
  char *s;
  int len;

  len = strlen (text);
  while (rlCompletionsIdx < rlCompletions.Entries ()) {
    s = (char *) rlCompletions.GetKey (rlCompletionsIdx++);
    if (strncmp (s, text, len) == 0) return strdup (s);
  }
  return NULL;
}





// ***** Readline: Custom Generators *****


static char *RlGeneratorNothing (const char *text, int state) {
  return NULL;
}


static char *RlGeneratorCommands (const char *text, int state) {
  int n;

  //~ INFOF (("### RlGeneratorCommands ('%s', %i)", text, state));

  // Init if necessary ...
  if (!state) {
    RlInitCompletions (strlen (text));
    for (n = 0; n < commands; n++)
      rlCompletions.Set (commandList[n].name);
  }

  // Completion from 'rlCompletions' ...
  return RlNextCompletion (text);
}


static char *RlGeneratorConfig (const char *text, int state) {
  int n;

  // Init if necessary ...
  if (!state) {
    RlInitCompletions (strlen (text));
    for (n = 0; n < brCfgDescs; n++)
      rlCompletions.Set (brCfgDescList[n].key);
  }

  // Completion from 'rlCompletions' ...
  return RlNextCompletion (text);
}


static char *RlGeneratorBrownies (const char *text, int state) {
  CBrownie *brownie;
  int n;

  // Init if necessary ...
  if (!state) {
    RlInitCompletions (strlen (text));
    for (n = 1; n < 128; n++) if ( (brownie = shellDatabase.Get (n)) )
      rlCompletions.Set (brownie->Id ());
  }

  // Completion from 'rlCompletions' ...
  return RlNextCompletion (text);
}


static char *RlGeneratorRegisters (const char *text, int state) {
  const char *name;
  int n;

  // Init if necessary ...
  if (!state) {
    RlInitCompletions (strlen (text));
    for (n = 0; n < 0x40; n++) if ( (name = BrRegName (n)) )
      rlCompletions.Set (name);
  }

  // Completion from 'rlCompletions' ...
  return RlNextCompletion (text);
}


static char *RlGeneratorInstalledElfs (const char *text, int state) {
  CString s;
  const char *name;
  int n, len;

  // Init if necessary ...
  if (!state) {
    RlInitCompletions (strlen (text));
    if (!ReadDir (EnvGetHome2lRootPath (&s, BROWNIE_ELF_DIR), &rlCompletions)) rlCompletions.Clear ();
    for (n = rlCompletions.Entries () - 1; n >= 0; n--) {
      name = rlCompletions.GetKey (n);
      len = strlen (name);
      if (strcmp (name + len - 4, ".elf") != 0) rlCompletions.Del (n);
    }
    rlCompletions.Set ("./");
    rlCompletions.Set ("../");
  }

  // Completion from 'rlCompletions' ...
  return RlNextCompletion (text);
}







// ***** Readline: Completion Function *****


static char **RlCompletionFunction (const char *text, int start, int end) {
  CSplitString cmdList, argList;
  CString cmd;
  TCmdFunc cmdFunc;
  rl_compentry_func_t *generator;
  int argc, argNo, cmdNo;

  // Analyse current command line ...
  cmdList.Set (rl_line_buffer, INT_MAX, ";");
  cmdNo = cmdList.GetIdx (start);
  if (cmdNo >= 0 && cmdNo < cmdList.Entries ())
    cmd.Set (cmdList.Get (cmdNo));
  argList.Set (cmd);
  argc = argList.Entries ();
  cmdFunc = (argc > 0) ? GetCmdFunc (argList.Get (0)) : NULL;
  argNo = argList.GetIdx (start - cmdList.GetOffset (cmdNo));
  if (start == end) argNo++;     // start of a not yet existing word

  //~ INFOF (("### RlCompletionFunction: rl_line_buffer = '%s', text = '%s', argNo = %i", rl_line_buffer, text, argNo));
  //~ INFOF (("###   cmd = '%s', argList[%i] = '%s'", cmd.Get (), argList.Entries (), argList.Get (argList.Entries () - 1)));

  // Determine generator (NULL = do file name completion) ...
  generator = RlGeneratorNothing;
  //   ... special case: 'for' ...
  if (cmdFunc == CmdFor) {
    if (argNo == 1) generator = RlGeneratorBrownies;
    else if (argNo >= 2) {
      cmdFunc = (argc > 2) ? GetCmdFunc (argList.Get (2)) : NULL;
      argNo -= 2;
    }
  }
  //   ... complete command names? ...
  if (argNo == 0 || cmdFunc == CmdHelp) generator = RlGeneratorCommands;
  //   ... command-specific objects ...
  else if (cmdFunc == CmdConfig) generator = RlGeneratorConfig;
  else if (cmdFunc == CmdOpen) generator = RlGeneratorBrownies;
  else if (cmdFunc == CmdProgram || cmdFunc == CmdUpgrade) {
    if (strchr (text, '/') == NULL) generator = RlGeneratorInstalledElfs;
    else generator = NULL;    // word contains a '/' => do filename completion
  }
  else if (cmdFunc == CmdRegRead || cmdFunc == CmdRegWrite) {
    if (argNo == 1) generator = RlGeneratorRegisters;
  }
  //   ... '-d <dbBrownie>' option ...
  //~ if (argNo >= 2) INFOF (("###   argList[1] = '%s', cmd = '%s'", argList.Get (argNo-1), cmd.Get ()));
  if (argNo >= 2 && strcmp (argList.Get (argNo-1), "-d") == 0)
    generator = RlGeneratorBrownies;

  // Return completion ...
  if (!generator) return NULL;        // do filename expansion
  rl_attempted_completion_over = 1;   // disable filename expansion
  return rl_completion_matches (text, generator);
}


#endif // WITH_READLINE





// *************************** Main ********************************************


int main (int argc, char **argv) {
#if WITH_READLINE
  static const char *prompt = "\001\033[1m\002brownie2l>\001\033[0m\002 ";   // The '\001' and '\002' mark invisible characters, so that libreadline knows about the visible length of the prompt.
  const char *homeDir;
  CString lastLine;
#else
  static const char *prompt = "\033[1mbrownie2l>\033[0m ";
  char buf[256];
#endif
  struct sigaction sigAction, sigActionSaved;
  CSplitString argCmds;
  CString line, histFile;
  char *p;
  int n;
  bool interactive;

  // Startup...
  EnvInit (argc, argv,
           "  -e '<command(s)>' : execute the command(s) and quit\n"
           "  -i '<command(s)>' : execute the command(s), then continue interactively\n");

  // Read arguments...
  interactive = true;
  p = NULL;
  for (n = 1; n < argc; n++) if (argv[n][0] == '-')
    switch (argv[n][1]) {
      case 'e':
        interactive = false;
      case 'i':
        p = argv[n+1];
        break;
    }
  if (p) argCmds.Set (p, INT_MAX, ";");

  // Open TWI device...
  if (shellLink.Open () == brOk)
    printf ("Connected to '%s' (%s).\n", shellLink.IfName (), TwiIfTypeStr (shellLink.IfType ()));
  else {
    printf ("\nNo Brownie link available.\n");      // Detailed information was logged by 'shellLink.Open ()').
  }

  // Load Brownie database...
  if (envBrDatabaseFile) if (envBrDatabaseFile[0]) {    // skip, if explicitly disabled
    if (shellDatabase.ReadDatabase ())
      printf ("Read database file '%s'.\n\n", envBrDatabaseFile);
    else {
      printf ("No or no completely valid database file '%s' found.\n\n", envBrDatabaseFile);
    }
    //~ shellDatabase.WriteDatabase ();
  }

  // Prepare interactive mode...
  if (interactive) {

    // Setup history...
#if WITH_READLINE
    using_history ();
    homeDir = getenv ("HOME");
    if (!homeDir) homeDir = "/";
    GetAbsPath (&histFile, envBrownie2lHistFile, homeDir);
    if (envBrownie2lHistLines > 0) read_history (histFile.Get ());
#endif

    // Set signal handler for keyboard interrupt (Ctrl-C)...
    sigAction.sa_handler = SignalHandler;
    sigemptyset (&sigAction.sa_mask);
    sigAction.sa_flags = 0;
    sigaction (SIGINT, &sigAction, &sigActionSaved);
  }

  // Run non-interactive commands...
  interrupted = false;
  for (n = 0; n < argCmds.Entries () && !interrupted; n++) {
    line.Set (argCmds.Get (n));
    line.Strip ();
    if (line[0]) {
      if (interactive) {
        for (p = (char *) prompt; *p; p++) if (*p > '\002') putchar (*p);
        puts (line.Get ());
      }
#if WITH_READLINE
      if (interactive) {
        if (line.Compare (lastLine.Get ()) != 0) {
          add_history (line);
          lastLine.Set (line);
        }
      }
#endif
      ExecuteCmd (line.Get ());
    }
  }
  //~ INFO ("### Non-interactive commands completed.");

  // Run interactive main loop...
  if (interactive) {

    // Main input loop...
#if WITH_READLINE
    rl_readline_name = "brownie2l";     // allow conditional parsing of the ~/.inputrc file.
    rl_attempted_completion_function = RlCompletionFunction;    // Tab-completion
    rl_completion_display_matches_hook = RlDisplayMatchList;    // Abbreviation of match lists
    rl_completer_word_break_characters = (char *) " ";
#endif
    while (!doQuit) {
#if WITH_READLINE
      p = readline (EnvHaveTerminal () ? prompt : NULL);
      if (!p) {
        if (EnvHaveTerminal ()) putchar ('\n');
        break;
      }
      line.SetO (p);
#else
      fputs (prompt, stdout);
      fflush (stdout);
      fgets (buf, sizeof (buf), stdin);
      if (feof (stdin)) {
        putchar ('\n');
        doQuit = true;
        break;
      }
      line.SetC (buf);
#endif
      line.Strip ();
      if (line[0]) {

        // Line not empty: Store in history and execute...
#if WITH_READLINE
        if (line.Compare (lastLine.Get ()) != 0) {
          add_history (line);
          lastLine.Set (line);
        }
#endif
        interrupted = false;
        argCmds.Set (line.Get (), INT_MAX, ";");
        for (n = 0; n < argCmds.Entries () && !interrupted; n++) {
          line.Set (argCmds.Get (n));
          line.Strip ();
          ExecuteCmd (line);
        }
      }
    }

    // Write back history...
#if WITH_READLINE
    if (envBrownie2lHistLines > 0) {
      stifle_history (envBrownie2lHistLines);
      write_history (histFile.Get ());
    }
#endif

    // Restore signal handler for keyboard interrupt (Ctrl-C)...
    sigaction (SIGINT, &sigActionSaved, NULL);
  }
  //~ INFO ("### Interactive commands completed.");

  // Done...
  shellLink.Close ();
  EnvDone ();
  return 0;
}
