
  PCI Express DIY hacking toolkit

*****************************************************************

  Contents
------------

  * s6_pcie_microblaze.xise -- Xilinx ISE project file. 

  * microblaze/pcores/axis_pcie_v1_00_a -- Custom peripheral module which allows to connect PCI Express integrated block of Spartan-6 FPGA as raw TLP stream to MicroBlaze soft processor core. 

  * sdk/srec_bootloader_0 -- Simple bootloader for MicroBlaze soft processor, it using SREC image format and onboard linear flash memory of SP605 to load and store main MicroBlaze program. 

  * sdk/main_0 -- Main program for MicroBlaze soft processor, it forwards raw TLP packets of PCI-E bus into the TCP connection using onboard Ethernet port of SP605 and lwIP network stack. 

  * python/pcie_lib.py -- Python library that talks to main MicroBlaze program running on SP605 over the network, for more information see pcie_lib.LinkLayer and pcie_lib.TransactionLayer classes.

  * python/pcie_mem.py -- Command line program that dumps host RAM into the screen or output file by sending MRd TLPs.

  * python/pcie_mem_scan.py -- Command line program that scans target host for physical memory ranges accessible over PCI-E bus, it's useful for security audit of IOMMU enabled platforms.

  * s6_pcie_microblaze.mcs -- FPGA bitstream image which can be written into the onboard SPI flash of SP605 over JTAG using Xilinx iMPACT (see this tutorial for more information: https://www.xilinx.com/support/documentation/boards_and_kits/sp605_PCIe_Gen1_x1_pdf_xtp065_13.4.pdf).

  * s6_pcie_microblaze.bin -- FPGA bitstream image which can be written into the onboard SPI flash of SP605 using external programmer.


  SP605 board configuration
-----------------------------

Xilinx UG526 document also known as "SP605 Hardware User Guide" (https://www.xilinx.com/support/documentation/boards_and_kits/ug526.pdf) is your best friend if you want to know more details about usage and configuration of this nice board.

  1) To load bitstream from onboard SPI flash chip you need to configure SP605 by turning SW1 switches into the [1-ON, 2-OFF] position.

  2) Now you have to write FPGA bitstream into the SPI flash. Use s6_pcie_microblaze.mcs file if you want to do it over JTAG with the help of Xilinx iMPACT utility, or s6_pcie_microblaze.bin if you want to use external SPI programmer connected to J17 header of SP605 (which is the most faster and convenient way). 

  In case of flashrom compatible external SPI programmer you can use flash_to_spi.py program as flashrom wrapper:

    # ./flash_to_spi.py s6_pcie_microblaze.bin
    Using region: "main".
    Calibrating delay loop... OK.
    Found Winbond flash chip "W25Q64.V" (8192 kB, SPI) on linux_spi.
    Reading old flash chip contents... done.
    Erasing and writing flash chip...
    Warning: Chip content is identical to the requested image.
    Erase/write done.  

  3) Bitstream which was written into the SPI flash in previous step includes a bootloader for MicroBlaze core (see sdk/srec_bootloader/src/srec_bootloader.c for more details). This bootloader allows to configure board options and write main program into the linear flash.

  To boot MicroBlaze into the update mode you have to disconnect SPI programmer and power up the board holding SW4 pushbutton switch, release SW4 when DS6 LED indicating active update mode turns on. 

  4) To write main program (see sdk/main_0/src/main.c for more details) into the linear flash connect your computer to UART bridge USB port of SP605 and run bootloaderctl.py program with --flash option:

    # easy_install pyserial
    # ./python/bootloaderctl.py /dev/ttyUSB0 --flash sdk/main_0/Debug/main_0.srec
    [+] Opening device "/dev/ttyUSB0"...
    [+] Flasing 339852 bytes from "sdk/main_0/Debug/main_0.srec"...
    Erasing flash...
    Writing 0x100 bytes at 0x00100000
    Writing 0x100 bytes at 0x00100100

    ...

    Writing 0x100 bytes at 0x00152e00
    Writing 0x8c bytes at 0x00152f00
    [+] DONE

  5) To configre board network settings run bootloaderctl.py program with --config option:

    # ./python/bootloaderctl.py /dev/ttyUSB0 --config 192.168.2.247:255.255.255.0:192.168.2.1:28472
    [+] Opening device "/dev/ttyUSB0"...
    [+] Updating board settings...

     Address: 192.168.2.247
     Netmask: 255.255.255.0
     Gateway: 192.168.2.1
        Port: 28472

    Erasing flash...
    Writing 0x12 bytes at 0x00000000
    [+] DONE

  6) Now you can exit from the update mode and boot main MicroBlaze program from linear flash:

    # ./python/bootloaderctl.py /dev/ttyUSB0 --boot
    [+] Opening device "/dev/ttyUSB0"...
    [+] Exitting from update mode...

    SREC Bootloader
    Loading SREC image from flash at address: 42000000
    Executing program starting at address: 00000000
    Loading settings from flash...
    [+] Address: 192.168.2.247
    [+] Netmask: 255.255.255.0
    [+] Gateway: 192.168.2.1
    auto-negotiated link speed: 100
    start_application(): TCP server is started at port 28472

  Main program prints it's error messages into the onboard UART, you can use --console option of bootloaderctl.py to monitor those messages in real time. 

  4) Connect SP605 to the PCI-E slot of target computer and turn the computer on. When PCI-E link was sucessfully established SP605 will fire DS3 and DS4 LEDs.

  5) Run lspci command on target computer to ensure that it seeing your board as PCI-E device:

     # lspci | grep Xilinx
     01:00.0 Ethernet controller: Xilinx Corporation Default PCIe endpoint ID

