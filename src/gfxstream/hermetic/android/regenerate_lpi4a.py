#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--meson', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[4]
with tempfile.TemporaryDirectory(prefix='lpi4a-mesa-soong-') as temporary:
    output = Path(temporary)
    subprocess.run([sys.executable, str(args.meson.resolve()), 'convert', 'android', 'aosp_mesa3d',
                    '--project-dir', str(root), '--output-dir', str(output)], check=True)
    files = sorted(output.rglob('Android.bp'))
    if not files:
        raise RuntimeError('No Android.bp output')
    for file in files:
        relative = file.relative_to(output)
        if str(relative).startswith(('src/asahi/', 'src/gallium/drivers/asahi/')):
            raise RuntimeError(str(relative))
        content = file.read_text()
        if relative == Path('Android.bp'):
            for name in ('aosp_mesa3d_c_project_args', 'aosp_mesa3d_cpp_project_args'):
                token = '    name: "' + name + '",\n'
                if content.count(token) != 1:
                    raise RuntimeError(name)
                content = content.replace(token, token + '    lto: { never: true },\n')
            def rust_arch_flags(match):
                entries = re.findall(r'"(\w+)": (\[[^\]]*\]),', match[1])
                if not entries or 'default: []' not in match[1]:
                    raise RuntimeError(match[0])
                return '    arch: {\n' + ''.join(
                    '        ' + arch + ': { flags: ' + flags + ' },\n'
                    for arch, flags in entries) + '    },'
            content, count = re.subn(
                r'    flags: select\(arch\(\), \{(.*?)\}\),',
                rust_arch_flags, content, flags=re.S)
            if count != 2:
                raise RuntimeError(('rust_arch_flags', count))
        target = root/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        for original in sorted(set(re.findall(r'"([A-Za-z_]\w*\.[A-Za-z_]\w*\.py)"', content))):
            alias = original[:-3].replace('.', '_') + '.py'
            link = target.parent/alias
            if not (target.parent/original).is_file():
                raise RuntimeError(original)
            if link.is_symlink():
                if str(link.readlink()) != original:
                    raise RuntimeError(link)
            elif link.exists():
                raise RuntimeError(link)
            else:
                link.symlink_to(original)
            content = content.replace('"' + original + '"', '"' + alias + '"')
        target.write_text(content)
    print('SOONG_FILES_GENERATED', len(files))
