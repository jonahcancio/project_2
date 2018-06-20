import os
import re
import logging
import subprocess
from collections import namedtuple, defaultdict

import serial
import serial.tools.list_ports as listports

TITLE = "CS 21 17.2 - Project 2 Server v1.1"
MARS_PATH = "mars.jar"
DEFAULT_ASM_FILENAME = "test.asm"

DEFAULT_PORT_CHOICE = 1
BAUDRATE = 9600
LOGGING_LEVEL = logging.DEBUG
MAX_ITERATIONS = 1e9

class State(object):
    IFUOutput = namedtuple('IFUnitOutput', ('inst'))
    RFOutput = namedtuple('RegFileOutput', ('readdata1', 'readdata2'))
    DMOutput = namedtuple('DataMemOutput', ('readdata'))
    StateRepr = namedtuple('StateString', ('pc', 'regfile', 'mem'))

    TEXTSEG_FILENAME = "textsegment.dump"
    DATASEG_FILENAME = "datasegment.dump"
    DEFAULT_PC = 0x00400000
    DEFAULT_DATASEG = 0x10010000

    def __init__(self, zeropc=False):
        self.pc = 0 if zeropc else State.DEFAULT_PC
        self.regfile = defaultdict(int)
        self.mem = defaultdict(int)

    def load_asm(self, filename, marspath):
        self._populate_text_segment(filename, marspath)
        self._populate_data_segment(filename, marspath)

    def _populate_text_segment(self, filename, marspath):
        self._mars_dump(marspath, filename, State.TEXTSEG_FILENAME, ".text")
        data = self._get_bytes_from_dump(State.TEXTSEG_FILENAME)
        self._populate_memory(self.pc, data)

    def _populate_data_segment(self, filename, marspath):
        self._mars_dump(marspath, filename, State.DATASEG_FILENAME, ".data")
        data = self._get_bytes_from_dump(State.DATASEG_FILENAME)
        self._populate_memory(State.DEFAULT_DATASEG, data)

    def _get_bytes_from_dump(self, filepath):
        ret = []

        with open(filepath, "r") as f:
            for line in f:
                line = line.strip()

                for b in [int(line[i:i+2].zfill(2), 16) for i in range(0, len(line), 2)]:
                    ret.append(b)

        return ret

    def _populate_memory(self, startaddr, data):
        logger.info("Populating memory starting address %s (%s)" % (startaddr, hex(startaddr)))
        for i in range(len(data)):
            # Optimization: defaultdict writes zero anyway
            if data[i] != 0:
                self.mem[startaddr + i] = data[i]

    def _mars_dump(self, marspath, inputfile, outputfile, segment):
        command = "java -Dapple.awt.UIElement=true -jar {marspath} nc np sm {zero} dump {segment} HexText {outputfile} {inputfile}".format(
            marspath=marspath,
            inputfile=inputfile,
            outputfile=outputfile,
            segment=segment,
            zero="mc CompactTextAtZero" if self.pc == 0 else "",
        )

        logging.info("Running %s" % command)

        subprocess.run(command.split())

    def input_if_unit(self, pc):
        pc = self._hex_to_int(pc)
        self.pc = pc

        inst = 0
        for i in range(4):
            inst <<= 8
            inst |= self._read_mem(pc + i)

        return State.IFUOutput(inst=inst)

    def input_register_file(self, readreg1, readreg2, writereg, writedata, write):
        logger.debug("Register file inputs: RR1({rr1}), RR2({rr2}), WR({wr}), WD({wd}), W({w})".format(
            rr1=readreg1,
            rr2=readreg2,
            wr=writereg,
            wd=writedata,
            w=write,
        ))

        if self._bin_to_int(write):
            self._write_regfile(writereg, writedata)

        rd1 = self._read_regfile(readreg1)
        rd2 = self._read_regfile(readreg2)

        return State.RFOutput(readdata1=rd1, readdata2=rd2)

    def input_data_memory(self, addr, writedata, memread, memwrite):
        logger.debug("Data memory inputs: Addr({addr}), WD({wd}), MR({mr}), MW({mw})".format(
            addr=addr,
            wd=writedata,
            mr=memread,
            mw=memwrite,
        ))

        if self._bin_to_int(memwrite):
            self._write_mem(addr, writedata)

        # MemRead is ignored
        data = self._read_mem(addr)

        return State.DMOutput(readdata=data)
    
    def _hex_to_int(self, s):
        if type(s) == int:
            return s

        if s.startswith("0x"):
            s = s.replace("0x", "")

        return int(s, 16)

    def _bin_to_int(self, s):
        if type(s) == int:
            return s

        if s.startswith("0b"):
            s = s.replace("0b", "")

        return int(s, 2)

    def _read_regfile(self, reg):
        nreg = self._bin_to_int(reg)
        logger.debug("Reading register: %s (%s)" % (reg, nreg))

        return self.regfile[nreg]

    def _write_regfile(self, reg, data):
        nreg = self._bin_to_int(reg)
        ndata = self._hex_to_int(data)
        logger.debug("Writing to register %s: %s (%s: %s)" % (reg, data, nreg, ndata))

        if nreg == 0:
            logger.warning("Attempting to write to register zero: %s (%s)" % (data, ndata))

        self.regfile[nreg] = ndata

    def _read_mem(self, addr):
        naddr = self._hex_to_int(addr)
        logger.debug("Reading memory address: %s (%s)" % (addr, naddr))

        return self.mem[naddr]

    def _write_mem(self, addr, data):
        naddr = self._hex_to_int(addr)
        ndata = self._hex_to_int(data)
        logger.debug("Writing to memory address %s: %s (%s: %s)" % (addr, data, naddr, ndata))

        self.mem[naddr] = ndata
    
    def __str__(self):
        ss = State.StateRepr(pc=self.pc, regfile=self.regfile, mem=self.mem)

        return str(ss)

