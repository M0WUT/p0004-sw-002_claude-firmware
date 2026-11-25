# OpenOCD script: hold host VBUS enable (PD14) high for 2 s, three times, 3 s apart, logging ~FAULT (PD15).
# For measuring 5V_VBUS with a meter. Assumes PD14 is already an output (run vbus_toggle.tcl first or set MODER).
set GPIOD 0x58020C00
proc pd15 {} { return [expr {([read_memory 0x58020C10 32 1] >> 15) & 1}] }
set moder [read_memory $GPIOD 32 1]
mww $GPIOD [expr {($moder & ~(0x3 << 28)) | (0x1 << 28)}]
for {set t 1} {$t <= 3} {incr t} {
  echo "pulse $t: EN HIGH now"
  mww [expr {$GPIOD + 0x18}] [expr {1 << 14}]
  set t0 [clock milliseconds]; set n 0; set nf 0
  while {[clock milliseconds] - $t0 < 2000} { incr n; if {[pd15] == 0} { incr nf } }
  mww [expr {$GPIOD + 0x18}] [expr {1 << 30}]
  echo "pulse $t: EN LOW, fault in $nf of $n samples"
  sleep 3000
}
echo "done, EN LOW, PD15=[pd15]"
