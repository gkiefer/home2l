EESchema Schematic File Version 4
LIBS:window-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Window Dual"
Date "2019-10-06"
Rev ""
Comp "Gundolf Kiefer"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L power:GND #PWR0101
U 1 1 5D2A7F30
P 1800 4000
F 0 "#PWR0101" H 1800 3750 50  0001 C CNN
F 1 "GND" H 1805 3827 50  0000 C CNN
F 2 "" H 1800 4000 50  0001 C CNN
F 3 "" H 1800 4000 50  0001 C CNN
	1    1800 4000
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0102
U 1 1 5D2A7F5C
P 1800 1600
F 0 "#PWR0102" H 1800 1450 50  0001 C CNN
F 1 "+5V" H 1815 1773 50  0000 C CNN
F 2 "" H 1800 1600 50  0001 C CNN
F 3 "" H 1800 1600 50  0001 C CNN
	1    1800 1600
	1    0    0    -1  
$EndComp
Text Label 2450 2900 0    50   ~ 0
SCL
Text Label 2450 3100 0    50   ~ 0
SDA
$Comp
L Device:C C1
U 1 1 5D2B344D
P 1650 1900
F 0 "C1" V 1398 1900 50  0000 R CNN
F 1 "100nF" V 1489 1900 50  0000 R CNN
F 2 "Capacitor_THT:C_Disc_D4.3mm_W1.9mm_P5.00mm" H 1688 1750 50  0001 C CNN
F 3 "~" H 1650 1900 50  0001 C CNN
	1    1650 1900
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR0107
U 1 1 5D2B357F
P 1500 1900
F 0 "#PWR0107" H 1500 1650 50  0001 C CNN
F 1 "GND" H 1505 1727 50  0000 C CNN
F 2 "" H 1500 1900 50  0001 C CNN
F 3 "" H 1500 1900 50  0001 C CNN
	1    1500 1900
	1    0    0    -1  
$EndComp
Wire Wire Line
	1800 1900 1800 2200
Wire Wire Line
	1800 1600 1800 1900
Connection ~ 1800 1900
$Comp
L power:GND #PWR0109
U 1 1 5D2B4A21
P 3550 1700
F 0 "#PWR0109" H 3550 1450 50  0001 C CNN
F 1 "GND" H 3555 1527 50  0000 C CNN
F 2 "" H 3550 1700 50  0001 C CNN
F 3 "" H 3550 1700 50  0001 C CNN
	1    3550 1700
	1    0    0    -1  
$EndComp
$Comp
L Device:R R1
U 1 1 5D2B4FF4
P 3450 2100
F 0 "R1" V 3243 2100 50  0000 C CNN
F 1 "100" V 3334 2100 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm_Horizontal" V 3380 2100 50  0001 C CNN
F 3 "~" H 3450 2100 50  0001 C CNN
	1    3450 2100
	-1   0    0    1   
$EndComp
$Comp
L Device:R R2
U 1 1 5D2B50A9
P 3250 2100
F 0 "R2" V 3457 2100 50  0000 C CNN
F 1 "100" V 3366 2100 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm_Horizontal" V 3180 2100 50  0001 C CNN
F 3 "~" H 3250 2100 50  0001 C CNN
	1    3250 2100
	-1   0    0    1   
$EndComp
$Comp
L power:+5V #PWR0103
U 1 1 5D33A297
P 3350 1700
F 0 "#PWR0103" H 3350 1550 50  0001 C CNN
F 1 "+5V" H 3365 1873 50  0000 C CNN
F 2 "" H 3350 1700 50  0001 C CNN
F 3 "" H 3350 1700 50  0001 C CNN
	1    3350 1700
	-1   0    0    1   