JTAG related notes: SP605 has onboard USB to JTAG interface compatible with iMPACT and others Xilinx tools. However, it's not very good so if you're planning to use onboard JTAG to program SPI flash like it was described in Xilinx tutorial you have to do the following things:

  * Remove any hardware connected to the FMC slot of SP605 while working with JTAG. 

  * In Xilinx iMPACT settings configure JTAG interface to use 750 KHz speed (on more higher speed it works unstable).


  Software configuration
--------------------------

Python tools to interact with the board and tiny implementation of PCI-E link layer are located in python folder. Because main microblaze program uses TCP to transfer TLP packets no any drivers or 3rd party dependencies needed, you can use provided Python code on any operating system. 

To set up target board IP address and port edit PCIE_TO_TCP_ADDR variable in python/pcie_lib.py file.


  Examples
------------

Information about PCI-E device implemented by provided FPGA bitstream (just like it seeing by target computer):

  # lspci -vvs 01:00.0
  01:00.0 Ethernet controller: Xilinx Corporation Default PCIe endpoint ID
      Subsystem: Xilinx Corporation Default PCIe endpoint ID
      Control: I/O- Mem- BusMaster- SpecCycle- MemWINV- VGASnoop- ParErr- Stepping- SERR- FastB2B- DisINTx-
      Status: Cap+ 66MHz- UDF- FastB2B- ParErr- DEVSEL=fast >TAbort- <TAbort- <MAbort- >SERR- <PERR- INTx-
      Interrupt: pin A routed to IRQ 11
      Region 0: Memory at f7d00000 (32-bit, non-prefetchable) [disabled] [size=1M]
      Capabilities: [40] Power Management version 3
          Flags: PMEClk- DSI- D1+ D2+ AuxCurrent=0mA PME(D0+,D1+,D2+,D3hot+,D3cold-)
          Status: D0 NoSoftRst+ PME-Enable- DSel=0 DScale=0 PME-
      Capabilities: [48] MSI: Enable- Count=1/1 Maskable- 64bit+
          Address: 0000000000000000  Data: 0000
      Capabilities: [58] Express (v1) Endpoint, MSI 00
          DevCap: MaxPayload 512 bytes, PhantFunc 0, Latency L0s unlimited, L1 unlimited
              ExtTag- AttnBtn- AttnInd- PwrInd- RBE+ FLReset-
          DevCtl: Report errors: Correctable- Non-Fatal- Fatal- Unsupported-
              RlxdOrd- ExtTag- PhantFunc- AuxPwr- NoSnoop+
              MaxPayload 128 bytes, MaxReadReq 512 bytes
          DevSta: CorrErr+ UncorrErr- FatalErr+ UnsuppReq- AuxPwr- TransPend-
          LnkCap: Port #0, Speed 2.5GT/s, Width x1, ASPM L0s, Latency L0 unlimited, L1 unlimited
              ClockPM- Surprise- LLActRep- BwNot-
          LnkCtl: ASPM Disabled; RCB 64 bytes Disabled- Retrain- CommClk-
              ExtSynch- ClockPM- AutWidDis- BWInt- AutBWInt-
          LnkSta: Speed 2.5GT/s, Width x1, TrErr- Train- SlotClk- DLActive- BWMgmt- ABWMgmt-
      Capabilities: [100 v1] Device Serial Number 00-00-00-01-01-00-0a-35



