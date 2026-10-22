import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("flash", Path(__file__).parents[1]/"tools/evb/fr3068_flash.py")
flash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flash)


class Protection(unittest.TestCase):
    def test_burn_gap(self):
        for value in (0, 255, 123):
            board = bytes([value])*4096
            image = b"A"*4096 + board + b"C"*6000
            segments = flash.plan_segments(image, flash.FLASH_BASE, board)
            self.assertEqual(segments, [(0x08000000,b"A"*4096),(0x08002000,b"C"*6000)])
            for base, data in segments:
                self.assertFalse(base < 0x08002000 and base+len(data) > 0x08001000)
            with self.assertRaises(ValueError):
                flash.plan_segments(image,flash.FLASH_BASE,bytes([value^1])*4096)

    def test_ranges(self):
        board=bytes(4096)
        for base,n in [(0x08001000,1),(0x08000fff,2),(0x08001fff,2),
                       (0x08001000,4096),(0x07ffffff,2),(0x081fffff,2),(0x08002000,0)]:
            with self.assertRaises(ValueError):flash.plan_segments(bytes(n),base,board)
        self.assertEqual(flash.plan_segments(b"x",0x08000fff,board),[(0x08000fff,b"x")])
        self.assertEqual(flash.plan_segments(b"x",0x08002000,board),[(0x08002000,b"x")])
        self.assertEqual(flash.plan_segments(b"x",0x081fffff,board),[(0x081fffff,b"x")])
        self.assertEqual(flash.plan_segments(b"app",0x08020000,board),[(0x08020000,b"app")])

    def test_header(self):
        image=(Path(__file__).parents[1]/"build_installer/output/Project_burn.bin").read_bytes()
        flash.validate_fr509x_header(image)
        bad=bytearray(image);bad[-1]^=1
        with self.assertRaises(ValueError):flash.validate_fr509x_header(bad)


if __name__ == "__main__":unittest.main()
