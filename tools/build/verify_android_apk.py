#!/usr/bin/env python3
"""Verify a signed Android release APK against its validated Meson staging tree."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import ssl
import subprocess
import zipfile

from android_elf import validate_library

LIBRARIES = {
    'libquake4.so', 'libSDL3.so', 'librenderer-gles_arm64.so',
    'libgame-sp_arm64.so', 'libgame-mp_arm64.so', 'libopenal.so', 'libc++_shared.so',
}


def digest(stream) -> str:
    result = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        result.update(chunk)
    return result.hexdigest()


def verify_payload(apk: Path, stage: Path, version: str) -> None:
    metadata = json.loads((stage / 'android-build.json').read_text(encoding='utf-8'))
    mod = json.loads((stage / 'baseoq4/mod.json').read_text(encoding='utf-8'))
    if metadata.get('abi') != 'arm64-v8a' or metadata.get('androidApi') != 24:
        raise ValueError('Release must target Android arm64 API 24')
    if mod.get('version') != version or mod.get('requiredopenQ4Version') != version:
        raise ValueError('Staged engine and mod versions do not match the release')
    expected = {}
    for name in LIBRARIES:
        path = stage / 'lib' / name
        validate_library(path, 24)
        expected[f'lib/arm64-v8a/{name}'] = path
    for name in ('baseoq4/pak0.pk4', 'baseoq4/pak1.pk4', 'baseoq4/mod.json',
                 'licenses/sdl3/LICENSE.txt', 'licenses/openal-soft/COPYING',
                 'licenses/android-ndk/NOTICE.toolchain'):
        if not (stage / name).is_file():
            raise ValueError(f'Missing required packaged file: {name}')
    for directory in ('baseoq4', 'licenses'):
        for path in (stage / directory).rglob('*'):
            if path.is_file():
                expected['assets/' + path.relative_to(stage).as_posix()] = path
    for name in ('LICENSE', 'CONTRIBUTORS.md', 'android-build.json'):
        expected['assets/' + name] = stage / name
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('APK contains duplicate entries')
        native = {name for name in names if name.startswith('lib/') and not name.endswith('/')}
        if native != {name for name in expected if name.startswith('lib/')}:
            raise ValueError('APK must contain exactly the seven matching arm64 native libraries')
        assets = {name for name in names if name.startswith('assets/') and not name.endswith('/')}
        if assets != {name for name in expected if name.startswith('assets/')} | {'assets/openq4-package-version.txt'}:
            raise ValueError('APK asset set differs from the staged openQ4 overlays and notices')
        for name, path in expected.items():
            with archive.open(name) as packaged, path.open('rb') as staged:
                if digest(packaged) != digest(staged):
                    raise ValueError(f'APK payload differs from validated stage: {name}')
        package_digest = hashlib.sha256()
        for path in sorted((stage / 'baseoq4').rglob('*')):
            if path.is_file():
                package_digest.update(path.relative_to(stage / 'baseoq4').as_posix().encode())
                with path.open('rb') as source:
                    while chunk := source.read(65536):
                        package_digest.update(chunk)
        if archive.read('assets/openq4-package-version.txt').decode() != package_digest.hexdigest():
            raise ValueError('APK extraction revision does not match its packaged data')
        if archive.testzip() is not None:
            raise ValueError('APK ZIP integrity failure')


def verify_tools(apk: Path, tools: Path, certificate: Path, version: str) -> None:
    def run(tool: str, *args: str) -> str:
        suffix = '.bat' if tool == 'apksigner' and (tools / 'apksigner.bat').exists() else ''
        result = subprocess.run([str(tools / (tool + suffix)), *args], check=True, capture_output=True, text=True)
        return result.stdout

    signature = run('apksigner', 'verify', '--verbose', '--print-certs', str(apk))
    expected_cert = hashlib.sha256(ssl.PEM_cert_to_DER_cert(certificate.read_text())).hexdigest()
    actual_certs = re.findall(r'Signer #\d+ certificate SHA-256 digest: ([0-9a-fA-F]+)', signature)
    if actual_certs != [expected_cert] or not re.search(r'Verified using v2 scheme.*: true', signature):
        raise ValueError('APK must have a valid v2 signature from the pinned release certificate')
    run('zipalign', '-c', '-P', '16', '4', str(apk))
    badging = run('aapt', 'dump', 'badging', str(apk))
    code = sum(int(part) * scale for part, scale in zip(version.split('.'), (1000000, 1000, 1)))
    for token in ("package: name='com.darkmatter.openq4'", f"versionCode='{code}'", f"versionName='{version}'",
                  "sdkVersion:'24'", "targetSdkVersion:'35'", "native-code: 'arm64-v8a'"):
        if token not in badging:
            raise ValueError(f'APK manifest mismatch: {token}')
    if 'application-debuggable' in badging:
        raise ValueError('Release APK must not be debuggable')
    manifest = run('aapt', 'dump', 'xmltree', str(apk), 'AndroidManifest.xml')
    if not re.search(r'android:extractNativeLibs\([^)]*\)=\(type 0x12\)0xffffffff', manifest):
        raise ValueError('APK must extract native libraries for runtime module loading')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--build-tools', type=Path, required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'\d{1,3}\.\d{1,3}\.\d{1,3}', args.version):
        parser.error('Invalid release version')
    verify_tools(args.apk, args.build_tools, args.certificate, args.version)
    verify_payload(args.apk, args.stage, args.version)
    print('Android APK verified: release certificate, manifest, 16 KiB ELF/ZIP alignment, exact native/data/license payload.')


if __name__ == '__main__':
    main()
