import requests
import time
import os, shutil
import subprocess
import glob
def format_time(secs):
    secs =int(secs)

    hs, secs = divmod(secs, 3600)
    mins, secs = divmod(secs, 60)
    if hs:
        return f"{hs}h {mins:02d}m {secs:02d}s"
    if mins:
        return f"{mins}m {secs:02d}s"
    return f"{secs}s"

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

ARCH = "x64"

DOWNLOAD_PATH = "tmp"

ELECTRON_VERSION = "v22.3.27"
ELECTRON_FILE_NAME = f"electron-{ELECTRON_VERSION}-linux-{ARCH}.zip"
ELECTRON_DOWNLOAD_LINK = f"https://github.com/electron/electron/releases/download/{ELECTRON_VERSION}/{ELECTRON_FILE_NAME}"

ZALO_DOWNLOAD = "https://zalo.me/download/zalo-pc?utm=90000"

headers = {
    "User-Agent": (
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"
    )
}

if os.path.exists(DOWNLOAD_PATH):
    shutil.rmtree(DOWNLOAD_PATH)

os.mkdir(DOWNLOAD_PATH)

print("Lấy link download ...")

response = requests.get(
    ZALO_DOWNLOAD,
    headers=headers,
    timeout=30,
    allow_redirects=False,
)

response.raise_for_status()

zalo_down_fl= response.headers.get("Location")

down_file(ELECTRON_DOWNLOAD_LINK, ELECTRON_FILE_NAME, DOWNLOAD_PATH)
down_file(zalo_down_fl, "ZaloSetup.dmg", DOWNLOAD_PATH)

extract(f"{DOWNLOAD_PATH}/{ELECTRON_FILE_NAME}", f"{DOWNLOAD_PATH}/electron")
extract(f"{DOWNLOAD_PATH}/ZaloSetup.dmg", f"{DOWNLOAD_PATH}/zalo")

ELECTRON_DIR = f"{DOWNLOAD_PATH}/electron/resources"

matches = glob.glob(f"{DOWNLOAD_PATH}/zalo/*-universal")

ZALO_DIR = ""
for path in matches:
    if os.path.isdir(path) and os.path.basename(path).startswith("Zalo ") :
        ZALO_DIR = f"{path}/Zalo.app/Contents/Resources"
        break

os.remove(f"{ELECTRON_DIR}/default_app.asar")

FLF_CP = ["app.asar", "app-update.yml", "app.asar.unpacked"]

for d in FLF_CP:
    shutil.move(f"{ZALO_DIR}/{d}", ELECTRON_DIR)