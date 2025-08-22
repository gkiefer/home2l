# This file is derived from https://github.com/rshcs/Grow-R503-Finger-Print/
# with some modifications for the Home2L project.
#
# MIT License
#
# Copyright (c) 2023 RoshanCS
#               2024 Gundolf Kiefer (Home2L adaptions only)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


# Modifications by Gundolf Kiefer:
#
# - add license header
#
# - eliminate 'confirmation_codes.json' to avoid file search issues
#
# - __init__: allow to pass a device file name as a 'port' (e.g. 'dev/ttyUSB0' on Linux)
#
# - ser_send: Add checksum verification
#
# - ser_send: considerably improve performance by receiving the exact mumber of
#   bytes indicated by 'len' instead of waiting for a timeout each time;
#   eliminate 'R503.rcv _size'


import serial
from time import sleep, time
from struct import pack, unpack
from platform import system


def to_hex (packet):
  """
  Helper to convert a packed byte array to a readable hex string (for debugging only).
  """
  s = ""
  for b in packet: s += " {:02x}".format (b)
  return s.strip ()


class R503:
    """
    R503 class for interacting with R503 fingerprint sensor module.
    """
    header = pack('>H', 0xEF01)
    pid_cmd = 0x01  # pid_command packet

    def __init__(self, port, baud=57600, pw=0, addr=0xFFFFFFFF, timeout=1):
        """
        Initialize the R503 class instance.
        Parameters:
          port (int): The COM port number (Windows) or device file name (Linux)
          baud (int): The baud rate, default 57600
          pw (int): The password, default 0
          addr (int): The module address, default 0xFFFFFFFF
          timeout (int): The serial timeout in seconds, default 1
        This initializes the R503 instance attributes like pw, addr etc.
        It opens the serial port with the given parameters.
        """
        self.pw = pack('>I', pw)
        self.addr = pack('>I', addr)
        if (isinstance (port, int)):
          port_name = f'COM{port}' if system() == 'Windows' else f'/dev/ttyUSB{port}'
        else:
          port_name = port
        self.ser = serial.Serial (port_name, baudrate=baud, timeout=timeout)

    def ser_close(self):
        """
        Closes the serial port
        """
        self.ser.close()

    def set_pw(self, new_pw):
        """
        Set modules handshaking password
        parameters: (int) new_pw - New password
        returns: (int) confirmation code
        """
        self.pw = pack('>I', new_pw)
        recv_data = self.ser_send(pid=0x01, pkg_len=0x07, instr_code=0x12, pkg=self.pw)
        return recv_data[4]

    def set_address(self, new_addr):
        """
        Set module address
        *Set the new address when setting the class object next time*
        parameter: (int) new_addr
        returns: (int) confirmation code => 0 [success], 1, 24, 99
        """
        self.addr = pack('>I', new_addr)
        recv_data = self.ser_send(pid=0x01, pkg_len=0x07, instr_code=0x15, pkg=self.addr)
        return recv_data[4]

    def read_msg(self, data_stream):
        """
        Unpack byte stream to readable data
        returns: (tuple) header, address, package id, package len, confirmation code, package, checksum
        """
        hdr_rd, adr_rd, pkg_id_rd, pkg_len_rd, conf_code_rd = unpack('>HIBHB', data_stream[:10])
        pkg = data_stream[10:len(data_stream)-2]
        chksum_rd = unpack('>H', data_stream[-2:])
        return hdr_rd, adr_rd, pkg_id_rd, pkg_len_rd, conf_code_rd, None if pkg == b'' else pkg, sum(chksum_rd)

    def cancel(self):
        """
        Cancel instruction
        returns: (int) confirmation code
        """
        recv_data = self.ser_send(pid=0x01, pkg_len=3, instr_code=0x30)
        return recv_data[4]

    def led_control(self, ctrl=0x03, speed=0, color=0x01, cycles=0):
        """
        ctrl: (int) 1 to 6
        1: breathing light, 2: flashing light, 3: always on, 4: always off, 5: gradually on, 6: gradually off
        speed: (int) 0 to 255
        color: (int) 0 to 7
        cycles: (int) 0 to 255
        returns: confirmation code
        """
        cmd = pack('>BBBB', ctrl, speed, color, cycles)
        return self.ser_send(pkg_len=0x07, instr_code=0x35, pkg=cmd)[4]

    def set_baud(self, baud=57600):
        """
        Set the baud rate for serial communication.

        This function sets the baud rate for serial communication to one of
        the allowed values: 9600, 19200, 38400, 57600 (default), 115200.

        Parameters:
            self: The R503 class instance.
            baud (int): The desired baud rate. Default is 57600.

        Returns:
            conf_code (int): The confirmation code received after setting
                the baud rate. 0 means success.

        It calculates the baud rate divisor, checks if it is valid, sends the
        set baud rate command, updates the self.ser.baudrate if successful,
        and returns the confirmation code.
        """
        baud0 = int(baud / 9600)
        if baud0 not in [1, 2, 4, 6, 12]:
            return 102
        conf_code = self.ser_send(pid=0x01, pkg_len=0x05, instr_code=0x0E, pkg=pack('>BB', 4, baud0))[4]
        if conf_code:
            return conf_code
        self.ser.baudrate = baud
        return conf_code

    def set_security(self, lvl=3):
        """
        Set the security level of the fingerprint sensor.

        This function sets the security level to one of 5 levels:
        1: Low
        2: Medium
        3: High (default)
        4: Higher
        5: Highest

        Parameters:
            self: The R503 class instance.
            lvl (int): The desired security level, 1-5. Default is 3.

        Returns:
            conf_code (int): The confirmation code received after setting the
            security level. 0 means success.

        It checks if the security level is valid, sends the set security
        command with the level, and returns the confirmation code response.
        """
        if lvl not in [1, 2, 3, 4, 5]:
            return 102
        return self.ser_send(pid=0x01, pkg_len=0x05, instr_code=0x0E, pkg=pack('>BB', 5, lvl))[4]

    def set_pkg_length(self, pkg_len=128):
        """
        Set the package length for serial communication.

        This function sets the package length to one of the allowed
        values: 32, 64, 128 (default), 256 bytes.

        Parameters:
            self: The R503 class instance.
            pkg_len (int): The desired package length. Default is 128.

        Returns:
            conf_code (int): The confirmation code received after setting
                the package length. 0 means success.

        It maps the package lengths to index values, checks if valid,
        sends the set command, and returns the confirmation code response.
        """
        pkg_len0 = {32: 0, 64: 1, 128: 2, 256: 3}.get(pkg_len)
        if pkg_len0 not in [0, 1, 2, 3]:
            return 102
        conf_code = self.ser_send(pid=0x01, pkg_len=0x05, instr_code=0x0E, pkg=pack('>BB', 6, pkg_len0))[4]
        return conf_code

    def read_sys_para(self):
        """
        Status register and other basic configuration parameters
        returns: (list) status_reg, sys_id_code, finger_lib_size, security_lvl, device_addr, data_packet_size, baud_rate
        """
        read_pkg = self.ser_send(pkg_len=0x03, instr_code=0x0F)
        return 99 if read_pkg[4] == 99 else unpack('>HHHHIHH', read_pkg[5])

    def read_sys_para_decode(self):
        """
        Get system parameters in a decoded, human-readable format.

        Otherwise, it returns a dictionary with the parameters decoded:

        - system_busy: boolean
        - matching_finger_found: boolean
        - pw_verified: boolean
        - valid_image_in_buffer: boolean
        - system_id_code: int
        - finger_library_size: int
        - security_level: int
        - device_address: int
        - data_packet_size: int
        - baud_rate: int

        Parameters:
            self: The R503 instance.

        Returns:
            dict: Decoded system parameters if successful, else 99.
        """
        rsp = self.read_sys_para()
        if rsp == 99:
            return 99
        return {
            'system_busy': bool(rsp[0] & 1),
            'matching_finger_found': bool(rsp[0] & 2),
            'pw_verified': bool(rsp[0] & 4),
            'valid_image_in_buffer': bool(rsp[0] & 8),
            'system_id_code': rsp[1],
            'finger_library_size': rsp[2],
            'security_level': rsp[3],
            'device_address': hex(rsp[4]),
            'data_packet_size': {0: 32, 1: 64, 2: 128, 3: 256}[rsp[5]],
            'baud_rate': rsp[6] * 9600,
        }

    def verify_pw(self, pw=0x00):
        """
        Verify modules handshaking password
        returns: (int) confirmation code
        """
        recv_data = self.ser_send(pkg_len=0x07, instr_code=0x13, pkg=pack('>I', pw))
        return recv_data[4]

    def handshake(self):
        """
        Send handshake instructions to the module, Confirmation code 0 receives if the sensor is normal
        returns: (int) confirmation code
        """
        recv_data = self.ser_send(pkg_len=0x03, instr_code=0x40)
        return recv_data[4]

    def check_sensor(self):
        """
        Check whether the sensor is normal
        returns: (int) confirmation code
        """
        recv_data = self.ser_send(pkg_len=0x03, instr_code=0x36)
        return recv_data[4]

    def confirmation_decode(self, c_code):
        """
        Decode confirmation code to understandable string
        parameter: (int) c_code - confirmation code
        returns: (str) decoded confirmation code
        """
        cc = {
              0: "00h: command execution complete",
              1: "01h: error when receiving data package",
              2: "02h: no finger on the sensor",
              3: "03h: fail to enroll the finger",
              6: "06h: fail to generate character file due to the over-disorderly fingerprint image",
              7: "07h: fail to generate character file due to lackness of character point or over-smallness of fingerprint image",
              8: "08h: finger does not match",
              9: "09h: fail to find the matching finger",
             10: "0Ah: fail to combine the character files",
             11: "0Bh: addressing PageID is beyond the finger library",
             12: "0Ch: error when reading template from library or the template is invalid",
             13: "0Dh: error when uploading template",
             14: "0Eh: Module cannot receive the following data packages.",
             15: "0Fh: error when uploading image",
             16: "10h: fail to delete the template",
             17: "11h: fail to clear finger library",
             19: "13h: wrong password!",
             21: "15h: fail to generate the image for the lackness of valid primary image",
             24: "18h: error when writing flash",
             25: "19h: No definition error",
             32: "20h: the address code is incorrect",
             33: "21h: password must be verified",
             34: "22h: fingerprint template is empty",
             36: "24h: fingerprint library is empty",
             38: "26h: timeout",
             39: "27h: fingerprints already exist",
             41: "29h: sensor hardware error",
             26: "1Ah: invalid register number",
             27: "1Bh: incorrect configuration of register",
             28: "1Ch: wrong notepad page number",
             29: "1Dh: fail to operate the communication port",
             31: "1Fh: fingerprint library is full",
            252: "FCh: unsupported command",
            253: "FDh: hardware error",
            254: "FEh: command execution failure",
             99: "Data not received from the module **",
            101: "Incorrect page number or content length **",
            102: "Not an expected argument"
          }
        return cc[c_code] if c_code in cc else 'others: system reserved'

    def load_char(self, page_id, buffer_id=1):
        """
        Load template ath the specified location of flash library to template buffer
        parameters: page_id => (int) page number
                    buffer id => (int) character buffer id
        """
        pkg = pack('>BH', buffer_id, page_id)
        recv_data = self.ser_send(pid=0x01, pkg_len=0x06, instr_code=0x07, pkg=pkg)
        return recv_data[4]

    def up_image(self, timeout=5, raw=False):
        """
        Upload the image in Img_Buffer to upper computer
        every image contains the data around 20kilo bytes
        parameter: (int) timeout: timeout could vary if you change the baud rate, for 57600baud 5seconds is sufficient
        If you use a lower baud rate timeout may have to be increased.
        returns: (bytesarray) if raw == True
                 else (list of lists)
        In raw mode returns the data with all headers (address byte, status bytes etc.)
        raw == False mode only returns the image data [all other header bytes are filtered out]
        """
        send_values = pack('>BHB', 0x01, 0x03, 0x0A)
        check_sum = sum(send_values)
        send_values = self.header + self.addr + send_values + pack('>H', check_sum)
        self.ser.write(send_values)
        self.ser.timeout = timeout
        read_val = self.ser.read(22000)
        if read_val == b'':
            return -1
        if read_val[9]:
            return read_val[9]
        return read_val if raw else [img_data[3:-2] for img_data in read_val.split(sep=self.header + self.addr)][2:]

    def down_image(self, img_data):
        """
        Download image from the upper computer to the image buffer
        parameters: img_data (list of lists) image data as a list of lists
        returns: confirmation code
        """
        recv_data0 = self.ser_send(pid=0x01, pkg_len=0x03, instr_code=0x0B)
        if recv_data0[4]:
            return recv_data0[4]
        for img_pkt in img_data[:-1]:
            self.down_packet(img_pkt)
        self.down_packet(img_data[-1], end=True)

    def down_packet(self, img_pkt, end=False):
        """
        Send a downlink data packet to the sensor module.

        Parameters:
           img_pkt (bytes): The image packet data to send.
           end (bool): Whether this packet indicates the end of the image.
               Default is False.

        Returns:
           None
        """
        pkt_len = len(img_pkt)
        content = pack(f'>BH{pkt_len}s', 0x08 if end else 0x02, pkt_len+2, img_pkt)
        checksum = sum(content)
        send_values = self.header + self.addr + content + pack('>H', checksum)
        self.ser.write(send_values)

    def up_char(self, timeout=5, raw=False):
        """
        Upload the data in template buffer to the upper computer
        parameter: (int) timeout: timeout could vary if you change the baud rate, for 57600baud 5seconds is sufficient
        If you use a lower baud rate timeout may have to be increased.
        returns: (bytearray) if raw == True
                 else (list of lists)
        In raw mode returns the data with all headers (address byte, status bytes etc.)
        raw == False mode only returns the image data [all other header bytes are filtered out]
        """
        send_values = pack('>BHBB', 0x01, 0x04, 0x08, 0x01)
        check_sum = sum(send_values)
        send_values = self.header + self.addr + send_values + pack('>H', check_sum)
        self.ser.write(send_values)
        self.ser.timeout = timeout
        read_val = self.ser.read(22000)
        if read_val == b'':
            return -1
        if read_val[9]:
            return read_val[9]
        return read_val if raw else [img_data[3:-2] for img_data in read_val.split(sep=self.header + self.addr)][2:]

    def down_char(self, img_data, buffer_id=1):
        """
        Download a fingerprint template to the sensor module buffer.

        Parameters:
            img_data (list): The fingerprint template data split into packets.
            buffer_id (int): The buffer ID to download to. Default is 1.

        Returns:
            int: The confirmation code from the module.

        This function downloads a full fingerprint template in packets
        to the specified buffer on the sensor module.
        """
        recv_data0 = self.ser_send(pid=0x01, pkg_len=0x04, instr_code=0x09, pkg=pack('>B', buffer_id))
        if recv_data0[4]:
            return recv_data0[4]
        for img_pkt in img_data[:-1]:
            self.down_packet(img_pkt)
        self.down_packet(img_data[-1], end=True)

    def read_info_page(self):
        """
        Read the information page
        returns: (int) confirmation code or (bytearray) info page contents
        """
        send_values = pack('>BHB', 0x01, 0x03, 0x16)
        send_values = self.header + self.addr + send_values + pack('>H', sum(send_values))
        self.ser.write(send_values)
        read_val = self.ser.read(580)
        return 99 if read_val == b'' else read_val[9] or read_val[21:-2]

    def get_img(self):
        """
        Detect a finger and store it in image_buffer
        returns: (int) confirmation code
        """
        read_conf_code = self.ser_send(pkg_len=0x03, instr_code=0x01)
        return read_conf_code[4]

    def get_image_ex(self):
        """
        Detect a finger and store it in image_buffer return 0x07 if image poor quality
        returns: (int) confirmation code
        """
        read_conf_code = self.ser_send(pkg_len=0x03, instr_code=0x28)
        return read_conf_code[4]

    def img2tz(self, buffer_id):
        """
        Generate character file from the original image in Image Buffer and store the file in CharBuffer 1 or 2
        parameter: (int) buffer_id, 1 or 2
        returns: (int) confirmation code
        """
        read_conf_code = self.ser_send(pkg_len=0x04, instr_code=0x02, pkg=pack('>B', buffer_id))
        return read_conf_code[4]

    def reg_model(self):
        """
        Combine info of character files in CharBuffer 1 and 2 and generate a template which is stored back in both
        CharBuffer 1 and 2
        input parameters: None
        returns: (int) confirmation code
        """
        read_conf_code = self.ser_send(pkg_len=0x03, instr_code=0x05)
        return read_conf_code[4]

    def store(self, buffer_id, page_id, timeout=2):
        """
        Store a fingerprint template to the module's flash library.

        This function stores the template from the specified buffer
        (buffer1 or buffer2) to the page number in the flash library.

        Parameters:
            buffer_id (int): 1 for buffer1, 2 for buffer2
            page_id (int): Page number to store the template
            timeout (int): Timeout in seconds. Default is 2.

        Returns:
            conf_code (int): The confirmation code received after storing.
                0 means success.

        It packs the buffer and page IDs into a package, sends the store
        command with the package, and returns the confirmation code response.
        """
        package = pack('>BH', buffer_id, page_id)
        read_conf_code = self.ser_send(pkg_len=0x06, instr_code=0x06, pkg=package, timeout=timeout)
        return read_conf_code[4]

    def manual_enroll(self, location, buffer_id=1, timeout=10, num_of_fps=4, loop_delay=.3):
        """
        Manually enroll a new fingerprint.

        Args:
            location: The memory location to store the fingerprint.
            buffer_id: The buffer id to store intermediate data. Default 1.
            timeout: The timeout in seconds. Default 10.
            num_of_fps: The number of fingerprints to capture. Default 4.
            loop_delay: The delay between capture attempts in seconds. Default 0.3.

        This function will:
            - Prompt the user to place their finger on the sensor and capture fingerprints.
            - Generate character files from the fingerprints.
            - Register a fingerprint model once num_of_fps prints are captured.
            - Store the fingerprint model in the specified memory location.
            - Timeout after timeout seconds if fingerprints are not captured.
        """
        inc = 1
        printed = False
        t1 = time()
        finger_prints = 0
        while True:
            if not printed:
                print(f'Place your finger on the sensor: {inc}')
                printed = True
            if not self.get_image_ex():
                print('Reading the finger print')
                if not self.img2tz(buffer_id=inc):
                    print('Character file generation successful.')
                    finger_prints += 1
                else:
                    print('Character file generation failed !')
                    inc -= 1
                if finger_prints >= num_of_fps:
                    print('registering the finger print')
                    if not self.reg_model():
                        if not self.store(buffer_id=buffer_id, page_id=location):
                            print('finger print registered successfully.')
                        else:
                            print('finger print register failed !')
                        break
                inc += 1
                t1 = time()
                printed = False
            sleep(loop_delay)
            if time() - t1 > timeout:
                print('Timeout')
                break

    def delete_char(self, page_num, num_of_temps_to_del=1):
        """
        Delete stored fingerprint templates.

        Args:
            page_num: The page number to delete templates from.
            num_of_temps_to_del: The number of templates to delete. Default is 1.

        Returns:
            Confirmation code integer.

        This function will:
            - Pack the page number and number of templates to delete into a packet.
            - Send the delete instruction packet to the sensor.
            - Return the confirmation code response from the sensor.
        """
        package = pack('>HH', page_num, num_of_temps_to_del)
        recv_code = self.ser_send(pid=0x01, pkg_len=0x07, instr_code=0x0C, pkg=package)
        return recv_code[4]

    def match(self):
        """
        Compare the recently extracted character with the templates in the ModelBuffer, providing matching result.
        returns: (tuple) status: [0: matching, 1: error, 8: not matching], match score
        """
        rec_data = self.ser_send(pid=0x01, pkg_len=0x03, instr_code=0x03)
        return rec_data[4], rec_data[5]

    def search(self, buff_num=1, start_id=0, para=200):
        """
        Search the whole finger library for the template that matches the one in CharBuffer 1 or 2
        parameters: buff_num = character buffer id, start_id = starting from, para = end position
        returns: (tuple) status [success:0, error:1, no match:9], template number, match score
        """
        package = pack('>BHH', buff_num, start_id, para)
        recv_data = self.ser_send(pid=0x01, pkg_len=0x08, instr_code=0x04, pkg=package)
        if recv_data[4] == 99:
            return (99, -1, 0)
        temp_num, match_score = unpack('>HH', recv_data[5])
        return recv_data[4], temp_num, match_score

    def empty_finger_lib(self):
        """
        Empty all stored fingerprints.

        This function will:
            - Send the empty library instruction to the sensor.
            - Return the confirmation code response.

        Returns:
            Confirmation code integer.
        """
        read_conf_code = self.ser_send(pkg_len=0x03, instr_code=0x0d)
        return read_conf_code[4]

    def read_valid_template_num(self):
        """
        Read number of valid templates stored in module.
        Returns:
            num_templates (int): Number of valid templates stored.
        """
        read_pkg = self.ser_send(pkg_len=0x03, instr_code=0x1d)
        return unpack('>H', read_pkg[5])[0]

    def read_index_table(self, index_page=0):
        """
        Read the fingerprint template index table
        parameters: (int) index_page = 0/1/2/3
        returns: (list) index which fingerprints saved already
        """
        index_page = pack('>B', index_page)
        temp = self.ser_send(pkg_len=0x04, instr_code=0x1f, pkg=index_page)
        if temp[4] == 99:
            return 99
        temp0 = temp[5]
        temp_indx = []
        for n, lv in enumerate(temp0):
            temp_indx.extend(8 * n + i for i in range(8) if (lv >> i) & 1)
        return temp_indx

    def auto_enroll(self, location_id, duplicate_id=1, duplicate_fp=1, ret_status=1, finger_leave=1):
        """
        Automatically register a fingerprint template.
        Parameters:
          location_id (int): The location ID to store the template.
          duplicate_id (int): The duplicate check method.
          duplicate_fp (int): Whether to return duplicate finger status.
          ret_status (int): Return registration status.
          finger_leave (int): Whether finger leaves sensor during registration.
        Returns:
            The confirmation code 0 if success
        """
        package = pack('>BBBBB', location_id, duplicate_id, duplicate_fp, ret_status, finger_leave)
        read_pkg = self.ser_send(pkg_len=0x08, instr_code=0x31, pkg=package)
        return read_pkg[4]

    def auto_identify(self, security_lvl=3, start_pos=0, end_pos=199, ret_key_step=0, num_of_fp_errors=1):
        """
        Search and verify a fingerprint
        return: (tuple) fp store location, match score
        """
        package = pack('>BBBBB', security_lvl, start_pos, end_pos, ret_key_step, num_of_fp_errors)
        read_pkg = self.ser_send(pkg_len=0x08, instr_code=0x32, pkg=package, timeout=10)
        if read_pkg[4] == 99:
            return -1, 0
        _, position, match_score = unpack('>BHH', read_pkg[5])
        return position, match_score

    def read_prod_info(self):
        """
        Read product information from the fingerprint sensor.

        It sends the command, checks the confirmation code, and if
        successful, slices the info byte string into 9 parts:

        - Manufacturer name:
        - Model number:
        - Serial number:
        - Hardware version:
        - Sensor type:
        - Sensor image width:
        - Sensor image height:
        - Template size:
        - Fingerprint database size:

        Parameters:
            self: The R503 instance

        Returns:
            Tuple of 9 info strings if successful, else 99
        """
        info = self.ser_send(pkg_len=0x03, instr_code=0x3c)
        if info[4] == 99:
            return 99
        inf = info[5]
        return inf[:16], inf[16:20], inf[20:28], inf[28:30], inf[30:38], inf[38:40], inf[40:42], inf[42:44], inf[44:46]

    def read_prod_info_decode(self):
        """
        Decode raw product info into a human-readable dictionary.

        This calls read_prod_info() to get the raw info bytes.
        If it returns 99 (error), this returns 99.

        Otherwise, it decodes the raw bytes into a dictionary:

        - module_type: ASCII string
        - batch_number: ASCII string
        - serial_number: ASCII string
        - hw_main_version: Integer
        - hw_sub_version: Integer
        - sensor_type: ASCII string
        - image_width: Integer
        - image_height: Integer
        - template_size: Integer
        - fp_database_size: Integer

        Parameters:
           self: The R503 instance

        Returns:
           dict: Decoded product info if successful, else 99
        """
        inf = self.read_prod_info()
        if inf == 99:
            return 99
        return {
            'module type': inf[0].decode('ascii').replace('\x00', ''),
            'batch number': inf[1].decode('ascii'),
            'serial number': inf[2].decode('ascii'),
            'hw main version': inf[3][0],
            'hw sub version': inf[3][1],
            'sensor type': inf[4].decode('ascii'),
            'image width': unpack('>H', inf[5])[0],
            'image height': unpack('>H', inf[6])[0],
            'template size': unpack('>H', inf[7])[0],
            'fp database size': unpack('>H', inf[8])[0]
        }

    def get_fw_ver(self):
        """
        Get firmware version.
        Parameters:
            self (R503): The R503 instance.
        Returns:
            (int, int): A tuple containing the confirmation code and firmware version.

            The serial number is returned in recv_data[4].
            The firmware version is returned in recv_data[5].
        """
        recv_data = self.ser_send(pid=0x01, pkg_len=3, instr_code=0x3A)
        return recv_data[4], recv_data[5]

    def get_alg_ver(self):
        """
        Get the algorithm version from the fingerprint sensor.
        Parameters:
            self (R503): The R503 instance.
        Returns:
            (int, int): A tuple containing the algorithm version.
            The confirmation code is returned as the first tuple value.
            The algorithm version is returned as the second tuple value.
        """
        recv_data = self.ser_send(pid=0x01, pkg_len=3, instr_code=0x39)
        return recv_data[4], recv_data[5]

    def soft_reset(self):
        """
        Perform a soft reset of the R503 module.
        Parameters:
            self (R503): The R503 instance.
        Returns:
            conf_code (int): The confirmation code received after
                resetting the module. 0 means success.
        """
        return self.ser_send(pid=0x01, pkg_len=3, instr_code=0x3D)[4]

    def get_random_code(self):
        """
        Generate a random 32-bit integer from the sensor module.
        Returns:
            random_num (int): The 32-bit random integer value if success else 99
        """
        read_pkg = self.ser_send(pkg_len=0x03, pid=0x01, instr_code=0x14)
        return 99 if read_pkg[4] == 99 else unpack('>I', read_pkg[5])[0]

    def get_available_location(self, index_page=0):
        """
        Provides next available location in fingerprint library
        parameters: (int) index_page
        Returns: (int) next available location
        """
        return min(set(range(200)).difference(self.read_index_table(index_page)), default=None)

    def write_notepad(self, page_no, content):
        """
        Write data to the specific flash pages: 0 to 15, each page contains 32bytes of data, any data type is given to
        the content will be converted to the string data type before writing to the notepad.
        parameters:
            page_no: (int) 1 - 15, page number
            content: (any) data to write to the flash
        returns: (int) status code => 0 - success, 1 - error when receiving pkg, 18 - error when write flash,
        """
        content = str(content)
        len_content = len(content)
        if len_content > 32 or page_no > 0x0F or page_no < 0:
            return 101
        pkg = pack('>B32s', page_no, content.encode())
        recv_data = self.ser_send(pid=0x01, pkg_len=0x24, instr_code=0x18, pkg=pkg)
        return recv_data[4]

    def read_notepad(self, page_no):
        """
        Read data from a specific notepad page in module memory.
        Parameters:
            page_no (int): The page number to read, 0-15
        Returns:
            status (int): Status code, -1 if invalid page
            data (bytearray): Data read from the page
        """
        if page_no > 0x0F or page_no < 0:
            return -1
        recv_data = self.ser_send(pid=0x01, pkg_len=0x04, instr_code=0x19, pkg=pack('>B', page_no))
        return recv_data[4], recv_data[5]

    def ser_send(self, pkg_len, instr_code, pid=pid_cmd, pkg=None, timeout=1):
        """
        Send a command packet to the R503 module and receive response.
        Parameters:
          pkg_len (int): Length of the payload
          instr_code (int): Instruction code
          pid (int): Packet ID, default is command packet ID
          pkg (bytes): Payload data
          timeout (int): Serial timeout in seconds
        Returns:
          result (list): Parsed response packet:
            [header, address, pid, pkg_len, conf_code, payload, checksum]
            If no response, an error list is returned.
        """
        send_values = pack('>BHB', pid, pkg_len, instr_code)
        if pkg is not None:
            send_values += pkg
        check_sum = sum(send_values)
        send_values = self.header + self.addr + send_values + pack('>H', check_sum)
        # ~ print ("### ser_send: ", to_hex (send_values))
        try:
          self.ser.timeout = timeout
          self.ser.write(send_values)
          read_val = self.ser.read (9)
        except serial.serialutil.SerialException:
          read_val = b''
        if len (read_val) != 9:
          return [0, 0, 0, 0, 99, None, 0]
        read_val += self.ser.read (unpack ('>H', read_val[7:9]) [0])
        sum_calculated = sum (read_val[6:-2])
        sum_received = unpack ('>H', read_val[-2:])[0]
        # ~ print ("### ... received: ", to_hex (read_val))
        if sum_calculated != sum_received:
          return [0, 0, 0, 0, 99, None, 0]
          # no dedicated error code for checksums, since in many places in this file,
          # only 99 is treated as an error code
        return self.read_msg(read_val)


if __name__ == '__main__':
    fp = R503(port=5)

    fp.read_sys_para_decode()

    fp.ser_close()
