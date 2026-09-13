"""在已激活的原生 ESP-IDF 终端运行：python tools/verify_builds.py。

重复使用一个专用验证目录以复用 SDK 编译缓存；不烧录、不修改 SDK 安装。
每次构建的完整日志和结果保存在 build/validation，遇到失败立即返回非零。
"""
from pathlib import Path
import json
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sdk = os.environ.get("IDF_PATH")
if not sdk:
    sys.exit("Open an ESP-IDF terminal first (IDF_PATH is missing).")

out = root / "build" / "validation"
out.mkdir(parents=True, exist_ok=True)
results = []
# 0 检查基础入口；17 链接完整外设栈；99 链接模型边界测试。
for lesson in (0, 17, 99):
    command = [sys.executable, str(Path(sdk) / "tools" / "idf.py"),
               "-B", str(out), f"-DCOURSE_LESSON={lesson}", "build"]
    log = out / f"lesson{lesson}.log"
    print(f"Building lesson {lesson}; log: {log}", flush=True)
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
    results.append({"lesson": lesson, "exit_code": result.returncode, "log": str(log)})
    (out / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    if result.returncode:
        print(log.read_text(encoding="utf-8", errors="replace")[-8000:])
        sys.exit(result.returncode)
    # 这是编译/链接成功，不代表模型自检已运行或真实硬件已通过验收。
    print(f"Lesson {lesson}: build passed", flush=True)
print("All three firmware build checks passed. Hardware execution is still required.")
