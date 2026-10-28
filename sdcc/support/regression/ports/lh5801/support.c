/* Hooks for pc1500sim (pc1500emu's tools/sim), the simulator this port's
   regression tests run on: it prints a byte written to ME1 FFF0H and stops
   on a write to ME1 FFF1H, an address no PC-1500 hardware answers to. */

unsigned char simByte;

void
_putchar(unsigned char c)
{
  simByte = c;
  __asm
    lda (_simByte)
    sta #(0xFFF0)
  __endasm;
}

void
_initEmu(void)
{
}

void
_exitEmu(void)
{
  __asm
    ldi a, 0x00
    sta #(0xFFF1)
  __endasm;
}
