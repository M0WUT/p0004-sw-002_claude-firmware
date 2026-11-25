import serial, sys, time
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.2)
s.dtr = True; s.rts = False
end = time.time() + float(sys.argv[1])
out = open(sys.argv[2], 'wb')
while time.time() < end:
    out.write(s.read(4096)); out.flush()
