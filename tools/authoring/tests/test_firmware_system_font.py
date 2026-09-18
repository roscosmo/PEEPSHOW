"""Check the production shell font against HW4 and the authoring glyphs."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_firmware_package_workflow import TOOL_ROOT, firmware_function
from peepshow_authoring.system_fonts import _FONT_8X8_BASIC

ROOT = TOOL_ROOT.parents[1]
FW = ROOT / "firmware/peepshow_hw6_fw0"


class SystemFontTests(unittest.TestCase):
    def test_glyph_parity(self):
        header = (FW / "Core/Inc/ps_system_font.h").read_text()
        data = bytes(int(n, 16) for n in re.findall(r"0x([0-9a-fA-F]{2})U", header))
        self.assertEqual(_FONT_8X8_BASIC, data)
        historical = (ROOT / "firmware/peepshow_hw4_fw1/Core/Src/font8x8_basic.c").read_text()
        rows = re.findall(r"\{([^{}]+)\}", historical)
        old = bytes(int(n, 16) for row in rows for n in re.findall(r"0x([0-9a-fA-F]{2})", row))
        self.assertEqual(old[32 * 8:127 * 8], data)

    def test_native_scaled_pixels_and_fit(self):
        compiler = os.environ.get("HOST_CC") or shutil.which("gcc") or "C:/msys64/ucrt64/bin/gcc.exe"
        if not Path(compiler).is_file():
            self.skipTest("native GCC unavailable")
        source = (FW / "Core/Src/display_renderer.c").read_text()
        functions = "\n".join(firmware_function(source, "DisplayRenderer_" + name)
                              for name in ("GlyphRows", "DrawGlyph", "TextWidth", "DrawText", "DrawCenteredText"))
        harness = '''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ps_system_font.h"
#define DISPLAY_RENDERER_WIDTH 168U
#define DISPLAY_RENDERER_HEIGHT 144U
static uint8_t pixels[144][168];
static uint32_t DisplayRenderer_SetBlack(uint16_t x, uint16_t y) {
  if (x >= 168 || y >= 144) return 0;
  pixels[y][x] = 1; return 1;
}
''' + functions + '''
int main(void) {
  for (uint16_t scale = 1; scale <= 8; ++scale) {
    for (uint16_t code = 32; code <= 126; ++code) {
      memset(pixels, 0, sizeof(pixels));
      DisplayRenderer_DrawGlyph(0, 0, (char)code, scale);
      fwrite(pixels, 1, sizeof(pixels), stdout);
    }
  }
  memset(pixels, 0, sizeof(pixels));
  DisplayRenderer_DrawCenteredText(7, "CALIBRATION", 2);
  fwrite(pixels, 1, sizeof(pixels), stdout);
  return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            environment = dict(os.environ)
            environment["PATH"] = str(Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
            src, exe = Path(tmp) / "font.c", Path(tmp) / "font.exe"
            src.write_text(harness)
            compiled = subprocess.run([compiler, "-std=c11", "-Wall", "-Werror", "-I", str(FW / "Core/Inc"), str(src), "-o", str(exe)], capture_output=True, env=environment, timeout=30)
            self.assertEqual(0, compiled.returncode, compiled.stderr.decode())
            result = subprocess.run([str(exe)], check=True, capture_output=True, env=environment, timeout=20).stdout
        offset = 0
        for scale in range(1, 9):
            for code in range(32, 127):
                expected = bytearray(168 * 144)
                for y, row in enumerate(_FONT_8X8_BASIC[(code - 32) * 8:(code - 31) * 8]):
                    for x in range(8):
                        if row & (1 << x):
                            for dy in range(scale):
                                for dx in range(scale):
                                    expected[(y * scale + dy) * 168 + x * scale + dx] = 1
                self.assertEqual(expected, result[offset:offset + len(expected)], (code, scale))
                offset += len(expected)
        expected = bytearray(168 * 144)
        for i, char in enumerate("CALIBRATION"):
            for y, row in enumerate(_FONT_8X8_BASIC[(ord(char) - 32) * 8:(ord(char) - 31) * 8]):
                for x in range(8):
                    if row & (1 << x):
                        expected[(7 + y) * 168 + 40 + i * 8 + x] = 1
        self.assertEqual(expected, result[offset:])


if __name__ == "__main__":
    unittest.main()
