set pagination off
set confirm off
file build/Debug/peepshow_hw5_fw0.elf
target remote localhost:61234
info registers pc lr sp xpsr
info breakpoints
detach
quit
