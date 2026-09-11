"""Single-slot NOR journal with interruption and protected-region checks."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class StorageTransactionTests(unittest.TestCase):
    def test_real_index_layout_reader_and_interrupted_replacement(self):
        firmware = Path(__file__).resolve().parents[3] / "firmware/peepshow_hw6_fw0"
        compiler = os.environ.get("HOST_CC", "C:/msys64/ucrt64/bin/gcc.exe")
        if not Path(compiler).is_file():
            self.skipTest("native GCC required")
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "stm32u5xx_hal.h").write_text(
                "#ifndef TEST_HAL_H\n#define TEST_HAL_H\n"
                "typedef struct { unsigned unused; } OSPI_HandleTypeDef;\n"
                "typedef struct { unsigned unused; } DMA_HandleTypeDef;\n#endif\n", encoding="ascii")
            exe = work / "transaction.exe"
            env = dict(os.environ)
            env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
            result = subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                "-I", str(work), "-I", str(firmware / "Core/Inc"), "-I", str(firmware / "Core/Src"),
                str(Path(__file__).with_name("native_storage_transaction.c")), "-o", str(exe)],
                capture_output=True, text=True, timeout=60, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, env=env)
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
