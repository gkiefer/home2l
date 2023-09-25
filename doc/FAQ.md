Building the software
---------------------

* **_Checkout on case insensitive filesystem:_** The software requires a case sensitive filesystem

Creating circuits
-----------------

* **_Which circuits require 100 ohm resistors in the SDA and SCL bus connections?_**
All but ahub circuits (including bhubs).

Networking 
----------

These observations are with home2l installed on FreeBSD 13.1.
* **_Does home2l support IPv6?_** No.
* **_Does home2l work with dual stack?_** No. If there exists both a A and a AAAA Resource record for a net-host used in resources.conf, its best, to remove the AAAA.
* **_Are there issues with Name resolution?_** It's best to use IP-Addresses instead of nethost name or FQDN.

Brownies
--------

* **_How to flash a new operational software?_** The Brownie must be in the maintenance state:

    Flashing device 011 with '/usr/local/home2l/share/brownies/winl.t84.elf' ... 
      0a00 - 188c (3724 bytes) ... Error accessing device 011: Operation not allowed
    brownie2l> boot -m
    Switching device 011 to MAINTENANCE firmware (block 0x01, adr=0x0040) ... Activating and rebooting ... OK
    brownie2l> program -d oa_win_right
    
    Segments in '/usr/local/home2l/share/brownies/winl.t84.elf':
       FLASH  : 0a00 - 188c (3724 bytes)
      (SRAM)  : 0060 - 011a (186 bytes)
      (EEPROM): 0000 - 0032 (50 bytes)
    
    (Re-)program FLASH of device 011 with this? (y/N) y
    
    Flashing device 011 with '/usr/local/home2l/share/brownies/winl.t84.elf' ... 
      0a00 - 188c (3724 bytes) ... verifying ... OK     
    brownie2l> boot -o
    Switching device 011 to OPERATIONAL firmware (block 0x28, adr=0x0a00) ... Activating and rebooting ... OK


* **_How to adjust Brownie configuratuion?_** The Brownie must be in the maintenance state:

    brownie2l> config -d oa_win_right
    
    011 oa_win_right
      adr=011 id=oa_win_right fw=winl         mcu=t84  sha0_dd=0.524 sha0_td=25.04 sha0_du=0.524 sha0_tu=25.04
    
    Write back this configuration and reboot node 011? (y/N) y
    Writing config ... verifying config ... OK
    Rebooting ... OK
    brownie2l> boot -o
    Switching device 011 to OPERATIONAL firmware (block 0x28, adr=0x0a00) ... Activating and rebooting ... OK


* **_What to do after a software update?_**

    After a new version of the home2l suite has been installed, all brownies must be re-programmed and re-configuered.

    Some brownies may not need a configuration update. Be sure to check all.


* **_How to change the address of a brownie?_**

- Avoid duplicate addresses.

- While changing addresses do one brownie after another, moving from leave nodes to hubs.

- Moving a brownie out of a subnet by address change via config command works without problems.


* **_How to change the features of a brownie?_**

- Modify brownies.conf

- Restart Server

- flash a new operational software


* **_Adding GPIOs_**

- Select free pin(s) (Fig. 7.5)

  Attention: Not all unused pins are available as GPIOs, only those, marked with gpio<n> !

- Create a new Brownie variant in Family.mk and set the GPIO mask bit(s) in one or more variables like so:
    GPIO_OUT_PRESENCE=0x80 GPIO_OUT_PRESET=0x00 GPIO_IN_PRESENCE=0x8 GPIO_IN_PULLUP 

- Mask bits are 2**n from gpio<n> in Fig. 7.5

- Add the new variant to BROWNIE_FAMILY macro

- Build and install the software


* **_Displaying GPIOs_**

  Open brownie in brownie2l or do a verbose scan:
  
    013 oa_light2           gpiolight v1.2-0        (t84)
            Device:   ATtiny84 (t84)
            Features: notify
            GPIOs:    00000pp
            Config:   

  GPIOs are displayed from left to right by GPIO index (gpio<n>) with symbols:
  
-  "-" not present

-  "i" input without pullup

-  "p" input with pullup (about 20k)

-  "1" output, preset (initialized to high)

-  "0" output, not preset  (initialized to low)
 
 


Trouble getting resources 
-------------------------

* **_Why is no resource returned in Python API?_** Setting allowWait may help.

Example:

    rcNow = RcGet ("/host/h2l/timer/now")
    rcNow.PrintInfo()
    /host/h2l/timer/now[none,wr] = ?
      (no requests)
      (no subscriptions)
    
    rcNow = RcGet ("/host/h2l/timer/now", allowWait = True)
    rcNow.PrintInfo()
    /host/home2l/timer/now[time,ro] = 2023-01-25-163849 @2023-01-25-163849.060
      (no requests)
      (no subscriptions)



Writing Rules Scripts
---------------------

* **_Creating local Resources_**

- Local Resources, created via NewResource()
  can only be referenced from the Script where
  they have been declared.
  
- Local Resources have a URI like:

        /host/home2l<roles:57244>/resource/shadingSouth

  instead of

        /host/h2l/resource/shadingSouth
  
- This works:

        rcShadingSouth = NewResource ('shadingSouth', rctPercent
        . . .
        RunOnUpdate (hadingChanged,
			         (rcShadingSouth, ), . . .

- Author says: These Resources may be referenced globally if you
  make sure, server has been started by:
  
  - config option:
  
        rc.enableServer = 1


  - plus instance definition of rules script in resources.conf:
  
        H myRules     rules@<some net host>:1234
    
    In rules script:
    
        Home2lInit (instance = myRules, server = True)


* **_Creating Subscriptions_**

  - Subscriptions  must be created for all resources, which are not used
    by triggers.
  
  - Subscriptions must be declared before Home2lStart() is called.
  
    example:
    
        from home2l import *
        
        me = 'subscriptionTest'
        Home2lInit (instance=me)
        myInstance = RcGet('/host/'+me+'/resources')
        mySubscriber = RcNewSubscriber(me, myInstance)
        
        minuteTimer = RcGet ('/local/timer/minutely')
        
        all = RcGet('/host/h2l/brownies/*/*')
        all.Subscribe(mySubscriber)
        
        Home2lStart ()
        
        t1 = RcGet('/host/h2l/brownies/oa_win1/temp')
        print('Temperature is now {:2.1f}'.format(v))
        
        Home2lRun ()



* **_Using the Directoryservice_**

-  To collect a list of Resources (e.g. Brownies) at startup, you
   should wait until network connections have been established.
   A RunAt of 1 second works for me.

-  Author says: Directory Service is for information and diagnosis
   purpose only, do not depend on it with productive code.