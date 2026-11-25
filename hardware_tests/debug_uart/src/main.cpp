// Hardware test: debug UART (USART2) -> STLINK-V3 VCP.
// Streams sequence-numbered lines and echoes received bytes back as "ECHO:<bytes>".
#include <Arduino.h>

Uart dbg(PD_6, PD_5); // RX PD6, TX PD5 (sw-001 pinout.h)

static uint32_t seq = 0;
static char echoBuf[64];
static size_t echoLen = 0;

void setup()
{
  dbg.begin(115200);
}

void loop()
{
  // ~40 lines/s, 48 bytes each: ~1.9 kB/s, well under the 11.5 kB/s line rate
  static uint32_t next = 0;
  if ((int32_t)(millis() - next) >= 0)
  {
    next += 25;
    dbg.printf("SEQ %08lu T=%010lu ABCDEFGHIJKLMNOPQRSTU\r\n", (unsigned long)seq++, (unsigned long)millis());
  }
  while (dbg.available())
  {
    int c = dbg.read();
    if (c == '\n' || echoLen >= sizeof(echoBuf) - 1)
    {
      echoBuf[echoLen] = 0;
      dbg.printf("ECHO:%s\r\n", echoBuf);
      echoLen = 0;
    }
    else if (c != '\r')
      echoBuf[echoLen++] = (char)c;
  }
}
