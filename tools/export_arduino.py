"""Generate/check a standalone Arduino IDE sketch from the canonical sources."""
from pathlib import Path
import argparse

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
dest = root / 'arduino' / 'AirDropUGV'
files = {
    'AirDropUGV.ino': '// Open this sketch in Arduino IDE. Implementation: controller.cpp\n',
    'controller.cpp': (root / 'src' / 'main.cpp').read_text(),
    'config.h': (root / 'include' / 'config.h').read_text(),
    'mission.h': (root / 'include' / 'mission.h').read_text(),
}
for name, content in files.items():
    target = dest / name
    if args.check:
        if not target.exists() or target.read_text() != content:
            raise SystemExit(f'Out-of-date Arduino export: {target}; rerun tools/export_arduino.py')
    else:
        dest.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
print('Arduino IDE export matches canonical sources.' if args.check else 'Arduino IDE sketch exported.')