$EndComp
$Comp
L MCU_Microchip_ATtiny:ATtiny84-20PU U1
U 1 1 5D33A5D0
P 1800 3100
F 0 "U1" H 1270 3146 50  0000 R CNN
F 1 "ATtiny84-20PU" H 1270 3055 50  0000 R CNN
F 2 "Package_DIP:DIP-14_W7.62mm_Socket" H 1800 3100 50  0001 C CIN
F 3 "http://ww1.microchip.com/downloads/en/DeviceDoc/doc8006.pdf" H 1800 3100 50  0001 C CNN
	1    1800 3100
	1    0    0    -1  
$EndComp
Wire Wire Line
	2400 2500 2700 2500
Wire Wire Line
	2400 2600 2700 2600
Wire Wire Line
	2400 2700 2700 2700
Wire Wire Line
	2400 2800 2700 2800
Wire Wire Line
	2400 3200 2700 3200
Wire Wire Line
	2400 3400 2700 3400
Wire Wire Line
	2400 3500 2700 3500
Wire Wire Line
	2400 3600 2700 3600
Text Label 2450 3400 0    50   ~ 0
shades_0_act_dn
Text Label 2450 3500 0    50   ~ 0
shades_0_act_up
Text Label 2450 3600 0    50   ~ 0
shades_1_act_up
Text Label 2450 3200 0    50   ~ 0
shades_1_act_dn
Wire Wire Line
	3250 1700 3250 1950
Wire Wire Line
	3450 1700 3450 1950
Wire Wire Line
	3450 2250 3450 2900
Wire Wire Line
	2400 2900 3450 2900
Wire Wire Line
	3250 2250 3250 3100
Wire Wire Line
	2400 3100 3250 3100
Text Label 2450 2700 0    50   ~ 0
shades_0_btn_up
Text Label 2450 2600 0    50   ~ 0
shades_0_btn_dn
Text Label 2450 3000 0    50   ~ 0
shades_1_btn_dn
Text Label 2450 2800 0    50   ~ 0
shades_1_btn_up
Text Label 2450 2500 0    50   ~ 0
temp
$Comp
L Sensor_Temperature:TSIC306-TO92 U2
U 1 1 5D33C1D8
P 4600 3600
F 0 "U2" H 4271 3646 50  0000 R CNN
F 1 "TSIC306-TO92" H 4271 3555 50  0000 R CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline_Wide" H 4250 3750 50  0001 L CNN
F 3 "https://shop.bb-sensors.com/out/media/Datasheet_Digital_Semiconductor_temperatur_sensor_TSIC.pdf" H 4600 3600 50  0001 C CNN
	1    4600 3600
	-1   0    0    -1  
$EndComp
$Comp
L power:+5V #PWR0104
U 1 1 5D33C4DF
P 4600 3300
F 0 "#PWR0104" H 4600 3150 50  0001 C CNN
F 1 "+5V" H 4615 3473 50  0000 C CNN
F 2 "" H 4600 3300 50  0001 C CNN
F 3 "" H 4600 3300 50  0001 C CNN
	1    4600 3300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0105
U 1 1 5D33C53D
P 4600 3900
F 0 "#PWR0105" H 4600 3650 50  0001 C CNN
F 1 "GND" H 4605 3727 50  0000 C CNN
F 2 "" H 4600 3900 50  0001 C CNN
F 3 "" H 4600 3900 50  0001 C CNN
	1    4600 3900
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0106
U 1 1 5D33C82D
P 4550 1700
F 0 "#PWR0106" H 4550 1450 50  0001 C CNN
F 1 "GND" H 4555 1527 50  0000 C CNN
F 2 "" H 4550 1700 50  0001 C CNN
F 3 "" H 4550 1700 50  0001 C CNN
	1    4550 1700
	1    0    0    -1  
$EndComp
Text Label 4650 2600 1    50   ~ 0
shades_0_btn_dn
Text Label 4750 2600 1    50   ~ 0
shades_0_btn_up
Wire Wire Line
	4750 1700 4750 2600
Wire Wire Line
	4650 1700 4650 2600
