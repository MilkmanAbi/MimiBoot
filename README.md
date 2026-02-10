# PicoBoot
Minimal second-stage bootloader for ARM Cortex-M. Loads ELF images from external storage (SD, SPI flash) into RAM, hands off execution, and vanishes. Like GRUB, but for microcontrollers. Not secure, not production-grade — just a dumb loader for tinkerers who want to boot custom payloads without reflashing.
