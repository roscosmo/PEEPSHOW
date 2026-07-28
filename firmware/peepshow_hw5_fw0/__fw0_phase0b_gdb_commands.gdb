set pagination off
set confirm off
file build/Debug/peepshow_hw5_fw0.elf
target remote localhost:61234
info registers pc lr sp xpsr
p/x g_ps_phase0b_pmic_probe
x/64xb &g_ps_phase0b_pmic_probe
info breakpoints
detach
quit