# Note: bad practice to instantiate module-level logging
logging.basicConfig(level=LOGGING_LEVEL, format='[%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)

def get_com_ports():
    # Not all COM ports have "Arduino" in description
    # port.device, port.description
    return [port for port in listports.comports()]

def select_com_port():
    com_ports = get_com_ports()

    print("COM ports:")
    for i, port in enumerate(com_ports, start=1):
        print("{num} - {path} ({desc})".format(
            num=i,
            path=port.device,
            desc=port.description,
        ))

    # No need for validation
    choice = input("Select COM port [%s]: " % DEFAULT_PORT_CHOICE)
    if not choice:
        choice = DEFAULT_PORT_CHOICE
    choice = int(choice) - 1

    return com_ports[choice]

def recv_cstring(ser):
    buf = []

    while True:
        ch = ser.read()
        logger.debug("Received character: [%s]" % ch)

        if ch != b"\0":
            buf.append(ch)
        else:
            # Returns a string (not a bytes object)
            try:
                s = b''.join(buf).decode("ascii")
            except UnicodeDecodeError as e:
                logger.error("Arduino did not restart cleanly! Exception: %s" % e)
                exit(-1)

            logger.debug("Constructed C-style string: %s" % s)

            return s

def send_asciizstr(ser, msg):
    logger.info("Sending ASCII string: %s" % msg)
    bytemsg = msg.encode("ascii")

    ser.write(bytemsg)

def is_start_message(msg):
    return msg == 'START'

def is_input_message(msg):
    # pppppppp aaaaa bbbbb ccccc dddddddd e ffffffff gggggggg h i
    # pc       rr1   rr2   wr    wdrf     w addr     wddm    mr mw

    # Lowercase only
    HEX_REGEX = "[0-9a-f]"
    BIN_REGEX = "[01]"
    HEX_8 = (8, HEX_REGEX)
    BIN_1 = (1, BIN_REGEX)
    BIN_5 = (5, BIN_REGEX)

    # Order is significant
    tokens = (
        HEX_8, # pc
        BIN_5, # rr1
        BIN_5, # rr2
        BIN_5, # wr
        HEX_8, # wdrf
        BIN_1, # w
        HEX_8, # addr
        HEX_8, # wddm
        BIN_1, # mr
        BIN_1, # mw
    )

    # Tokens are delimited by single space
    pattern = ' '.join(("%s{%s}" % (pattern, length) for length, pattern in tokens))
    logger.debug("Full-match pattern: %s" % pattern)

    # Can be optimized by compiling once for entire run; pattern is constant
    return re.fullmatch(pattern, msg)

def handle_start_message(state, ser):
    send_asciizstr(ser, "{:08x}".format(state.pc))

def handle_input_message(ser, state, msg):
    ifout, rfout, dmout = simulate_inputs(state, msg)
    send_output_response(ser, ifout, rfout, dmout)

def simulate_inputs(state, msg):
    logger.debug("Simulating input: %s" % msg)

    # Message is already formatted properly
    pc, rr1, rr2, wr, wdrf, w, addr, wddm, mr, mw = msg.split()

    ifuout = state.input_if_unit(pc)
    rfout = state.input_register_file(rr1, rr2, wr, wdrf, w)
    dmout = state.input_data_memory(addr, wddm, mr, mw)

    logger.debug("Simulation outputs: %s, %s, %s" % (ifuout, rfout, dmout))

    return ifuout, rfout, dmout

def send_output_response(ser, ifuout, rfout, dmout):
    msg = make_output_message(ifuout, rfout, dmout)
    send_asciizstr(ser, msg)

def make_output_message(ifuout, rfout, dmout):
    # iiiiiiii aaaaaaaa bbbbbbbb cccccccc
    # inst     rd1      rd2      rd
    
    msg = "{inst:08x} {rd1:08x} {rd2:08x} {rd:08x}".format(
        inst=ifuout.inst,
        rd1=rfout.readdata1,
        rd2=rfout.readdata2,
        rd=dmout.readdata,
    )
    logger.debug("Created output message: %s" % msg)

    return msg

def get_asm_filename():
    filename = input("Enter source ASM filename [%s]: " % DEFAULT_ASM_FILENAME)
    if not filename:
        filename = DEFAULT_ASM_FILENAME

    return filename

def clear_temp_files():
    for path in (State.TEXTSEG_FILENAME, State.DATASEG_FILENAME):
        if os.path.exists(path):
            logger.debug("Deleting [%s]..." % path)
            os.remove(path)

def check_filename(filename):
    if not os.path.exists(filename):
        logger.error("The file [%s] does not exist" % filename)
        exit(-1)

def check_temp_files():
    if not os.path.exists(State.DATASEG_FILENAME) or not os.path.exists(State.TEXTSEG_FILENAME):
        logger.error("Failed to generate temporary files")
        exit(-1)

def main():
    print(TITLE)
    print()

    clear_temp_files()
    check_filename(MARS_PATH)
    print("Please ensure that your Arduino is connected.")

    filename = get_asm_filename()
    check_filename(filename)

    state = State()
    state.load_asm(filename, MARS_PATH)

    check_temp_files()

    port = select_com_port()
    print("Port selected: %s" % port.device)

    try:
        ser = serial.Serial(port.device, BAUDRATE)
    except (OSError, serial.SerialException):
        logging.error("Port is being used (ensure that Serial Monitor is not running)")
        exit(-1)

    print("Serial port opened successfully.")

    # Arduino program is automatically restarted
    print("Waiting for START message...")

    mainloop(state, ser)

def mainloop(state, ser):
    iteration = 0
    cycle = 0
    started = False

    while iteration < MAX_ITERATIONS:
        iteration += 1
        logger.debug("Iteration: %s" % iteration)

        logger.debug("Old state: %s" % state)

        msg = recv_cstring(ser)
        logger.info("Received message: [%s]" % msg)

        if is_start_message(msg):
            logger.info("Recognized start init message")
            handle_start_message(state, ser)
            started = True
            cycle = 0

        elif not started:
            logger.warn("Not started; ignoring message: [%s]" % msg)

        else:
            # Has already started
            cycle += 1
            logger.info("Cycle: %s" % cycle)

            if is_input_message(msg):
                logger.info("Recognized input message")
                handle_input_message(ser, state, msg)

            else:
                logger.warn("Unrecognized message: [%s]" % msg)

            print(state)

        logger.debug("New state: %s" % state)
        print()

if __name__ == "__main__":
    main()