$Comp
L Transistor_FET:BS170 Q3
U 1 1 5D3414A8
P 6200 2100
F 0 "Q3" H 6405 2146 50  0000 L CNN
F 1 "BS170" H 6405 2055 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline_Wide" H 6400 2025 50  0001 L CIN
F 3 "http://www.fairchildsemi.com/ds/BS/BS170.pdf" H 6200 2100 50  0001 L CNN
	1    6200 2100
	1    0    0    -1  
$EndComp
$Comp
L Device:R R5
U 1 1 5D3414AF
P 6000 2250
F 0 "R5" H 6070 2296 50  0000 L CNN
F 1 "10k" H 6070 2205 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm_Horizontal" V 5930 2250 50  0001 C CNN
F 3 "~" H 6000 2250 50  0001 C CNN
	1    6000 2250
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR0111
U 1 1 5D3414B6
P 6300 2400
F 0 "#PWR0111" H 6300 2150 50  0001 C CNN
F 1 "GND" H 6305 2227 50  0000 C CNN
F 2 "" H 6300 2400 50  0001 C CNN
F 3 "" H 6300 2400 50  0001 C CNN
	1    6300 2400
	1    0    0    -1  
$EndComp
Wire Wire Line
	6300 2300 6300 2400
Wire Wire Line
	6300 2400 6000 2400
Connection ~ 6300 2400
$Comp
L Device:D D3
U 1 1 5D3414BF
P 6300 1750
F 0 "D3" V 6300 1829 50  0000 L CNN
F 1 "D" V 6345 1829 50  0001 L CNN
F 2 "Diode_THT:D_T-1_P5.08mm_Horizontal" H 6300 1750 50  0001 C CNN
F 3 "~" H 6300 1750 50  0001 C CNN
	1    6300 1750
	0    1    1    0   
$EndComp
$Comp
L power:+5V #PWR0112
U 1 1 5D3414C6
P 6300 1600
F 0 "#PWR0112" H 6300 1450 50  0001 C CNN
F 1 "+5V" H 6315 1773 50  0000 C CNN
F 2 "" H 6300 1600 50  0001 C CNN
F 3 "" H 6300 1600 50  0001 C CNN
	1    6300 1600
	1    0    0    -1  
$EndComp
Connection ~ 6300 1900
Connection ~ 6000 2100
Text Label 5300 2100 0    50   ~ 0
shades_0_act_up
Wire Wire Line
	5300 2100 6000 2100
$Comp
L Transistor_FET:BS170 Q2
U 1 1 5D34194E
P 6900 2800
F 0 "Q2" H 7105 2846 50  0000 L CNN
F 1 "BS170" H 7105 2755 50  0000 L CNN
F 2 "Package_TO_SOT_THT:TO-92_Inline_Wide" H 7100 2725 50  0001 L CIN
F 3 "http://www.fairchildsemi.com/ds/BS/BS170.pdf" H 6900 2800 50  0001 L CNN
	1    6900 2800
	1    0    0    -1  
$EndComp
$Comp
L Device:R R4
U 1 1 5D341955
P 6700 2950
F 0 "R4" H 6770 2996 50  0000 L CNN
F 1 "10k" H 6770 2905 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0204_L3.6mm_D1.6mm_P7.62mm_Horizontal" V 6630 2950 50  0001 C CNN
F 3 "~" H 6700 2950 50  0001 C CNN
	1    6700 2950
	-1   0    0    1   
$EndComp
$Comp
L power:GND #PWR0113
U 1 1 5D34195C
P 7000 3100
F 0 "#PWR0113" H 7000 2850 50  0001 C CNN
F 1 "GND" H 7005 2927 50  0000 C CNN
F 2 "" H 7000 3100 50  0001 C CNN
F 3 "" H 7000 3100 50  0001 C CNN
	1    7000 3100
	1    0    0    -1  
$EndComp
Wire Wire Line
	7000 3000 7000 3100
Wire Wire Line
	7000 3100 6700 3100
