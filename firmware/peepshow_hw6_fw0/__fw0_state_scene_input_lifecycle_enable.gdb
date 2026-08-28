set pagination off
call (unsigned int)PS_HW6_RTOS_DebugRequestInputDiagnostic()
printf "--- HW6 input lifecycle diagnostic requested ---\n"
printf "Continue until INPUT TEST is visible.\n"
printf "Use A/B/L/R and the joystick naturally. The screen shows the latest source, event, raw 8-way candidate, and deterministic 4-way result.\n"
printf "Hold any non-START input to see HOLD and REPEAT. No exact timing or ordered sequence is required.\n"
printf "When done, halt and source __fw0_state_scene_input_lifecycle_prints.gdb.\n"
