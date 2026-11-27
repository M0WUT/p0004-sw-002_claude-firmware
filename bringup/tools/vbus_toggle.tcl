# OpenOCD script: toggle host VBUS enable (PD14) and sample ~FAULT (PD15) over SWD.
# The CPU keeps running; whatever firmware is loaded must not drive PD14/PD15.
set GPIOD 0x58020C00
proc rd {a} { return [read_memory $a 32 1] }
# PD14 output push-pull, low first; PD15 input with pull-up
mww [expr {$GPIOD + 0x18}] [expr {1 << 30}]
set moder [rd $GPIOD]
set moder [expr {($moder & ~(0xF << 28)) | (0x1 << 28)}]
mww $GPIOD $moder
set pupdr [rd [expr {$GPIOD + 0x0C}]]
mww [expr {$GPIOD + 0x0C}] [expr {($pupdr & ~(0x3 << 30)) | (0x1 << 30)}]
sleep 100
echo "idle (EN low): PD15=[expr {([rd [expr {$GPIOD + 0x10}]] >> 15) & 1}]"
for {set t 1} {$t <= 3} {incr t} {
  set t0 [clock milliseconds]
  mww [expr {$GPIOD + 0x18}] [expr {1 << 14}]
  set samples {}
  set firstFault -1
  set lastFault -1
  while {[clock milliseconds] - $t0 < 300} {
    set f [expr {(([rd [expr {$GPIOD + 0x10}]] >> 15) & 1) == 0}]
    set ms [expr {[clock milliseconds] - $t0}]
    if {$f} { if {$firstFault < 0} { set firstFault $ms }; set lastFault $ms }
    lappend samples $f
  }
  set end [expr {(([rd [expr {$GPIOD + 0x10}]] >> 15) & 1) == 0}]
  mww [expr {$GPIOD + 0x18}] [expr {1 << 30}]
  set n [llength $samples]
  set nf [llength [lsearch -all $samples 1]]
  echo "try $t: EN high 300 ms, $n samples, fault in $nf, first fault at $firstFault ms, last at $lastFault ms, fault at end=$end"
  sleep 1000
  echo "try $t: after EN low 1 s, PD15=[expr {([rd [expr {$GPIOD + 0x10}]] >> 15) & 1}]"
}
