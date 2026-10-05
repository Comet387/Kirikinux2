# External game fixtures

The Linux port keeps real game archives outside this repository. This avoids
silently redistributing game assets and keeps ordinary source archives small.

## fork3 `data.xp3`

Source repository:
`https://github.com/weimingtom/kirikiroid2_fork3/tree/master/_testdata`

Expected file:
`_testdata/data.xp3`

Expected size: `14396068` bytes

Expected SHA-256:
`15f008b19fbd10d08002c8419670c0d7c98d03958047f2ded9dc3f1e8c58b02b`

At reference commit `9f84364f47215ce9d352a1029879fdeca14f2653`, the
archive contains 181 entries. With TJS regular expressions enabled, the
current compatibility host loads `Config.tjs`, `UpdateConfig.tjs`, and KAG
System, then stops at `system/LayerEx.tjs:39` because no native `Layer` class is
registered yet.

Place or clone the fixture anywhere and pass its path to the external XP3
regression runner. The fixture itself is intentionally not committed here.

Direct invocation:

```sh
python3 linux/tests/run_external_xp3.py \
  --binary build-linux/kirikinux-linux \
  --xp3 ../kirikinux_fork3/_testdata/data.xp3 \
  --minimum layer
```

CMake/CTest invocation:

```sh
cmake -S linux -B build-linux \
  -DKRKR2_EXTERNAL_XP3="$PWD/../kirikinux_fork3/_testdata/data.xp3"
cmake --build build-linux
ctest --test-dir build-linux -L external --output-on-failure
```

When Oniguruma is unavailable, CMake automatically uses `regexp` as the
minimum boundary. When regular-expression support is built, it requires the
later `LayerEx.tjs:39` boundary. A successful game startup always passes.
