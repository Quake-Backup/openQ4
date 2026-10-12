#!/usr/bin/env python3
"""Check Android cross-toolchain validation and renderer-free engine source split."""

from __future__ import annotations

from pathlib import Path
import json
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import ssl
import zipfile
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
import android_cross
import prepare_android_deps
import android_elf
import verify_android_apk


def android_library(api: int = 24, alignment: int = 16384, machine: int = 183) -> bytes:
    """Small valid ELF fixture with one load segment and an NDK target note."""
    data = bytearray(200)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", data, 16, 3, machine, 1)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HHH", data, 52, 64, 56, 2)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, len(data), len(data), alignment)
    struct.pack_into("<IIQQQQQQ", data, 120, 4, 4, 176, 176, 176, 24, 24, 4)
    struct.pack_into("<III8sI", data, 176, 8, 4, 1, b"Android\0", api)
    return bytes(data)


class AndroidBuildTests(unittest.TestCase):
    def test_release_apk_payload_rejects_mixed_or_incomplete_packages(self) -> None:
        with tempfile.TemporaryDirectory(prefix='android-apk-', dir=ROOT / '.tmp') as temporary:
            stage = Path(temporary) / 'stage'
            files = {'lib/' + name: android_library() for name in verify_android_apk.LIBRARIES}
            files.update({
                'android-build.json': b'{"abi":"arm64-v8a","androidApi":24}',
                'baseoq4/mod.json': b'{"version":"0.14.000","requiredopenQ4Version":"0.14.000"}',
                'baseoq4/pak0.pk4': b'first pack', 'baseoq4/pak1.pk4': b'second pack',
                'LICENSE': b'engine license', 'LICENSING.md': b'component licenses', 'CONTRIBUTORS.md': b'credits',
                'licenses/QUAKE-4-SDK-EULA.rtf': b'SDK EULA',
                'licenses/sdl3/LICENSE.txt': b'SDL', 'licenses/openal-soft/COPYING': b'OpenAL',
                'licenses/android-ndk/NOTICE.toolchain': b'NDK',
            })
            entries = {}
            revision = hashlib.sha256()
            for name, data in sorted(files.items()):
                path = stage / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                entries['lib/arm64-v8a/' + name[4:] if name.startswith('lib/') else 'assets/' + name] = data
                if name.startswith('baseoq4/'):
                    revision.update(name[len('baseoq4/'):].encode())
                    revision.update(data)
            entries['assets/openq4-package-version.txt'] = revision.hexdigest().encode()
            apk = Path(temporary) / 'release.apk'

            def package(payload):
                with zipfile.ZipFile(apk, 'w') as archive:
                    for name, data in payload.items():
                        archive.writestr(name, data)
                    archive.writestr('AndroidManifest.xml', b'manifest checked by aapt')

            package(entries)
            verify_android_apk.verify_payload(apk, stage, '0.14.000')
            for name, replacement, message in (
                ('lib/arm64-v8a/libquake4.so', b'old binary', 'differs from validated stage'),
                ('lib/arm64-v8a/libgame-sp_arm64.so', None, 'seven matching'),
                ('assets/baseoq4/pak0.pk4', b'old data', 'differs from validated stage'),
                ('assets/licenses/openal-soft/COPYING', None, 'asset set differs'),
                ('assets/licenses/QUAKE-4-SDK-EULA.rtf', None, 'asset set differs'),
                ('assets/q4base/pak001.pk4', b'retail data', 'asset set differs'),
                ('assets/openq4-package-version.txt', b'wrong revision', 'extraction revision'),
            ):
                with self.subTest(name=name):
                    changed = dict(entries)
                    if replacement is None:
                        changed.pop(name)
                    else:
                        changed[name] = replacement
                    package(changed)
                    with self.assertRaisesRegex(ValueError, message):
                        verify_android_apk.verify_payload(apk, stage, '0.14.000')
            package(entries)
            with self.assertRaisesRegex(ValueError, 'versions do not match'):
                verify_android_apk.verify_payload(apk, stage, '0.15.000')

    def test_release_apk_manifest_and_signer_gates(self) -> None:
        certificate = ROOT / 'mobile/android/release-signing-cert.pem'
        cert_digest = hashlib.sha256(ssl.PEM_cert_to_DER_cert(certificate.read_text())).hexdigest()
        good = [
            f'Verified using v2 scheme (APK Signature Scheme v2): true\nSigner #1 certificate SHA-256 digest: {cert_digest}\n',
            '',
            "package: name='com.darkmatter.openq4' versionCode='14000' versionName='0.14.000'\nsdkVersion:'24'\ntargetSdkVersion:'35'\nnative-code: 'arm64-v8a'\n",
            'A: android:extractNativeLibs(0x010104ea)=(type 0x12)0xffffffff',
        ]
        for index, replacement, message in (
            (-1, '', ''),
            (0, good[0].replace(cert_digest, '0' * 64), 'pinned release certificate'),
            (0, good[0].replace(': true', ': false'), 'valid v2 signature'),
            (2, good[2].replace("sdkVersion:'24'", "sdkVersion:'28'"), 'manifest mismatch'),
            (2, good[2].replace("versionCode='14000'", "versionCode='13001'"), 'manifest mismatch'),
            (2, good[2] + 'application-debuggable', 'not be debuggable'),
            (3, good[3].replace('0xffffffff', '0x0'), 'extract native libraries'),
        ):
            outputs = good.copy()
            if index >= 0:
                outputs[index] = replacement
            with mock.patch.object(verify_android_apk.subprocess, 'run', side_effect=[
                subprocess.CompletedProcess([], 0, stdout=text) for text in outputs
            ]):
                if message:
                    with self.assertRaisesRegex(ValueError, message):
                        verify_android_apk.verify_tools(Path('release.apk'), Path('tools'), certificate, '0.14.000')
                else:
                    verify_android_apk.verify_tools(Path('release.apk'), Path('tools'), certificate, '0.14.000')

    def test_library_validation_checks_loadability(self) -> None:
        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="android-elf-test-", dir=ROOT / ".tmp") as temporary:
            library = Path(temporary) / "libfixture.so"
            library.write_bytes(android_library(api=21))
            self.assertEqual(android_elf.validate_library(library, 24), 21)
            for contents, error in (
                (android_library(api=28), "above the configured API 24"),
                (android_library(alignment=4096), "16 KiB"),
                (android_library(machine=62), "AArch64"),
                (android_library()[:130], "program headers"),
            ):
                library.write_bytes(contents)
                with self.assertRaisesRegex(ValueError, error):
                    android_elf.validate_library(library, 24)
            library.write_bytes(android_library(api=28))
            self.assertEqual(android_elf.validate_library(library, 28), 28)

    def test_dependency_preparation_preserves_and_requires_notices(self) -> None:
        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="android-deps-test-", dir=ROOT / ".tmp") as temporary:
            fixture = Path(temporary)
            ndk, sdl, openal = (fixture / name for name in ("ndk", "sdl", "openal"))
            sources = {
                ndk / "build/cmake/android.toolchain.cmake": "# toolchain\n",
                ndk / "NOTICE.toolchain": "C++ runtime notice\n",
                ndk / "NOTICE": "NDK notice\n",
                sdl / "CMakeLists.txt": "# SDL\n",
                sdl / "LICENSE.txt": "SDL notice\n",
                openal / "CMakeLists.txt": "# OpenAL\n",
                openal / "COPYING": "OpenAL notice\n",
                openal / "LICENSE-pffft": "PFFFT notice\n",
                openal / "fmt-11.1.3/LICENSE": "fmt notice\n",
            }
            for path, contents in sources.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents, encoding="utf-8")
            prefix = fixture / "prefix"
            runtime = android_library(api=21)
            for path in (
                ndk / "toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so",
                prefix / "lib/libSDL3.so", prefix / "lib/libopenal.so",
            ):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(runtime)
            argv = ["prepare_android_deps.py", "--ndk", str(ndk), "--sdl-source", str(sdl),
                    "--openal-source", str(openal), "--prefix", str(prefix),
                    "--build-root", str(fixture / "build"), "--api", "28"]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(prepare_android_deps.platform, "system", return_value="Windows"):
                with mock.patch.object(prepare_android_deps.subprocess, "run") as runner:
                    prepare_android_deps.main()
                # The dependency build must honor the same target API and
                # page-size settings for both independent native libraries.
                self.assertEqual(runner.call_count, 6)
                for call in (runner.call_args_list[0], runner.call_args_list[3]):
                    self.assertIn("-DANDROID_PLATFORM=android-28", call.args[0])
                    self.assertIn("-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON", call.args[0])
                for relative, expected in {
                    "licenses/sdl3/LICENSE.txt": "SDL notice\n",
                    "licenses/openal-soft/COPYING": "OpenAL notice\n",
                    "licenses/openal-soft/LICENSE-fmt": "fmt notice\n",
                    "licenses/openal-soft/LICENSE-pffft": "PFFFT notice\n",
                    "licenses/android-ndk/NOTICE.toolchain": "C++ runtime notice\n",
                }.items():
                    self.assertEqual((prefix / relative).read_text(encoding="utf-8"), expected)
                self.assertEqual((prefix / "lib/libc++_shared.so").read_bytes(), runtime)
                (sdl / "LICENSE.txt").unlink()
                with mock.patch.object(prepare_android_deps.subprocess, "run") as runner:
                    with mock.patch("sys.stderr"), self.assertRaises(SystemExit) as error:
                        prepare_android_deps.main()
                    self.assertEqual(error.exception.code, 2)
                    runner.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "Windows Meson wrapper")
    def test_windows_wrapper_refreshes_android_game_sources(self) -> None:
        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="android-wrapper-test-", dir=ROOT / ".tmp") as temporary:
            fixture = Path(temporary)
            project = fixture / "openQ4"
            game = project
            build = project / "builddir"
            scripts = project / "tools/build"
            scripts.mkdir(parents=True)
            for filename in ("meson_setup.ps1", "game_source_inventory.py"):
                shutil.copy2(ROOT / "tools/build" / filename, scripts / filename)
            for module in ("game", "mpgame"):
                (game / "src" / module).mkdir(parents=True)
                (game / "src" / module / "Game_local.cpp").write_text("// game\n", encoding="utf-8")
            (build / "meson-private").mkdir(parents=True)
            (build / "meson-info").mkdir()
            (build / "meson-private/coredata.dat").touch()
            (build / "build.ninja").touch()
            (build / "meson-info/intro-machines.json").write_text(
                json.dumps({"host": {"system": "android", "cpu_family": "aarch64"}}), encoding="utf-8")
            (build / "meson-info/intro-buildoptions.json").write_text(
                json.dumps([{"name": "build_engine", "value": True}, {"name": "build_games", "value": True}]), encoding="utf-8")
            # Record wrapper dispatch without loading MSVC or running a compiler.
            fake_meson = fixture / "python/mesonbuild"
            fake_meson.mkdir(parents=True)
            (fake_meson / "__init__.py").touch()
            log = fixture / "meson.jsonl"
            (fake_meson / "mesonmain.py").write_text(
                "import json, os, sys\n"
                "if __name__ == '__main__':\n"
                "    with open(os.environ['OPENQ4_TEST_MESON_LOG'], 'a') as output:\n"
                "        output.write(json.dumps(sys.argv[1:]) + '\\n')\n", encoding="utf-8")
            env = os.environ.copy()
            env.update({"PYTHONPATH": str(fake_meson.parent), "OPENQ4_GAMELIBS_REPO": str(game),
                        "OPENQ4_TEST_MESON_LOG": str(log)})

            def invoke() -> list[list[str]]:
                log.unlink(missing_ok=True)
                result = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                         str(scripts / "meson_setup.ps1"), "compile", "-Cbuilddir"],
                                        cwd=project, env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return [json.loads(line) for line in log.read_text().splitlines()]

            self.assertEqual([call[0] for call in invoke()], ["setup", "compile"])
            inventory_path = build / 'openq4_game_sources.json'
            generated = subprocess.run([sys.executable, str(scripts / 'game_source_inventory.py'), str(project)],
                                       check=True,capture_output=True,text=True)
            inventory_path.write_text(generated.stdout,encoding='utf-8')
            self.assertEqual([call[0] for call in invoke()], ["compile"])
            (game / "src/game/Game_local.cpp").write_text("// changed game\n", encoding="utf-8")
            self.assertEqual([call[0] for call in invoke()], ["setup", "compile"])

    def test_ndk_target_and_missing_tools(self) -> None:
        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="android-build-test-", dir=ROOT / ".tmp") as temporary:
            ndk = Path(temporary)
            toolchain = ndk / "toolchains/llvm/prebuilt/windows-x86_64"
            binaries = toolchain / "bin"
            binaries.mkdir(parents=True)
            for name in ("clang.exe", "clang++.exe", "llvm-ar.exe", "llvm-strip.exe"):
                (binaries / name).touch()
            (toolchain / "sysroot/usr/lib/aarch64-linux-android/24").mkdir(parents=True)
            contents = android_cross.generate(ndk, 24, "windows-x86_64")
            self.assertIn("--target=aarch64-linux-android24", contents)
            self.assertIn("system = 'android'", contents)
            self.assertIn("needs_exe_wrapper = true", contents)
            self.assertNotIn(".cmd", contents)
            self.assertIn("max-page-size=16384", contents)
            with self.assertRaisesRegex(ValueError, "API 24"):
                android_cross.generate(ndk, 23, "windows-x86_64")
            with self.assertRaisesRegex(ValueError, "requested arm64 API"):
                android_cross.generate(ndk, 25, "windows-x86_64")
            (binaries / "clang.exe").unlink()
            with self.assertRaisesRegex(ValueError, "Missing NDK tool"):
                android_cross.generate(ndk, 24, "windows-x86_64")

    def test_module_only_android_sources(self) -> None:
        command = [sys.executable, str(ROOT / "tools/build/meson_sources.py"),
                   "--host-system", "android", "--include-game", "false", "--renderer", "module"]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        sources = result.stdout.splitlines()
        self.assertEqual([p for p in sources if p.startswith("src/renderer/")],
                         ["src/renderer/RendererModule.cpp"])
        self.assertIn("src/sys/android/android_main.cpp", sources)
        self.assertIn("src/sys/android/android_sdl3.cpp", sources)
        self.assertFalse(any("/sys/linux/" in p or "/sys/win32/" in p for p in sources))
        for rejected in (["--platform-backend", "native"], ["--target-kind", "dedicated"]):
            result = subprocess.run(command + rejected, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Android requires the SDL3 client", result.stderr)


if __name__ == "__main__":
    unittest.main()
