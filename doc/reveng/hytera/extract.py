#!/usr/bin/env python3

import pyshark
import struct
import sys
import binascii

import argparse
parser = argparse.ArgumentParser()
parser.add_argument("command", help="""What to extract. Possible commands are: serial (dumps serial data in hex).
""")
parser.add_argument("cap_file", help="Path to the captured data.")
parser.add_argument("--nodump", help="Hide data dumps.", action="count", default=0)
args = parser.parse_args()


def hexDump(s, prefix="", addr=0):
  N = len(s)
  Nb = N//16
  if (N%16): Nb += 1
  res = ""
  for j in range(Nb):
    a,b = j*16, min((j+1)*16,N)

    h = " ".join(map("{:02x}".format, s[a:b]))
    h += "   "*(16-(b-a))
    t = ""
    for i in range(a,b):
      c = s[i]
      if c>=0x20 and c<0x7f:
        t += chr(c)
      else:
        t += "."

    res += (prefix + "{:08X} ".format(addr+16*j) + h + " | " + t + "\n")
  return res[:-1]

def isDataPacket(p):
  return ("USB.CAPDATA" in p)

def isFromHost(p):
  return ("host" == p.usb.src)

def isToHost(p):
  return ("host" == p.usb.dst)

def getData(p):
  if not isDataPacket(p): 
    return None
  return binascii.a2b_hex(p["USB.CAPDATA_RAW"].value)

def unpackLayer0(p):
  if not isDataPacket(p):
    return None
  data = getData(p)
  cmd, flag, src, des, rcount, length = struct.unpack(">xBxBBBHH", data[:10])
  payload = data[10:]
  return cmd, flag, src, des, rcount, length, payload

def unpackRequestResponse(payload):
  msb, u1, f1, what, u2, length = struct.unpack("<BBBBBH", payload[:7])
  lsb, end = struct.unpack("BB", payload[-2:])
  return u1, f1, what, u2, ((msb<<8)|lsb), end, length, payload[7:-2]

def unpackReadWriteRequest(payload):
  ukn1, ukn2, ukn3, ukn4, addr, length = struct.unpack("<bHHbIH", payload[:12])
  crc = 0x59fd - (addr&0xffff) - (addr>>16) - length - 0x01
  content = payload[12:]
  for i in range(len(content)):
    crc -= content[i]
  return ukn1, ukn2, ukn3, ukn4, addr, length, crc%0xffff, content

def unpackReadWriteResponse(payload):
  addr, length = struct.unpack("<xxxxxxxIH", payload[:13])
  crc = 0x59fd - (addr&0xffff) - (addr>>16) - length - 0x01
  content = payload[13:]
  for i in range(len(content)):
    crc -= content[i]
  return addr, length, crc&0xffff, content

def isReadRequest(p):
  return isDataPacket(p) and isFromHost(p) and (0x01 == getData(p)[1])

def isReadResponse(p):
  return isDataPacket(p) and isToHost(p) and (0x04 == getData(p)[1])


if "raw" == args.command:
  print("#")
  print("# Serial data from '{0}'".format(args.cap_file))
  print("# '>' means from host to device.")
  print("# '<' means from device to host.")
  print("#")
  cap = pyshark.FileCapture(args.cap_file, include_raw=True, use_json=True)
  for p in cap:
    if not isDataPacket(p):
      continue
    if isFromHost(p):
      print("-"*80)
    hexdat = hexDump(getData(p))
    if isFromHost(p):
      print(hexDump(getData(p), "> | "))
    elif isToHost(p):
      print(hexDump(getData(p), "< | "))
      
elif "level0" == args.command:
  cap = pyshark.FileCapture(args.cap_file, include_raw=True, use_json=True)
  for p in cap:
    if not isDataPacket(p):
      continue
    if isFromHost(p):
      print("-"*80)
    cmd, flag, src, des, rcount, tlen, payload = unpackLayer0(p)
    plen = len(payload)
    dir = None
    if isFromHost(p): dir = ">"
    elif isToHost(p): dir = "<"

    if 0x00 == cmd: cmd = "CMD"
    elif 0x01 == cmd: cmd = "REQ"
    elif 0x04 == cmd: cmd = "RES"
    print("{} type={}, len={:04X}, flag={:02X}, rcount={:04X}:".format(dir, cmd, tlen, flag, rcount))
    print(hexDump(payload, "  | "))

