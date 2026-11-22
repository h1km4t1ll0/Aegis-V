"""Range/header checks must reject invalid inputs before QEMU starts."""
import importlib.util
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]
spec = importlib.util.spec_from_file_location('aegis_linux', ROOT / 'src/scripts/linux.py')
boot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boot)

class ImageHeaderTests(unittest.TestCase):
    def image(self, offset=0x200000, size=0x200000, flags=0):
        data = bytearray(64)
        struct.pack_into('<QQQ', data, 8, offset, size, flags)
        data[48:56] = b'RISCV\0\0\0'
        data[56:60] = b'RSC\x05'
        return data

    def test_valid(self):
        self.assertEqual(boot.validate_image(self.image()), 0x200000)

    def test_truncated(self):
        with self.assertRaises(ValueError):
            boot.validate_image(b'')

    def test_magic(self):
        data = self.image()
        data[56] = 0
        with self.assertRaises(ValueError):
            boot.validate_image(data)

    def test_ranges_and_endianness(self):
        for values in [dict(offset=0), dict(size=63), dict(size=0x8000000), dict(flags=1)]:
            with self.subTest(values=values), self.assertRaises(ValueError):
                boot.validate_image(self.image(**values))


class BootArgumentTests(unittest.TestCase):
    def test_disk_root_stage_and_owned_disk_exclusivity(self):
        a = boot.parse_args(['--shell', '--disk-root', 'root.raw', '--root-stage', 'first'])
        self.assertEqual(a.disk_root, Path('root.raw'))
        self.assertEqual(a.root_stage, 'first')
        self.assertLess(len(boot.kernel_cmdline(True, 'AEGIS_ROOT=aegis-0123456789abcd AEGIS_BOOT=verify')), 200)
        for args in (['--test', '--root-stage', 'first'],
                     ['--test', '--disk-root', 'a', '--disk', 'b'],
                     ['--test', '--disk-root', 'a', '--verify-disk', 'b']):
            with self.subTest(args=args), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                boot.parse_args(args)

    def test_source_export_and_kernel_override(self):
        args = boot.parse_args(['--shell', '--disk', 'data.raw', '--input-tar', 'source.tar',
                                '--export-disk', 'export.raw', '--image', 'Image'])
        self.assertEqual(args.input_tar, Path('source.tar'))
        self.assertEqual(args.export_disk, Path('export.raw'))
        self.assertEqual(args.image, Path('Image'))
        for options in (['--input-tar', 'source.tar'], ['--export-disk', 'export.raw']):
            with self.subTest(options=options), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                boot.parse_args(['--shell'] + options)

    def test_disk_modes_preserve_boot_options(self):
        for option in ('--disk', '--new-disk', '--verify-disk'):
            with self.subTest(option=option):
                args = boot.parse_args(['--test', option, 'disk.raw', '--rootfs', 'guest.tar',
                                        '--timeout', '1800'])
                self.assertEqual(args.rootfs, Path('guest.tar'))
                self.assertEqual(args.timeout, 1800)
                self.assertTrue(args.test)
        self.assertTrue(boot.parse_args(['--shell', '--disk', 'disk.raw']).shell)

    def test_invalid_argument_combinations_and_timeout(self):
        for args in (['--test', '--disk', 'a', '--new-disk', 'b'],
                     ['--test', '--new-disk', 'a', '--verify-disk', 'b'],
                     ['--test', '--disk', 'a', '--verify-disk', 'b'],
                     ['--test', '--timeout', '0'], ['--test', '--timeout', '-1']):
            with self.subTest(args=args), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                boot.parse_args(args)

    def test_command_line_rejects_truncation_and_nul(self):
        self.assertIn(b'PATH=/usr/bin:/usr/sbin:/bin:/sbin', boot.kernel_cmdline())
        padding = 'x' * (boot.CMDLINE_BYTES - len(boot.kernel_cmdline()) - 2)
        self.assertEqual(len(boot.kernel_cmdline(bootargs=padding)), 199)
        for value in (padding + 'x', 'bad\0argument', 'non-ascii-\u2603'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                boot.kernel_cmdline(bootargs=value)
        args = boot.kernel_cmdline(True, 'aegis.disk-mode=verify aegis.disk-token=aegis-0123456789abcd')
        self.assertIn(b'aegis.shell=1', args)
        self.assertIn(b'aegis.disk-token=aegis-0123456789abcd', args)


class OwnedDiskTests(unittest.TestCase):
    def test_new_blank_disk_and_metadata_round_trip(self):
        with tempfile.TemporaryDirectory() as tmp, redirect_stdout(io.StringIO()):
            path, token = boot.create_disk(Path(tmp) / 'disk.raw')
            self.assertEqual(path.stat().st_size, 4 * 1024**3)
            self.assertRegex(token, r'^aegis-[0-9a-f]{14}$')
            self.assertEqual(boot.load_disk(path), (path, token))
            with path.open('rb') as raw:
                self.assertEqual(raw.read(4096), bytes(4096))
                raw.seek(-4096, 2)
                self.assertEqual(raw.read(), bytes(4096))

    def test_creation_preserves_existing_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'existing.raw'
            path.write_bytes(b'existing user data')
            with self.assertRaises(FileExistsError):
                boot.create_disk(path)
            self.assertEqual(path.read_bytes(), b'existing user data')
            link = Path(tmp) / 'link.raw'
            target = Path(tmp) / 'missing.raw'
            link.symlink_to(target)
            with self.assertRaises(FileExistsError):
                boot.create_disk(link)
            self.assertFalse(target.exists())
            sidecar = Path(tmp) / 'fresh.raw.owner.json'
            sidecar.write_text('existing metadata')
            with self.assertRaises(FileExistsError):
                boot.create_disk(Path(tmp) / 'fresh.raw')
            self.assertEqual(sidecar.read_text(), 'existing metadata')
            self.assertFalse((Path(tmp) / 'fresh.raw').exists())
            with self.assertRaises(ValueError):
                boot.create_disk(Path(tmp) / 'comma,disk.raw')

    def test_verify_rejects_wrong_path_size_and_token(self):
        with tempfile.TemporaryDirectory() as tmp, redirect_stdout(io.StringIO()):
            path, _ = boot.create_disk(Path(tmp) / 'disk.raw')
            sidecar = path.with_name(path.name + '.owner.json')
            metadata = json.loads(sidecar.read_text())
            for field, value in (('raw_path', str(path) + '.other'), ('size_bytes', 512),
                                 ('token', 'aegis-0123456789abcZ')):
                with self.subTest(field=field):
                    sidecar.write_text(json.dumps({**metadata, field: value}))
                    with self.assertRaises(ValueError):
                        boot.load_disk(path)
            sidecar.write_text(json.dumps(metadata))
            with path.open('r+b') as raw:
                raw.truncate(512)
            with self.assertRaises(ValueError):
                boot.load_disk(path)


if __name__ == '__main__':
    unittest.main()
