EESchema Schematic File Version 4
LIBS:hubcard-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Asterix Hub Card"
Date "2019-07-14"
Rev ""
Comp "Gundolf Kiefer"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L MCU_Microchip_ATtiny:ATtiny85V-10PU U1
U 1 1 5D2A7E84
P 3550 3500
F 0 "U1" H 3020 3546 50  0000 R CNN
F 1 "ATtiny85V-10PU" H 3020 3455 50  0000 R CNN
F 2 "Package_DIP:DIP-8_W7.62mm_Socket" H 3550 3500 50  0001 C CIN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf" H 3550 3500 50  0001 C CNN
	1    3550 3500
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0101
U 1 1 5D2A7F30
P 3550 4100
F 0 "#PWR0101" H 3550 3850 50  0001 C CNN
F 1 "GND" H 3555 3927 50  0000 C CNN
F 2 "" H 3550 4100 50  0001 C CNN
F 3 "" H 3550 4100 50  0001 C CNN
	1    3550 4100
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0102
U 1 1 5D2A7F5C
P 3550 2300
F 0 "#PWR0102" H 3550 2150 50  0001 C CNN
F 1 "+5V" H 3565 2473 50  0000 C CNN
F 2 "" H 3550 2300 50  0001 C CNN
F 3 "" H 3550 2300 50  0001 C CNN
	1    3550 2300
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x03_Male J3
U 1 1 5D2A802B
P 2600 5600
F 0 "J3" H 2706 5878 50  0000 C CNN
F 1 "Conn_01x03_Male" H 2706 5787 50  0000 C CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" H 2600 5600 50  0001 C CNN
F 3 "~" H 2600 5600 50  0001 C CNN
	1    2600 5600
	0    1    1    0   
$EndComp
Wire Wire Line
	2100 6500 2200 6500
Wire Wire Line
	2600 6500 2500 6500
$Comp
L power:+12V #PWR0103
U 1 1 5D2B0982
P 2100 6350
F 0 "#PWR0103" H 2100 6200 50  0001 C CNN
F 1 "+12V" H 2115 6523 50  0000 C CNN
F 2 "" H 2100 6350 50  0001 C CNN
F 3 "" H 2100 6350 50  0001 C CNN
	1    2100 6350
	1    0    0    -1  
$EndComp
Wire Wire Line
	2300 6450 2300 6500
$Comp
L power:GND #PWR0104
U 1 1 5D2B09B4
P 2300 6450
F 0 "#PWR0104" H 2300 6200 50  0001 C CNN
F 1 "GND" H 2305 6277 50  0000 C CNN
F 2 "" H 2300 6450 50  0001 C CNN
F 3 "" H 2300 6450 50  0001 C CNN
	1    2300 6450
	-1   0    0    1   
$EndComp
Wire Wire Line
	2400 6500 2300 6500
Connection ~ 2300 6500
Connection ~ 2100 6500
Connection ~ 2500 6500
$Comp
L Connector_Generic:Conn_02x10_Odd_Even J1
U 1 1 5D2A82FD
P 2500 6800
F 0 "J1" V 2596 6212 50  0000 R CNN
F 1 "Conn_02x10_Odd_Even" V 2505 6212 50  0000 R CNN
F 2 "kicad-library:PinHeader_2x10_P2.54mm_Horizontal_Pins_Reversed" H 2500 6800 50  0001 C CNN
F 3 "~" H 2500 6800 50  0001 C CNN
	1    2500 6800
	0    -1   -1   0   
$EndComp
Wire Wire Line
	2100 6350 2100 6500
$Comp
L power:+5V #PWR0105
U 1 1 5D2B1CBD
P 2600 6000
F 0 "#PWR0105" H 2600 5850 50  0001 C CNN
F 1 "+5V" H 2615 6173 50  0000 C CNN
F 2 "" H 2600 6000 50  0001 C CNN
F 3 "" H 2600 6000 50  0001 C CNN
	1    2600 6000
	-1   0    0    1   
$EndComp
Wire Wire Line
	2500 5800 2500 6500
Wire Wire Line
	2700 5800 2700 6500
Wire Wire Line
	2600 5800 2600 6000
NoConn ~ 2800 6500
Wire Wire Line
	2900 6500 2900 6250
Wire Wire Line
	3000 6500 3000 6250
