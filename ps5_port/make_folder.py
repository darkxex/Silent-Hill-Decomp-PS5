#!/usr/bin/env python3
import json, os, shlex, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PS5 = Path(os.environ.get("PS5_ROOT", Path.home() / "PS5"))
REPO = HERE.parent
BUILD = Path(os.environ.get("BUILD_DIR", REPO / "build/ps5"))
GL = PS5 / "ps5-opengl"
SDK_GL = PS5 / "ps5-opengl-sdk-dl/ps5-opengl-sdk-1.0.2/sdk"
TEMPLATE = PS5 / "ps5-native-app-boilerplate"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "build/ps5-folder"
TITLE_ID = "PPSA98542"


def run(*args, **kw):
    return subprocess.run([str(a) for a in args], check=True, **kw)


def link_inputs():
    line = run("ninja", "-C", BUILD, "-t", "commands", "SilentHillPC",
               capture_output=True, text=True).stdout.strip().splitlines()[-1]
    objs, archives = [], []
    for tok in shlex.split(line):
        if tok.endswith((".o", ".obj")) and not tok.startswith("-"):
            objs.append(BUILD / tok)
        elif tok.endswith(".a"):
            archives.append(Path(tok) if tok.startswith("/") else BUILD / tok)
    return objs, archives


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    objs, archives = link_inputs()
    OUT.mkdir(parents=True)
    for d in ("runtime", "sce_sys", "tooling", "tools", ".deps/native"):
        shutil.copytree(TEMPLATE / d, OUT / d)
    sdk = OUT / ".deps/native/ps5-payload-sdk"
    for d in ("src", "vendor", "build/native-imports"):
        (OUT / d).mkdir(parents=True)
    for n in ("runtime_shims.c", "app_heap.c"):
        shutil.copy2(GL / "native-app" / n, OUT / "src" / n)
    shutil.copy2(OUT / "tooling/native/ps5-pie.ld", OUT / "tooling/native/ps5-pie-base.ld")
    for n in ("ps5-pie.ld", "app-symbols.map"):
        shutil.copy2(GL / "native-app" / n, OUT / "tooling/native" / n)

    param = json.loads((GL / "native-app/param.json").read_text())
    param["titleId"] = TITLE_ID
    param["conceptId"] = TITLE_ID[4:]
    param["contentId"] = f"UP9000-{TITLE_ID}_00-SILENTHILLPS5000"
    param["localizedParameters"]["en-US"]["titleName"] = "Silent Hill"
    (OUT / "sce_sys/param.json").write_text(json.dumps(param, indent=2) + "\n")

    writer = OUT / "tooling/native/sce_module_writer.cpp"
    old = "write_u64(result.data, result.heap_size, std::numeric_limits<std::uint64_t>::max());"
    t = writer.read_text()
    assert t.count(old) == 1
    writer.write_text(t.replace(old, "write_u64(result.data, result.heap_size, 0x10000000ULL);"))
    script = OUT / "tools/build.sh"
    t = script.read_text()
    for a, b in (('bash "$root/tools/setup-native-dependencies.sh" >/dev/null',
                  'test -x "$root/.deps/native/ps5-payload-sdk/bin/prospero-lld"'),
                 ('[[ -f $root/runtime/libc.prx ]] || bash "$root/tools/rebuild-libc.sh"',
                  'test -f "$root/runtime/libc.prx"'),
                 ("--eh-frame-hdr \\",
                  "--eh-frame-hdr --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free "
                  "--wrap=posix_memalign --wrap=malloc_usable_size "
                  "--wrap=fopen --wrap=open --wrap=stat --wrap=lstat --wrap=access --wrap=opendir "
                  "--wrap=mkdir --wrap=remove --wrap=unlink --wrap=rmdir --wrap=rename --wrap=getcwd --wrap=exit --wrap=_Exit \\")):
        assert t.count(a) == 1, a
        t = t.replace(a, b)
    script.write_text(t)

    env = os.environ.copy()
    env["PS5_PAYLOAD_SDK"] = str(sdk)
    for n in ("libSceAgc.so", "libSceAgcDriver.so"):
        shutil.copy2(SDK_GL / "lib" / n, sdk / "target/lib" / n)
    rt = Path(run("clang-18", "--print-resource-dir", capture_output=True,
                  text=True).stdout.strip()) / "lib/linux/libclang_rt.builtins-x86_64.a"

    # Map overlays rely on being force-loaded, so their members go in as plain objects.
    extra = OUT / "vendor/objs"
    extra.mkdir()
    members = []
    for i, a in enumerate(archives):
        if a.name.startswith("libmap") or a.name == "libchara_global.a":
            d = extra / f"{i}_{a.stem}"
            d.mkdir()
            run("ar", "x", a.resolve(), cwd=d)
            members += sorted(d.glob("*.o*"))
    libs = list(objs) + members
    libs += [a for a in archives if not (a.name.startswith("libmap") or a.name in
             ("libchara_global.a", "libSDL2.a", "libopenal.a"))]
    sdl = GL.parent / "sdl4k/build/native-build-audio/sdk/lib/libSDL2.a"
    libs += [sdl, SDK_GL / "lib/libPS5OpenGLCore33.a"]
    libs += [sdk / f"target/lib/{n}" for n in ("libunwind.a", "libc++abi.a", "libc++.a")] + [rt]
    for p in libs:
        if not Path(p).is_file():
            raise SystemExit(f"missing link input: {p}")
    group = (f'SEARCH_DIR("{sdk}/target/lib")\nSEARCH_DIR("{SDK_GL}/lib")\n'
             'EXTERN(ps5_agc_gate2_run)\nGROUP (\n' + "".join(f'  "{p}"\n' for p in libs) + ')\n')
    (OUT / "vendor/libsh.a").write_text(group)
    env["APP_STATIC_ARCHIVES"] = "vendor/libsh.a"
    for k in ("APP_DEFINITIONS", "APP_INCLUDE_PATHS", "PACBREW_PACKAGES", "PACBREW_INCLUDE_PATHS",
              "PACBREW_STATIC_ARCHIVES", "APP_RUNTIME_MODULES"):
        env[k] = ""
    run("bash", script, "Folder", env=env, cwd=OUT)
    app = OUT / "dist" / TITLE_ID
    helper = OUT / "lapy-helper"
    run("python3", HERE / "tools/build-lapy-helper.py", TITLE_ID, helper)
    shutil.copy2(helper / "lapy.elf", app / "lapy.elf")
    shutil.copy2(helper / "lapy-manifest.json", app / "lapy-manifest.json")
    (app / "licenses").mkdir(exist_ok=True)
    shutil.copy2(helper / "Lapy-MIT.txt", app / "licenses/Lapy-MIT.txt")
    final = REPO / "dist" / TITLE_ID
    if final.exists():
        shutil.rmtree(final)
    final.parent.mkdir(exist_ok=True)
    shutil.copytree(app, final)
    shutil.copy2(OUT / "dist" / f"{TITLE_ID}.zip", REPO / "dist" / f"{TITLE_ID}.zip")
    print("folder:", final)


if __name__ == "__main__":
    main()
