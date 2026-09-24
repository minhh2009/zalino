import requests
import time
import os, shutil
import subprocess
import glob
import tarfile

ARCH = "x64"

DOWNLOAD_PATH = "tmp"

ELECTRON_VERSION = "22.3.27"
ELECTRON_FILE_NAME = f"electron-v{ELECTRON_VERSION}-linux-{ARCH}.zip"
ELECTRON_DOWNLOAD_LINK = f"https://github.com/electron/electron/releases/download/v{ELECTRON_VERSION}/{ELECTRON_FILE_NAME}"

ELECTRON_DIR = os.path.join(DOWNLOAD_PATH, "electron")

ZALO_DOWNLOAD = "https://zalo.me/download/zalo-pc?utm=90000"
ZALO_DIR = os.path.join(DOWNLOAD_PATH, "zalo")

SQLITE3_VERSION = "6.0.1"
SQLITE3_FILE_NAME = f"sqlite3-v{SQLITE3_VERSION}-napi-v6-linux-{ARCH}.tar.gz"
SQLITE3_DOWNLOAD_LINK =  f"https://github.com/TryGhost/node-sqlite3/releases/download/v{SQLITE3_VERSION}/{SQLITE3_FILE_NAME}"

DB_CROSS_DIR = "db-cross-build"
headers = {
    "User-Agent": (
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"
    )
}

def format_time(secs):
    secs = int(secs)

    hours, secs = divmod(secs, 3600)
    mins, secs = divmod(secs, 60)

    if hours:
        return f"{hours}h {mins:02d}m {secs:02d}s"

    if mins:
        return f"{mins}m {secs:02d}s"

    return f"{secs}s"


