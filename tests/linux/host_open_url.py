#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
import json, os, pathlib, subprocess, sys, tempfile
with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp)
    helper = root / 'xdg-open'
    helper.write_text('#!/usr/bin/env python3\nimport json,os,sys\nopen(os.environ["RESULT"],"w").write(json.dumps({"args":sys.argv[1:],"env":dict(os.environ)}))\n')
    helper.chmod(0o755)
    env = dict(os.environ, PATH=str(root)+':'+os.environ['PATH'], RESULT=str(root/'result.json'),
        KIRIKINUX_HOST_ENV_SAVED='1', KIRIKINUX_HOST_LD_LIBRARY_PATH='/host/libraries',
        KIRIKINUX_HOST_FONTCONFIG_FILE='/host/fonts.conf', LD_LIBRARY_PATH='/tmp/.mount_kirikinux/lib',
        FONTCONFIG_FILE='/tmp/.mount_kirikinux/fonts.conf', GTK_MODULES='incompatible-host-module')
    url = "https://example.org/patch?q='quote'&x=$(touch SHOULD_NOT_EXIST)"
    subprocess.run([sys.argv[1], url], env=env, check=True)
    result=json.loads((root/'result.json').read_text())
    assert result['args']==[url], result['args']
    assert result['env']['LD_LIBRARY_PATH']=='/host/libraries'
    assert result['env']['FONTCONFIG_FILE']=='/host/fonts.conf'
    assert 'GTK_MODULES' not in result['env']
    assert not any(k.startswith('KIRIKINUX_HOST_') for k in result['env'])
    print('PASS: exact URL argument, original host libraries/fonts restored, private environment removed')
