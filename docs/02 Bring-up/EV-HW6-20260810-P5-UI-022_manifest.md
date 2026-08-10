# EV-HW6-20260810-P5-UI-022

## Scope

UI router, basic display orientation, generic A/B/L/R button routing, and
joystick calibration status from the HW6 FW0 board session.

## Source

Conversation-provided GDB captures and user observations from HW6 unit 001.

## Captures And Observations

Initial UI HOME route after boot event:

```text
api/status/event       = 1 / 0x0 / 1
page prev/current/req  = 0 / 1 / 1
nav/modal/cal         = 1 / 0 / 0
transitions/rejected  = 1 / 0
display ui req/render = 1 / 1
display ui page/cal   = 1 / 0
display status/hash   = 0x0 / 0x2940db85
```

User confirmed a display appeared after a few seconds. The display was initially
rotated incorrectly. Firmware was then adjusted for 90 degree counter-clockwise
orientation, and the user confirmed the corrected orientation.

Button/UI routing capture after physical A/B/L/R navigation:

```text
api/status/event       = 1 / 0x0 / 18
page prev/current/req  = 2 / 4 / 4
nav/modal/cal/focus   = 1 / 0 / 1 / 0
transitions/rejected  = 29 / 3
button event/count    = 18 / 32
display ui req/render = 30 / 30
display ui page/cal   = 4 / 1
input edge/press      = 65 / 32
input pending/button  = 0x0 / 2
```

User confirmation: A/B/L/R can navigate, with L/R moving focus and A/B entering
and exiting pages.

Joystick diagnostic status before pivot:

```text
input api/policy/status   = 1 / 4 / 0x0
cal/active/dir/mag       = 1 / 1 / 0x4 / 903
norm X/Y                 = 0 / -903
delta X/Y                = -3008 / -20080
raw X/Y/Z conv           = -8624 / -25616 / -32576 / 0xd1
sample tick/age/updates  = 277 / 0 / 84
live req/status/count/err = 1 / 0x0 / 84 / 0
```

The user reported the stick was neutral at the end of that capture, so the
current joystick calibration/deadzone state is not acceptable for normal shell
or game navigation.

## Result

PASS for basic owner-routed UI display and contextual A/B/L/R shell navigation.
PASS for display orientation correction.
PARTIAL for joystick: raw/live diagnostic sampling exists, but calibration and
threshold/cardinal policy remain open.

## Interpretation

The Platform-level button model remains generic. `BTN_A`, `BTN_B`, `BTN_L`, and
`BTN_R` are not universal accept/back/up/down events; `thUI` maps them according
to current shell context. This is compatible with future OS requirements.

Joystick calibration should move to an on-device guided flow using the display
and buttons. Debugger-only one-position captures are not sufficient calibration
evidence.

## Still Open

- final UI renderer and page content
- joystick guided calibration state flow
- joystick deadzone/range persistence
- joystick threshold interrupt cardinal-input mode
- realtime live joystick streaming mode for gameplay/calibration
- long press, repeat, chord, stuck-button, wake, and START shipping-prep tests