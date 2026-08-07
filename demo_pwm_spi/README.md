## PWM + SPI demo

PWM on PMOD0

SPI on PMOD 1 to 4:

```
_spi_extension = [ ("spi", 0,
  Subsignal("clk",  Pins("PMOD:1")),
  Subsignal("mosi", Pins("PMOD:2")),
  Subsignal("miso", Pins("PMOD:3")),
  Subsignal("cs_n", Pins("PMOD:4")),
)]
```

<img src="PWM_p1000_d100.png">

<img src="SPI_CK_PMOD1.png">

<img src="SPI_CS_PMOD4.png">

When executing ``spi w 55``:

<img src="SPI_MOSI_PMOD2.png">

After synthesis, see ``build/olimex_gatemate_a1_evb/gateware/*ccf`` for pin assignment.

After synthesis, see ``build/olimex_gatemate_a1_evb/software/include/generated/csr.h``
for register location and associated functions.
