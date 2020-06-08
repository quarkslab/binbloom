import ida_ua
import ida_bytes
import ida_funcs
import ida_kernwin
from ida_idaapi import BADADDR

def read_addrs(file):
    lines = []
    with open(file, "r") as f:
        lines = f.readlines()
    for l in lines:
        yield int(l.strip(), 16)

pto_file = ida_kernwin.ask_file(0, "*.fad", "Choose a function address file")
for addr in read_addrs(pto_file):
    ida_ua.create_insn(addr)
    ida_funcs.add_func(addr, BADADDR)

ptr_file = ida_kernwin.ask_file(0, "*.fpt", "Choose a function pointer file")
for addr in read_addrs(ptr_file):
    ida_bytes.create_dword(addr, 4)