Text Label 2900 6400 1    50   ~ 0
SCL
Text Label 3000 6400 1    50   ~ 0
SDA
$Comp
L Connector_Generic:Conn_02x04_Odd_Even J2
U 1 1 5D2B2696
P 4250 6800
F 0 "J2" V 4346 6512 50  0000 R CNN
F 1 "Conn_02x04_Odd_Even" V 4255 6512 50  0000 R CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Horizontal" H 4250 6800 50  0001 C CNN
F 3 "~" H 4250 6800 50  0001 C CNN
	1    4250 6800
	0    -1   -1   0   
$EndComp
NoConn ~ 4150 6500
NoConn ~ 4250 6500
NoConn ~ 4350 6500
NoConn ~ 4450 6500
NoConn ~ 4450 7000
NoConn ~ 4350 7000
NoConn ~ 4250 7000
NoConn ~ 4150 7000
Wire Wire Line
	4150 3200 4400 3200
Wire Wire Line
	4150 3400 4400 3400
Text Label 4250 3200 0    50   ~ 0
SDA
Text Label 4250 3400 0    50   ~ 0
SCL
$Comp
L Device:C C1
U 1 1 5D2B344D
P 3400 2600
F 0 "C1" V 3148 2600 50  0000 R CNN
F 1 "100nF" V 3239 2600 50  0000 R CNN
F 2 "Capacitor_THT:C_Disc_D4.3mm_W1.9mm_P5.00mm" H 3438 2450 50  0001 C CNN
F 3 "~" H 3400 2600 50  0001 C CNN
	1    3400 2600
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR0107
U 1 1 5D2B357F
P 3250 2600
F 0 "#PWR0107" H 3250 2350 50  0001 C CNN
F 1 "GND" H 3255 2427 50  0000 C CNN
F 2 "" H 3250 2600 50  0001 C CNN
F 3 "" H 3250 2600 50  0001 C CNN
	1    3250 2600
	1    0    0    -1  
$EndComp
Wire Wire Line
	3550 2600 3550 2900
Wire Wire Line
	3550 2300 3550 2600
Connection ~ 3550 2600
$Comp
L Transistor_FET:IRF4905 Q1
U 1 1 5D2B3C5F
P 4950 2700
F 0 "Q1" H 5156 2746 50  0000 L CNN
F 1 "IRF5305" H 5156 2655 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-220-3_Vertical" H 5150 2625 50  0001 L CIN
F 3 "https://www.infineon.com/cms/de/product/power/mosfet/20v-250v-p-channel-power-mosfet/irf5305/" H 4950 2700 50  0001 L CNN
	1    4950 2700
	1    0    0    1   
$EndComp
$Comp
L power:+5V #PWR0108
U 1 1 5D2B3ED0
P 5050 2400
F 0 "#PWR0108" H 5050 2250 50  0001 C CNN
F 1 "+5V" H 5065 2573 50  0000 C CNN
F 2 "" H 5050 2400 50  0001 C CNN
F 3 "" H 5050 2400 50  0001 C CNN
	1    5050 2400
	1    0    0    -1  
$EndComp
$Comp
L Device:R R1
U 1 1 5D2B3F19
P 4750 2550
F 0 "R1" V 4543 2550 50  0000 C CNN
F 1 "47k" V 4634 2550 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 4680 2550 50  0001 C CNN
F 3 "~" H 4750 2550 50  0001 C CNN
	1    4750 2550
	1    0    0    -1  
$EndComp
Wire Wire Line
	4750 2400 5050 2400
Wire Wire Line
	5050 2400 5050 2500
Connection ~ 5050 2400
Wire Wire Line
	4750 2700 4750 3300
Wire Wire Line
	4750 3300 4150 3300
Connection ~ 4750 2700
$Comp
L Connector:Conn_01x04_Male J4
U 1 1 5D2B47CB
P 6000 2200
F 0 "J4" V 6060 2340 50  0000 L CNN
F 1 "Conn_01x04_Male" V 6151 2340 50  0000 L CNN
F 2 "kicad-library:PinHeader_1x04_Home2lBus" H 6000 2200 50  0001 C CNN
F 3 "~" H 6000 2200 50  0001 C CNN
	1    6000 2200
	0    1    1    0   
