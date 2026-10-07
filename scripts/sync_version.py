#!/usr/bin/env python3
"""Keep derived metadata in step with PLUG_VERSION_STR in config.h (STR always wins).

Run automatically before every build (see VaiaSyncVersion target in config/<Name>-win.props).
Idempotent: files are only rewritten when something actually changes, so it never causes
needless rebuilds. Never fails the build (exit code is always 0).

Derived from PLUG_VERSION_STR "MAJOR.MINOR.PATCH":
  config.h        PLUG_VERSION_HEX        0xMMMMmmpp
                  PLUG_COPYRIGHT_STR      "<PLUG_MFR_ID> (c) <current year>"
  resources/main.rc  FILEVERSION / PRODUCTVERSION, FileVersion, ProductVersion,
                     CompanyName (= PLUG_MFR_ID text), LegalCopyright (= PLUG_COPYRIGHT_STR)
  installer/*.iss    AppVersion, VersionInfoVersion
"""
import datetime
import glob
import os
import re
import sys


def read(path):
    raw = open(path, 'rb').read()
    if raw.startswith(b'\xff\xfe') or raw.startswith(b'\xfe\xff'):
        enc = 'utf-16'
    elif raw.startswith(b'\xef\xbb\xbf'):
        enc = 'utf-8-sig'
    else:
        try:
            raw.decode('utf-8')
            enc = 'utf-8'
        except UnicodeDecodeError:
            enc = 'cp1252'
    return raw.decode(enc), enc


def write_if_changed(path, old, new, enc):
    if new == old:
        return False
    tmp = path + '.tmp_sync'
    data = new.encode(enc)
    open(tmp, 'wb').write(data)
    os.replace(tmp, path)  # atomic: parallel project builds never see a half-written file
    return True


def define(text, name):
    m = re.search(r'^[ \t]*#define[ \t]+' + name + r'[ \t]+"([^"]*)"', text, re.M)
    return m.group(1) if m else None


def main():
    project = os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), os.pardir))
    cfg = next((p for p in (os.path.join(project, 'config.h'), os.path.join(project, 'src', 'config.h')) if os.path.exists(p)), None)
    if not cfg:
        print('VaiaSync: no config.h found, skipping')
        return
    text, enc = read(cfg)
    ver = define(text, 'PLUG_VERSION_STR')
    m = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)', ver or '')
    if not m:
        print('VaiaSync: PLUG_VERSION_STR must look like "1.2.3" (got %r), skipping' % ver)
        return
    maj, mnr, pat = (int(x) for x in m.groups())
    if maj > 0xFFFF or mnr > 0xFF or pat > 0xFF:
        print('VaiaSync: version component out of range (major<=65535, minor/patch<=255), skipping')
        return
    hexv = '0x%04X%02X%02X' % (maj, mnr, pat)
    mfr = (re.search(r"#define[ \t]+PLUG_MFR_ID[ \t]+'(.{4})'", text) or [None, 'Vaia'])[1]  # 'Vaia'
    copyright_str = '%s (c) %d' % (mfr, datetime.date.today().year)

    new = re.sub(r'(^[ \t]*#define[ \t]+PLUG_VERSION_HEX[ \t]+)0x[0-9A-Fa-f]+', lambda mo: mo.group(1) + hexv, text, flags=re.M)
    new = re.sub(r'(^[ \t]*#define[ \t]+PLUG_COPYRIGHT_STR[ \t]+")[^"]*(")', lambda mo: mo.group(1) + copyright_str + mo.group(2), new, flags=re.M)
    changed = []
    if write_if_changed(cfg, text, new, enc):
        changed.append('config.h')

    rc = os.path.join(project, 'resources', 'main.rc')
    if os.path.exists(rc):
        t, renc = read(rc)
        n = t
        n = re.sub(r'(^[ \t]*FILEVERSION[ \t]+)[\d, ]+', lambda mo: mo.group(1) + '%d,%d,%d,0' % (maj, mnr, pat), n, flags=re.M)
        n = re.sub(r'(^[ \t]*PRODUCTVERSION[ \t]+)[\d, ]+', lambda mo: mo.group(1) + '%d,%d,%d,0' % (maj, mnr, pat), n, flags=re.M)
        for key, val in (('FileVersion', ver), ('ProductVersion', ver), ('CompanyName', mfr), ('LegalCopyright', copyright_str)):
            n = re.sub(r'(VALUE[ \t]+"%s",[ \t]*")[^"]*(")' % key, lambda mo, v=val: mo.group(1) + v + mo.group(2), n)
        if write_if_changed(rc, t, n, renc):
            changed.append('resources/main.rc')

    for iss in sorted(glob.glob(os.path.join(project, 'installer', '*.iss'))):
        t, ienc = read(iss)
        n = re.sub(r'(^[ \t]*(?:AppVersion|VersionInfoVersion)[ \t]*=[ \t]*)[^\r\n]*', lambda mo: mo.group(1) + ver, t, flags=re.M)
        if write_if_changed(iss, t, n, ienc):
            changed.append('installer/' + os.path.basename(iss))

    if changed:
        print('VaiaSync: %s -> version %s (HEX %s), "%s"' % (', '.join(changed), ver, hexv, copyright_str))


if __name__ == '__main__':
    try:
        main()
    except Exception as e:  # never break a build over metadata
        print('VaiaSync: skipped (%s)' % e)
    sys.exit(0)
