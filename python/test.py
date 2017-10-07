import sys, os

from pcie_lib import *

# open PCI-E device
dev = TransactionLayer()

# write bytes to memory
dev.mem_write(0x1000, '\xAA' * 0x10)

# write single qword/dword/word/byte to memory
dev.mem_write_8(0x1000, 0)
dev.mem_write_4(0x1000, 0)
dev.mem_write_2(0x1000, 0)
dev.mem_write_1(0x1000, 0)

# read bytes from memory
print(repr(dev.mem_read(0x1000, 0x10)))

# read single qword/dword/word/byte from memory
print('%.16x' % dev.mem_read_8(0x1000))
print('%.8x' % dev.mem_read_4(0x1000))
print('%.4x' % dev.mem_read_2(0x1000))
print('%.2x' % dev.mem_read_1(0x1000))

# get bus:device.function address of our PCI-E endpoint
bus_id = dev.get_bus_id()

#
# MRd TLP request which reads 1 dword of memory at address 0x1000
#
tlp_tx = [ 0x20000001,                   # TLP type and data size
           0x000000ff | (bus_id << 16),  # requester ID
           0x00000000,                   # high dword of physical memory address
           0x00001000 ]                  # low dword of physical memory address
         
# send TLP
dev.write(tlp_tx)

# receive root complex reply
tlp_rx = dev.read(raw = True)

# prints 4a000001 00000004 01000000 00000000
print('%.8x %.8x %.8x %.8x' % tuple(tlp_rx))

# check for CplD TLP format and type
assert (tlp_rx[0] >> 24) & 0xff == 0x4a

# print readed dword
print('%.8x' % tlp_rx[3])


# MRd TLP request which reads 1 dword of memory at address 0x1000
tlp_tx = dev.PacketMRd64(dev.bus_id, 0x1000, 4)

# send TLP
dev.write(tlp_tx)

# receive root complex reply
tlp_rx = dev.read()

# check for CplD TLP
assert isinstance(tlp_rx, dev.PacketCplD)

# print readed dword
print('%.8x' % tlp_rx.data[0])