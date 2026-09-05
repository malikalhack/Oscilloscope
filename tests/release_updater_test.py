#!/usr/bin/env python3
"""Regression coverage for release metadata updates in standalone tests."""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    source_directory = Path(sys.argv[1]).resolve()
    expected_version = "9.8.7"
    result = 1

    with tempfile.TemporaryDirectory() as temporary_directory:
        project_directory = Path(temporary_directory) / "project"
        project_directory.mkdir()
        for path in (
            "CMakeLists.txt",
            "make_release.py",
            "app",
            "capture",
            "docs",
            "tests/raw_packet_queue_test.cpp",
            "usb",
        ):
            source_path = source_directory / path
            destination_path = project_directory / path
            if source_path.is_dir():
                shutil.copytree(source_path, destination_path)
            else:
                destination_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_path, destination_path)

        completed_process = subprocess.run(
            [
                sys.executable,
                "make_release.py",
                expected_version,
                "--skip-build",
                "--skip-readme",
            ],
            cwd=project_directory,
            check=False,
        )
        test_source = project_directory / "tests/raw_packet_queue_test.cpp"
        expected_header = " * @version {}".format(expected_version)

        if (
            (completed_process.returncode == 0) and
            (expected_header in test_source.read_text(encoding="utf-8"))
        ):
            result = 0

    return result


if __name__ == "__main__":
    sys.exit(main())