Connection ~ 7000 3100
$Comp
L Device:D D2
U 1 1 5D341965
P 7000 2450
F 0 "D2" V 7000 2529 50  0000 L CNN
F 1 "D" V 7045 2529 50  0001 L CNN
F 2 "Diode_THT:D_T-1_P5.08mm_Horizontal" H 7000 2450 50  0001 C CNN
F 3 "~" H 7000 2450 50  0001 C CNN
	1    7000 2450
	0    1    1    0   
$EndComp
$Comp
L power:+5V #PWR0114
U 1 1 5D34196C
P 7000 2300
F 0 "#PWR0114" H 7000 2150 50  0001 C CNN
F 1 "+5V" H 7015 2473 50  0000 C CNN
F 2 "" H 7000 2300 50  0001 C CNN
F 3 "" H 7000 2300 50  0001 C CNN
	1    7000 2300
	1    0    0    -1  
$EndComp
Connection ~ 7000 2600
Connection ~ 6700 2800
Text Label 6000 2800 0    50   ~ 0
shades_0_act_dn
Wire Wire Line
	6000 2800 6700 2800
$Comp
L power:+5V #PWR0117
U 1 1 5D3451F5
P 7900 1450
F 0 "#PWR0117" H 7900 1300 50  0001 C CNN
F 1 "+5V" H 7915 1623 50  0000 C CNN
F 2 "" H 7900 1450 50  0001 C CNN
F 3 "" H 7900 1450 50  0001 C CNN
	1    7900 1450
	1    0    0    -1  
$EndComp
Wire Wire Line
	6300 1900 7500 1900
Wire Wire Line
	7000 2600 7600 2600
Wire Wire Line
	2400 3000 2700 3000
Wire Wire Line
	4200 3600 3900 3600
Text Label 3900 3600 0    50   ~ 0
temp
$Comp
L home2l:Conn_Home2lBus J4
U 1 1 5D916CF2
P 3450 1500
F 0 "J4" V 3424 1660 50  0000 L CNN
F 1 "Conn_Home2lBus" V 3515 1660 50  0000 L CNN
F 2 "home2l:Conn_Home2lBus" H 3450 1500 50  0001 C CNN
F 3 "~" H 3450 1500 50  0001 C CNN
	1    3450 1500
	0    1    1    0   
$EndComp
Wire Wire Line
	7500 1550 7900 1550
Wire Wire Line
	7500 1550 7500 1900
Wire Wire Line
	7600 1650 7900 1650
Wire Wire Line
	7600 1650 7600 2600
$Comp
L home2l:Conn_Shades J2
U 1 1 5D9B8075
P 8100 1550
F 0 "J2" H 7899 1490 50  0000 R CNN
F 1 "Conn_Shades" H 7899 1581 50  0000 R CNN
F 2 "home2l:Conn_Shades" H 8100 1550 50  0001 C CNN
F 3 "~" H 8100 1550 50  0001 C CNN
	1    8100 1550
	-1   0    0    1   
$EndComp
$Comp
L Connector:Conn_01x03_Male J1
U 1 1 5D9B84B4
P 4650 1500
F 0 "J1" V 4710 1312 50  0000 R CNN
F 1 "buttons" V 4801 1312 50  0000 R CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical" H 4650 1500 50  0001 C CNN
F 3 "~" H 4650 1500 50  0001 C CNN
	1    4650 1500
	0    -1   1    0   
$EndComp
NoConn ~ 2700 2800
NoConn ~ 2700 3000
NoConn ~ 2700 3200
NoConn ~ 2700 3600
$Comp
L power:+5V #PWR0108
U 1 1 5D9D0448
P 2400 3700
F 0 "#PWR0108" H 2400 3550 50  0001 C CNN
F 1 "+5V" V 2415 3828 50  0000 L CNN
F 2 "" H 2400 3700 50  0001 C CNN
F 3 "" H 2400 3700 50  0001 C CNN
	1    2400 3700
	0    1    1    0   
$EndComp
$EndSCHEMATC