elif "payload" == args.command:
  print("#")
  print("#")
  cap = pyshark.FileCapture(args.cap_file, include_raw=True, use_json=True)
  for p in cap:
    if not isDataPacket(p):
      continue
    cmd, flag, src, des, rcount, tlen, payload = unpackLayer0(p)
    plen = len(payload)

    dir = None
    if isFromHost(p): dir = ">"
    elif isToHost(p): dir = "<"

    if 0x00 == cmd: cmd = "CMD"
    elif 0x01 == cmd: cmd = "REQ"
    elif 0x04 == cmd: cmd = "RES"

    if (">" == dir):
      print("-"*80) 
    
    print("{} type={}, len={:04X}, flag={:02X}, rcount={:04X}:".format(dir, cmd, tlen, flag, rcount))
    if (("REQ" == cmd) or ("RES"==cmd)) and plen>9:
      u1, f1, what, u2, crc, end, plen, payload = unpackRequestResponse(payload)
      print("  | crc={:04X}, seq={:02X}, fixed={:02X}, req={:02X}, ukn2={:02X}, payload len={:04X}".format(crc, u1, f1, what, u2, plen))

      if (0xC7 == what) and ("REQ"==cmd): # read request
        ukn1, ukn2, ukn3, ukn4, addr, length, crc_comp, payload = unpackReadWriteRequest(payload); 
        crc_comp -= u1; crc_comp = crc_comp % 0xffff
        if crc!=crc_comp:
          crc = "\x1b[1;31m{:04X} ERR\x1b[0m".format(crc_comp)
        else: 
          crc = "{:04X}".format(crc_comp) 
        print("  | READ from={:08X}, len={:04X}, crc={}".format(addr, length, crc))
        if not args.nodump:
          print(hexDump(payload, "  |  | "))
      elif (0xC7 == what) and ("RES"==cmd): # read response
        addr, length, crc_comp, payload = unpackReadWriteResponse(payload); 
        crc_comp -= (u1)
        crc_comp = crc_comp % 0xffff
        if crc!=crc_comp:
          crc = "{:04X} ERR".format(crc_comp)
        else: 
          crc = "{:04X}".format(crc_comp) 
        print("  | READ from={:08X}, len={:04X}, crc={}".format(addr, length, crc))
        if not args.nodump:
          print(hexDump(payload, "  |  | "))
      elif (0xC8 == what) and ("REQ"==cmd):
        ukn1, ukn2, ukn3, ukn4, addr, length, crc_comp, payload = unpackReadWriteRequest(payload)
        if crc!=crc_comp:
          crc = "{:04X} ERR".format(crc_comp)
        else: 
          crc = "{:04X}".format(crc_comp) 
        print("  | WRITE to={:08X}, len={:04X}, crc={}".format(addr, length, crc))
        if not args.nodump:
          print(hexDump(payload, "  |  | "))
      elif (0xC8 == what) and ("RES"==cmd):
        addr, length, crc_comp, payload = unpackReadWriteResponse(payload)
        if crc!=crc_comp:
          crc = "{:04X} ERR".format(crc_comp)
        else: 
          crc = "{:04X}".format(crc_comp) 
        print("  | WRITE to={:08X}, len={:04X}, crc={}".format(addr, length, crc))
        if not args.nodump:
          print(hexDump(payload, "  |  | "))
      elif not args.nodump:
        print(hexDump(payload, "  | "))


elif "read_codeplug"==args.command:
  print("#")
  cap = pyshark.FileCapture(args.cap_file, include_raw=True, use_json=True)
  current_addr = 0xffffffff
  leftover = b''
  for p in cap:
    if (not isDataPacket(p)) or isFromHost(p):
      continue
    
    # unpack layer 0
    cmd, flag, src, des, ukn, tlen, payload = unpackLayer0(p)
    plen = len(payload)
    if (0x04 != cmd) or (9 > plen): # only handle responses
      continue
    
    # unpack layer 1
    u1, f1, what, u2, crc, end, plen, payload = unpackRequestResponse(payload)
    if 0xC7 != what: # filter only read data 
      continue

    # unpack layer 2
    addr, length, payload = unpackReadWriteResponse(payload)
    # Prepend left over bytes to fill up to 16b
    if len(leftover):
      payload  = leftover+payload
      addr    -= len(leftover)
      length  += len(leftover)
      leftover = b''

    # ensure payload length is multiple of 16
    if len(payload)%16:
      leftover = payload[-(len(payload)%16):]
      payload  = payload[:-(len(payload)%16)]
      length  -= len(leftover)

    # print
    if (0xffffffff != current_addr) and (current_addr != addr):
      if len(leftover):
        print(hexDump(leftover, "", current_addr))
        leftover = b''
      print("-"*75)
    print(hexDump(payload, "", addr))
    # update current address
    current_addr = addr+length
  
  if len(leftover):
    print(hexDump(payload, "", current_addr))


elif "write_codeplug"==args.command:
  print("#")
  cap = pyshark.FileCapture(args.cap_file, include_raw=True, use_json=True)
  current_addr = 0xffffffff
  for p in cap:
    if (not isDataPacket(p)) or isToHost(p):
      continue
    
    # unpack layer 0
    cmd, flag, src, des, ukn, tlen, payload = unpackLayer0(p)
    plen = len(payload)
    if (0x01 != cmd) or (8 > plen): # only handle requests
      continue
    
    # unpack layer 1
    u1, f1, what, u2, crc, end, plen, payload = unpackRequestResponse(payload)
    if 0xC8 != what: # filter only write data 
      continue

    # unpack layer 2
    addr, length, payload = unpackReadWriteRequest(payload)

    # if not continuos -> print separator
    if (current_addr != addr):
      print("-"*75)
    
    print(hexDump(payload, "", addr))
    
    # update current address
    current_addr = addr+length

elif "crc" == args.command:
  print("#crc,ukn1,ukn2,addr,length")
  cap = pyshark.FileCapture(args.cap_file, include_raw=True, use_json=True)
  for p in cap:
    if (not isDataPacket(p)) or isFromHost(p):
      continue
    # unpack layer 0
    cmd, flag, src, des, ukn, tlen, payload = unpackLayer0(p)
    plen = len(payload)
    # only handle responses
    if (0x04 != cmd) or (8 > plen): 
      continue
    # unpack layer 1
    u1, f1, what, u2, crc, end, plen, payload = unpackRequestResponse(payload)
    if 0xC8 != what: # filter only write data 
      continue
    # unpack layer 2
    addr, length, crc_comp, payload = unpackReadWriteResponse(payload)
    print("{},{},{},{},{}".format(crc, u1, u2, addr, length))