$EndComp
Wire Wire Line
	5050 2900 5900 2900
Wire Wire Line
	5900 2900 5900 2400
$Comp
L power:GND #PWR0109
U 1 1 5D2B4A21
P 6100 2400
F 0 "#PWR0109" H 6100 2150 50  0001 C CNN
F 1 "GND" H 6105 2227 50  0000 C CNN
F 2 "" H 6100 2400 50  0001 C CNN
F 3 "" H 6100 2400 50  0001 C CNN
	1    6100 2400
	1    0    0    -1  
$EndComp
$Comp
L Device:R R2
U 1 1 5D2B4FF4
P 4950 3500
F 0 "R2" V 4743 3500 50  0000 C CNN
F 1 "100" V 4834 3500 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 4880 3500 50  0001 C CNN
F 3 "~" H 4950 3500 50  0001 C CNN
	1    4950 3500
	0    1    1    0   
$EndComp
$Comp
L Device:R R3
U 1 1 5D2B50A9
P 5250 3600
F 0 "R3" V 5457 3600 50  0000 C CNN
F 1 "100" V 5366 3600 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 5180 3600 50  0001 C CNN
F 3 "~" H 5250 3600 50  0001 C CNN
	1    5250 3600
	0    1    1    0   
$EndComp
Text Label 4250 3300 0    50   ~ 0
gpio0
Text Label 4250 3500 0    50   ~ 0
twi_ma_scl
Text Label 4250 3600 0    50   ~ 0
twi_ma_sda
$Comp
L Device:R R4
U 1 1 5D2B57DB
P 6400 3350
F 0 "R4" H 6330 3304 50  0000 R CNN
F 1 "1k" H 6330 3395 50  0000 R CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 6330 3350 50  0001 C CNN
F 3 "~" H 6400 3350 50  0001 C CNN
	1    6400 3350
	1    0    0    -1  
$EndComp
$Comp
L Device:R R5
U 1 1 5D2B589C
P 6600 3350
F 0 "R5" H 6670 3396 50  0000 L CNN
F 1 "1k" H 6670 3305 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal" V 6530 3350 50  0001 C CNN
F 3 "~" H 6600 3350 50  0001 C CNN
	1    6600 3350
	1    0    0    -1  
$EndComp
Wire Wire Line
	5100 3500 6000 3500
Connection ~ 6000 3500
Wire Wire Line
	6000 3500 6400 3500
Wire Wire Line
	4150 3600 5100 3600
Wire Wire Line
	5400 3600 5800 3600
Wire Wire Line
	6600 3600 6600 3500
Wire Wire Line
	6000 3500 6000 2400
Wire Wire Line
	4150 3500 4800 3500
Wire Wire Line
	5800 2400 5800 3600
Connection ~ 5800 3600
Wire Wire Line
	5800 3600 6600 3600
Text Notes 5800 2150 1    50   ~ 0
SDA
Text Notes 5900 2150 1    50   ~ 0
+5V\n
Text Notes 6000 2150 1    50   ~ 0
SCL\n
Text Notes 6100 2150 1    50   ~ 0
GND
Wire Wire Line
	2100 7000 2100 6500
Wire Wire Line
	2200 7000 2200 6500
Connection ~ 2200 6500
Wire Wire Line
	2300 7000 2300 6500
Wire Wire Line
	2400 7000 2400 6500
Connection ~ 2400 6500
Wire Wire Line
	2500 7000 2500 6500
Wire Wire Line
	2600 7000 2600 6500
Connection ~ 2600 6500
Wire Wire Line
	2700 7000 2700 6500
Connection ~ 2700 6500
Wire Wire Line
	2800 7000 2800 6500
Wire Wire Line
	2900 7000 2900 6500
Connection ~ 2900 6500
Wire Wire Line
	3000 7000 3000 6500
Connection ~ 3000 6500
NoConn ~ 4150 3700
Wire Wire Line
	6400 3050 6400 3200
Wire Wire Line
	6600 3050 6600 3200
Wire Wire Line
	6400 3050 6500 3050
Wire Wire Line
	5900 2900 6500 2900
Wire Wire Line
	6500 2900 6500 3050
Connection ~ 5900 2900
Connection ~ 6500 3050
Wire Wire Line
	6500 3050 6600 3050
$EndSCHEMATC