Dumping target computer physical memory at zero address from control computer:

  $ sudo DEBUG_TLP=1 ./python/pcie_mem.py 0x0 0x80
  TLP TX: size = 0x04, source = 01:00.0, type = MRd64
          tag = 0x00, bytes = 0x84, addr = 0x00000000

          0x20000021 0x010000ff 0x00000000 0x00000000

  TLP RX: size = 0x23, source = 00:00.0, type = CplD
          tag = 0x00, bytes = 132, req = 01:00.0, comp = 00:00.0

          0x4a000020 0x00000084 0x01000000
          0xf3ee00f0 0xf3ee00f0 0xc3e200f0 0xf3ee00f0 
          0xf3ee00f0 0x54ff00f0 0x053100f0 0xfe3000f0 
          0xa5fe00f0 0xe40400e8 0xf3ee00f0 0xf3ee00f0 
          0xf3ee00f0 0xf3ee00f0 0x57ef00f0 0x53ff00f0 
          0x140000c0 0x4df800f0 0x41f800f0 0x59ec00f0 
          0x39e700f0 0xd40600e8 0x2ee800f0 0xd2ef00f0 
          0x00e000f0 0xf2e600f0 0x6efe00f0 0x53ff00f0 
          0x53ff00f0 0xa4f000f0 0xc7ef00f0 0xb19900c0

  TLP RX: size = 0x04, source = 00:00.0, type = CplD
          tag = 0x00, bytes = 4, req = 01:00.0, comp = 00:00.0

          0x4a000001 0x00000004 0x01000000
          0xf3ee00f0

  00000000: f3 ee 00 f0 f3 ee 00 f0 c3 e2 00 f0 f3 ee 00 f0 | ................
  00000010: f3 ee 00 f0 54 ff 00 f0 05 31 00 f0 fe 30 00 f0 | ....T....1...0..
  00000020: a5 fe 00 f0 e4 04 00 e8 f3 ee 00 f0 f3 ee 00 f0 | ................
  00000030: f3 ee 00 f0 f3 ee 00 f0 57 ef 00 f0 53 ff 00 f0 | ........W...S...
  00000040: 14 00 00 c0 4d f8 00 f0 41 f8 00 f0 59 ec 00 f0 | ....M...A...Y...
  00000050: 39 e7 00 f0 d4 06 00 e8 2e e8 00 f0 d2 ef 00 f0 | 9...............
  00000060: 00 e0 00 f0 f2 e6 00 f0 6e fe 00 f0 53 ff 00 f0 | ........n...S...
  00000070: 53 ff 00 f0 a4 f0 00 f0 c7 ef 00 f0 b1 99 00 c0 | S...............


  Building project from the source code
-----------------------------------------

  1) Install Xilinx ISE 13.4 which comes with your SP605 board and open s6_pcie_microblaze.xise project file.

  2) Regenerate s6_pcie_v2_4 and fifo_generator_v8_4 cores which presents in project hierarchy. 

  3) Click on microblaze_i instance in project hierarchy and run "Export Hardware Design to SDK" with Bitstream.

  4) When build will be completed ISE opens Xilinx Software Development Kit IDE, use sdk forlder as it's workspace. 

  5) Create new standalone board support package in your Xilinx SDK project tree, choose lwIP and xilflash libraries in BSP configuration. 

  6) Import sdk/srec_bootloader_0 and sdk/main_0 projects into the project tree and run the build.

  7) Run make_out.sh (or make_out.bat) shell script from Xilinx ISE command prompt to generate needed output files. 


*****************************************************************

Developed by:
Dmytro Oleksiuk (aka Cr4sh)

cr4sh0@gmail.com
http://blog.cr4.sh
