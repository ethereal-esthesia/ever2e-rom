#!/usr/bin/env python3
"""Assemble a standalone ca65 source and emit Applesoft BASIC DATA lines."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import tempfile


def parse_int(value: str) -> int:
    value = value.strip().lower()
    if value.startswith("0x"):
        return int(value, 16)
    if value.startswith("$"):
        return int(value[1:], 16)
    return int(value, 10)


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def main() -> int:
    p = argparse.ArgumentParser(description="Assemble ca65 source and emit BASIC DATA lines.")
    p.add_argument("--asm", required=True, help="Input .asm file (standalone source)")
    p.add_argument("--start", default="0x300", help="Load/start address for CODE segment (default: 0x300)")
    p.add_argument("--chunk", type=int, default=16, help="Bytes per DATA line (default: 16)")
    p.add_argument("--line-start", type=int, default=None, help="First BASIC DATA line number")
    p.add_argument(
        "--basic-start",
        type=int,
        default=None,
        help="Optional BASIC line number for a heading line: REM <filename> DATA",
    )
    p.add_argument("--line-step", type=int, default=10, help="BASIC line increment (default: 10)")
    p.add_argument("--sentinel", type=int, default=None, help="Optional trailing sentinel value (example: -1)")
    p.add_argument("--out", default="-", help="Output file path, or '-' for stdout")
    p.add_argument("--ca65", default="ca65", help="ca65 binary path")
    p.add_argument("--ld65", default="ld65", help="ld65 binary path")
    args = p.parse_args()

    asm_path = pathlib.Path(args.asm).resolve()
    if not asm_path.exists():
        raise SystemExit(f"error: asm file not found: {asm_path}")

    start = parse_int(args.start)
    if not (0 <= start <= 0xFFFF):
        raise SystemExit("error: --start must be in 0..0xFFFF")
    if args.chunk <= 0:
        raise SystemExit("error: --chunk must be > 0")

    with tempfile.TemporaryDirectory(prefix="asm_data_") as td:
        td_path = pathlib.Path(td)
        obj_path = td_path / "input.o"
        bin_path = td_path / "out.bin"
        cfg_path = td_path / "link.cfg"

        ram_size = 0x10000 - start
        if ram_size <= 0:
            raise SystemExit("error: --start leaves no room for output")

        cfg_path.write_text(
            """
MEMORY {
    RAM: start = __START__, size = __SIZE__, file = %O, type = ro;
}
SEGMENTS {
    CODE: load = RAM, type = ro;
}
""".strip()
            .replace("__START__", f"${start:04X}")
            .replace("__SIZE__", f"${ram_size:04X}")
            + "\n",
            encoding="ascii",
        )

        run([
            args.ca65,
            "-t",
            "none",
            "-I",
            str(asm_path.parent),
            "-o",
            str(obj_path),
            str(asm_path),
        ])
        run([
            args.ld65,
            "-C",
            str(cfg_path),
            "-o",
            str(bin_path),
            str(obj_path),
        ])

        data = list(bin_path.read_bytes())

    if data:
        # Pad to the end of the 256-byte page containing the last emitted byte.
        end_addr = start + len(data) - 1
        page_end = end_addr | 0x00FF
        target_len = (page_end - start) + 1
        if target_len > len(data):
            data.extend([0] * (target_len - len(data)))

    load_end = start + len(data) - 1 if data else start

    if args.sentinel is not None:
        data.append(args.sentinel)

    lines: list[str] = []
    marker_name = asm_path.name.replace('"', '""')
    marker_data = [-2, f'"{marker_name}"', start]

    if args.basic_start is not None:
        b = args.basic_start
        s = args.line_step
        scan_line = b + (3 * s)
        lines.extend(
            [
                f"{b} REM Load {asm_path.name} @ ${start:X} - ${load_end:X}",
                f"{b + s} RESTORE",
                f'{b + (2 * s)} Q0$="{marker_name}":Q1={start}',
                f"{scan_line} READ Q2",
                f"{scan_line + s} IF Q2<>-2 THEN {scan_line}",
                f"{scan_line + (2 * s)} READ Q3$",
                f"{scan_line + (3 * s)} IF Q3$<>Q0$ THEN {scan_line}",
                f"{scan_line + (4 * s)} READ Q4",
                f"{scan_line + (5 * s)} IF Q4<>Q1 THEN {scan_line}",
                f"{scan_line + (6 * s)} Q5=Q1",
                f"{scan_line + (7 * s)} READ Q6:IF Q6=-1 THEN {scan_line + (10 * s)}",
                f"{scan_line + (8 * s)} POKE Q5,Q6:Q5=Q5+1",
                f"{scan_line + (9 * s)} GOTO {scan_line + (7 * s)}",
                f"{scan_line + (10 * s)} RETURN",
            ]
        )

    if args.line_start is not None:
        line_no = args.line_start
    elif args.basic_start is not None:
        line_no = args.basic_start + (14 * args.line_step)
    else:
        line_no = 10

    lines.append(f"{line_no} DATA {marker_data[0]},{marker_data[1]},{marker_data[2]}")
    line_no += args.line_step

    for i in range(0, len(data), args.chunk):
        chunk = data[i : i + args.chunk]
        payload = ",".join(str(v) for v in chunk)
        lines.append(f"{line_no} DATA {payload}")
        line_no += args.line_step

    output = "\n".join(lines) + "\n"
    if args.out == "-":
        sys.stdout.write(output)
    else:
        pathlib.Path(args.out).write_text(output, encoding="ascii")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
