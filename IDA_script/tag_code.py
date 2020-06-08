import idautils
import idaapi

def save_functions():
    print("Writing all functions to file.")
    fn = ida_nalt.get_root_filename() + ".fun"
    fp = open(fn, "w")
    for f in idautils.Functions():
        addr = idaapi.get_func(f).start_ea
        fp.write("%08x\n" % (addr))
    fp.close()


def tag_code():
    seg = idaapi.getnseg(0)
    min = seg.start_ea
    max = seg.end_ea
    if min > 0:
        min = min - 1
    curaddr = ida_search.find_unknown(min, idc.SEARCH_DOWN)
    while curaddr < max:
        idc.create_insn(curaddr)
        line_asm = idc.generate_disasm_line(curaddr, 0)
## If you need to bypass the padding instructions, use this:
#        if "se_or" in line_asm:
#            curaddr = curaddr + 2
        if ida_funcs.add_func(curaddr) != True:
            idc.create_insn(curaddr)
        curaddr = ida_search.find_unknown(curaddr, idc.SEARCH_DOWN)
    print("Waiting for auto-analysis to finish")
    ida_auto.auto_wait()
    save_functions()
    print("Done.")

def tag_functions():
    seg = idaapi.getnseg(0)
    min = seg.start_ea
    max = seg.end_ea
    if min > 0:
        min = min - 1
    curaddr = ida_search.find_not_func(min, SEARCH_DOWN)
    while curaddr < max:
        idc.create_insn(curaddr)
        line_asm = idc.generate_disasm_line(curaddr, 0)
## If you need to bypass the padding instructions, use this:
#        if "se_or" in line_asm:
#            curaddr = curaddr + 2
        if ida_funcs.add_func(curaddr) != True:
            idc.create_insn(curaddr)
        curaddr = ida_search.find_not_func(curaddr, SEARCH_DOWN)