def run_command(cmd, cwd=None):
    print()
    print("$", " ".join(map(str, cmd)))

    process = subprocess.Popen(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    if process.stdout:
        for line in process.stdout:
            print(line, end="")

    code = process.wait()

    if code != 0:
        raise RuntimeError(
            f"Command failed with exit code {code}: "
            f"{' '.join(map(str, cmd))}"
        )
def down_file(url, out_name, out_path, chunk_size=1024 * 64):
    res = requests.get(url, stream=True)
    res.raise_for_status()

    total = int(res.headers.get("content-length", 0))
    downd = 0

    start_time= time.time()
    ltf = start_time
    ldownd = 0

    while True:
        chunk = res.raw.read(chunk_size)
        if not chunk:
            break

        with open(f"{out_path}/{out_name}", "ab") as f:
            f.write(chunk)

        downd += len(chunk)

        now = time.time()
        elapsed = now - start_time

        interval = now - ltf
        if interval > 0:
            spd = (downd - ldownd) / interval
        else:
            spd = 0

        avg_spd = downd / elapsed if elapsed > 0 else 0

        if total and avg_spd > 0:
            rem = (total - downd) / avg_spd
        else:
            rem = 0

        if total:
            pct = downd / total * 100
            print(
                f"\r{out_name} | "
                f"{pct:6.2f}% | "
                f"{downd / 1024 / 1024:.2f}/"
                f"{total / 1024 / 1024:.2f} MB | "
                f"{spd / 1024 / 1024:.2f} MB/s | "
                f"avg {avg_spd / 1024 / 1024:.2f} MB/s | "
                f"ETA {format_time(rem)}",
                end="",
                flush=True,
            )
        else:
            print(
                f"\r{out_name} | "
                f"{downd / 1024 / 1024:.2f} MB | "
                f"{spd / 1024 / 1024:.2f} MB/s | "
                f"avg {avg_spd / 1024 / 1024:.2f} MB/s",
                end="",
                flush=True,
            )

        ltf = now
        ldownd = downd

    print(f"\nTải xong {out_name}.")
    return os.path.join(out_path, out_name)

def extract(archive_path, extract_path):
    os.makedirs(extract_path, exist_ok=True)

    print(f"Giải nén {os.path.basename(archive_path)}")

    process = subprocess.Popen(
        [
            "7z",
            "x",
            archive_path,
            f"-o{extract_path}", 
            "-y",
            "-bsp1",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    for line in process.stdout:
        print(line, end="")

    return_code = process.wait()

    if return_code != 0:
        print()
        raise RuntimeError(f"7z extraction failed: {archive_path}")

def check_asar():
    try:
        subprocess.run(
            [
                "npx",
                "@electron/asar",
                "--version",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError ):
        raise RuntimeError( "where's asar bro?" )


def extract_asar( asar_path,output_dir):
    print("Extract app.asar")

    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)

    run_command(
        [
            "npx",
            "@electron/asar",
            "extract",
            asar_path,
            output_dir,
        ]
    )


def pack_asar(source_dir,asar_path,):
    print("Pack app.asar")

    if os.path.exists(asar_path):
        os.remove(asar_path)

    run_command(
        [
            "npx",
            "@electron/asar",
            "pack",
            source_dir,
            asar_path,
        ]
    )


def extract_tar_gz(file_path,out_path):
    if os.path.exists(out_path):
        shutil.rmtree(out_path)

    os.makedirs(
        out_path,
        exist_ok=True,
    )

    with tarfile.open(file_path, "r:gz") as archive:
        root = os.path.realpath(out_path)

        for member in archive.getmembers():
            target = os.path.realpath(
                os.path.join(out_path, member.name)
            )

            if os.path.commonpath([root, target]) != root:
                raise RuntimeError(f"Unsafe tar member: {member.name}")

        archive.extractall(out_path )

def find_zalo_resources(zalo_extract_path,):
    matches = glob.glob(os.path.join(zalo_extract_path,"*-universal",))

    for path in matches:
        if not os.path.isdir(path):
            continue

        if not os.path.basename(path).startswith("Zalo "):
            continue

        resources = os.path.join(path, "Zalo.app", "Contents", "Resources",)

        if os.path.isdir(resources):
            return resources

    raise FileNotFoundError("uh where are resources")


def install_native_sqlite3(sqlite3_extract_path,app_source_dir):

    binaries = glob.glob(
        os.path.join( sqlite3_extract_path,"**", "node_sqlite3.node"),
        recursive=True,
    )

    if len(binaries) != 1:
        raise FileNotFoundError("uh how???")

    source = binaries[0]

    destination_dir = os.path.join(
        app_source_dir,
        "native",
        "nativelibs",
        "sqlite3",
        "binding",
        f"napi-v6-linux-{ARCH}",
    )

    os.makedirs(
        destination_dir,
        exist_ok=True,
    )

    moveto = os.path.join(destination_dir, "node_sqlite3.node" )

    print("Installing sqlite3")

    shutil.copy2(source, moveto)

def build_db_cross_v4(app_source_dir):
    run_command(["./db-cross-build/build.sh"])
    db_cross_build_dir = os.path.join(DB_CROSS_DIR, "build", "Release")
    db_cross_lib_dir = os.path.join(app_source_dir, "native", "nativelibs", "db-cross-v4")
    db_cross_native_node_dir = os.path.join(db_cross_lib_dir, "prebuilt", "linux", "electron", ARCH)
    os.makedirs(db_cross_native_node_dir)
    shutil.copy2(os.path.join(db_cross_build_dir, "db-cross-v4-native.node"), db_cross_native_node_dir)

    with open(os.path.join(db_cross_lib_dir, "dist", "binding.js"), "w") as f:
        f.write(r""""use strict";
// @ts-ignore
let addon;
if (process.platform === 'darwin') {
    addon = require(`../prebuilt/darwin/electron/${process.arch}/db-cross-v4-native.node`);
}
else if (process.platform=== 'linux') {
    addon = require(`../prebuilt/linux/electron/${process.arch}/db-cross-v4-native.node`);
}
else {
    if (process.arch === 'x64') {
        addon = require('../prebuilt/window/electron_x86_64/db-cross-v4-native.node');
    }
    else {
        addon = require('../prebuilt/window/electron_x86/db-cross-v4-native.node');
    }
}
module.exports = addon;""")

def main():

    if os.path.exists(DOWNLOAD_PATH):
        shutil.rmtree(DOWNLOAD_PATH)

    os.makedirs(DOWNLOAD_PATH,exist_ok=True)
    check_asar()

    print("Getting URL...")

    res = requests.get(ZALO_DOWNLOAD,headers=headers, timeout=30, allow_redirects=False,)

    res.raise_for_status()

    zalo_url = res.headers.get("Location")

    if not zalo_url:
        raise RuntimeError("url not found")

    electron_zip = down_file(ELECTRON_DOWNLOAD_LINK, ELECTRON_FILE_NAME, DOWNLOAD_PATH)
    zalo_zip = down_file(zalo_url,"ZaloSetup.dmg", DOWNLOAD_PATH)


    sql3_zip = down_file(SQLITE3_DOWNLOAD_LINK,SQLITE3_FILE_NAME, DOWNLOAD_PATH)


    
    extract(electron_zip, ELECTRON_DIR)

    
    extract( zalo_zip,ZALO_DIR)

    sqlite3_dir = os.path.join( DOWNLOAD_PATH,"sqlite3")
    extract_tar_gz(sql3_zip, sqlite3_dir)

    electron_res = os.path.join(ELECTRON_DIR, "resources",)

    if not os.path.isdir(electron_res):
        raise FileNotFoundError(electron_res)

    zalo_res = find_zalo_resources(ZALO_DIR)

    original_asar = os.path.join(zalo_res,"app.asar",)

    if not os.path.isfile( original_asar):
        raise FileNotFoundError( original_asar)

    default_app = os.path.join(electron_res,"default_app.asar",)

    if os.path.exists(default_app ):
        print("Removing default_app.asar" )
        os.remove( default_app)


    app_source_dir = os.path.join( DOWNLOAD_PATH, "zalo-app")

    extract_asar( original_asar,app_source_dir)
    install_native_sqlite3(sqlite3_dir, app_source_dir)
    build_db_cross_v4(app_source_dir)

    output_asar = os.path.join(electron_res,"app.asar")

    pack_asar(app_source_dir,output_asar,)

    print("DONE")


if __name__ == "__main__":
    main()