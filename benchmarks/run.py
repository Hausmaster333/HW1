"""Run each benchmark in a fresh process and build tables and SVG charts.

From hw1: python benchmarks/run.py
For larger runs: python benchmarks/run.py --sizes 1000 1000000 10000000 --repeats 3 --allow-large
"""

import argparse
import csv
import math
import os
import platform
import random
import statistics
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROGRAM = ROOT / ("benchmark.exe" if os.name == "nt" else "benchmark")
SCENARIOS = ("owners", "clones", "descriptors")
VARIANTS = ("raw", "std", "custom")
COLORS = {"raw": "#677489", "std": "#268a80", "custom": "#bb5c3e"}
TITLES = {
    "owners": "Создание N независимых владельцев",
    "clones": "Глубокое копирование одного объекта N раз",
    "descriptors": "Создание N ссылок на один объект",
}
FIELDS = ("scenario", "variant", "count", "create_ns", "release_ns", "live_rss_bytes", "checksum")


def sample(scenario, variant, count):
    completed = subprocess.run(
        [str(PROGRAM), scenario, variant, str(count)],
        check=True, capture_output=True, text=True,
    )
    values = next(csv.reader([completed.stdout.strip()]))
    if len(values) != len(FIELDS):
        raise ValueError(f"Unexpected benchmark output: {completed.stdout!r}")
    row = {
        "scenario": values[0], "variant": values[1], "count": int(values[2]),
        "create_ns": float(values[3]), "release_ns": float(values[4]),
        "live_rss_bytes": int(values[5]), "checksum": int(values[6]),
    }
    expected = count * (count - 1) // 2 if scenario == "owners" else 42 * count
    if (row["scenario"], row["variant"], row["count"], row["checksum"]) != (
            scenario, variant, count, expected):
        raise ValueError(f"Incorrect benchmark result: {row}")
    return row


def write_chart(rows, sizes, metric, title, unit, destination):
    width, height = 900, 900
    left, right = 95, 35
    plot_width = width - left - right
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{left}" y="36" font-family="Arial" font-size="24" fill="#202936">{title}</text>',
    ]
    for index, variant in enumerate(VARIANTS):
        x = left + index * 170
        parts.append(f'<circle cx="{x}" cy="61" r="5" fill="{COLORS[variant]}"/>')
        parts.append(f'<text x="{x + 12}" y="66" font-family="Arial" font-size="14">{variant}</text>')

    for panel, scenario in enumerate(SCENARIOS):
        top = 105 + panel * 255
        bottom = top + 170
        selected = [row for row in rows if row["scenario"] == scenario]
        maximum = max(metric(row) for row in selected) * 1.1 or 1
        parts.append(f'<text x="{left}" y="{top - 15}" font-family="Arial" font-size="17" '
                     f'fill="#202936">{TITLES[scenario]} ({unit})</text>')
        for step in range(3):
            y = bottom - step * 85
            value = maximum * step / 2
            parts.append(f'<line x1="{left}" y1="{y}" x2="{width - right}" y2="{y}" '
                         f'stroke="#e2e7ed"/>')
            parts.append(f'<text x="{left - 9}" y="{y + 4}" text-anchor="end" '
                         f'font-family="Arial" font-size="12" fill="#566274">{value:.1f}</text>')
        for position, size in enumerate(sizes):
            x = left + (position + 0.5) * plot_width / len(sizes)
            parts.append(f'<text x="{x:.1f}" y="{bottom + 22}" text-anchor="middle" '
                         f'font-family="Arial" font-size="12" fill="#566274">{size:,}</text>')
        for variant in VARIANTS:
            points = []
            for position, size in enumerate(sizes):
                row = next(row for row in selected if row["variant"] == variant and row["count"] == size)
                x = left + (position + 0.5) * plot_width / len(sizes)
                y = bottom - metric(row) / maximum * 170
                points.append((x, y))
            polyline = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
            parts.append(f'<polyline points="{polyline}" fill="none" stroke="{COLORS[variant]}" '
                         f'stroke-width="2.5"/>')
            for x, y in points:
                parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" '
                             f'fill="{COLORS[variant]}"/>')
    parts.append('</svg>')
    destination.write_text("\n".join(parts), encoding="utf-8")


def write_comparison_chart(samples, scenario, count, destination):
    variants = ("std", "custom")
    names = {
        "owners": ("std::unique_ptr", "UnqPtr"),
        "descriptors": ("std::shared_ptr", "ShrdPtr"),
    }
    colors = {"std": "#1f77b4", "custom": "#ff7f0e"}
    runs = sorted({row["repeat"] for row in samples
                   if row["scenario"] == scenario and row["count"] == count})
    times = {(row["repeat"], row["variant"]):
             (row["create_ns"] + row["release_ns"]) * count / 1_000_000
             for row in samples if row["scenario"] == scenario and row["count"] == count
             and row["variant"] in variants}
    if not runs or any((run, variant) not in times for run in runs for variant in variants):
        raise ValueError(f"Incomplete comparison data for {scenario} at N={count}")

    width, height = 1350, 760
    left, right, top, bottom = 100, 35, 155, 660
    plot_width, plot_height = width - left - right, bottom - top
    maximum = max(times.values())
    ceiling = math.ceil(maximum / 10) * 10 if maximum >= 10 else math.ceil(maximum)
    title = (f"create/destroy x{count:,}: {names[scenario][0]} vs {names[scenario][1]}"
             if scenario == "owners" else
             f"copy/release x{count:,}: {names[scenario][0]} vs {names[scenario][1]}")
    subtitle = ("N владельцев живут одновременно; время = создание + освобождение"
                if scenario == "owners" else
                "N ссылок на один объект; время = создание + освобождение")
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width / 2}" y="45" text-anchor="middle" '
        f'font-family="Arial" font-size="26">{title}</text>',
        f'<text x="{width / 2}" y="78" text-anchor="middle" '
        f'font-family="Arial" font-size="17" fill="#555">{subtitle}</text>',
    ]
    for index, variant in enumerate(variants):
        x = left + index * 230
        parts.append(f'<rect x="{x}" y="105" width="32" height="18" fill="{colors[variant]}"/>')
        parts.append(f'<text x="{x + 42}" y="121" font-family="Arial" font-size="19">'
                     f'{names[scenario][index]}</text>')
    for tick in range(6):
        value = ceiling * tick / 5
        y = bottom - plot_height * tick / 5
        parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{width - right}" y2="{y:.1f}" '
                     'stroke="#b8bec5" stroke-dasharray="6 4"/>')
        parts.append(f'<text x="{left - 13}" y="{y + 6:.1f}" text-anchor="end" '
                     f'font-family="Arial" font-size="17">{value:g}</text>')
    step = plot_width / len(runs)
    bar_width = min(36, step * 0.34)
    for index, run in enumerate(runs):
        center = left + (index + 0.5) * step
        for offset, variant in enumerate(variants):
            value = times[(run, variant)]
            bar_height = value / ceiling * plot_height
            x = center + (offset - 1) * bar_width + 2 * offset - 2
            parts.append(f'<rect x="{x:.1f}" y="{bottom - bar_height:.1f}" '
                         f'width="{bar_width:.1f}" height="{bar_height:.1f}" '
                         f'fill="{colors[variant]}"/>')
        parts.append(f'<text x="{center:.1f}" y="{bottom + 29}" text-anchor="middle" '
                     f'font-family="Arial" font-size="19">{run}</text>')
    parts += [
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{bottom}" stroke="#222"/>',
        f'<line x1="{left}" y1="{bottom}" x2="{width - right}" y2="{bottom}" stroke="#222"/>',
        f'<text x="{width / 2}" y="735" text-anchor="middle" '
        'font-family="Arial" font-size="22">Прогон</text>',
        '<text x="35" y="410" text-anchor="middle" transform="rotate(-90 35 410)" '
        'font-family="Arial" font-size="22">Время, мс</text>',
        '</svg>',
    ]
    destination.write_text("\n".join(parts), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sizes", nargs="+", type=int, default=[1000, 100000, 1000000])
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--allow-large", action="store_true")
    args = parser.parse_args()
    sizes = sorted(set(args.sizes))
    if not sizes or sizes[0] <= 0 or args.repeats <= 0:
        parser.error("Sizes and repeats must be positive")
    if sizes[-1] > 1000000 and not args.allow_large:
        parser.error("More than 1,000,000 live objects needs --allow-large (high memory use)")
    if not PROGRAM.is_file():
        parser.error("Build the executable first: mingw32-make benchmark")

    jobs = [(scenario, variant, size, repeat)
            for scenario in SCENARIOS for variant in VARIANTS
            for size in sizes for repeat in range(args.repeats)]
    random.Random(42).shuffle(jobs)
    samples = []
    for number, (scenario, variant, size, repeat) in enumerate(jobs, 1):
        print(f"{number}/{len(jobs)} {scenario} {variant} N={size} repeat={repeat + 1}")
        row = sample(scenario, variant, size)
        row["repeat"] = repeat + 1
        samples.append(row)

    output = ROOT / "benchmarks" / "results"
    output.mkdir(exist_ok=True)
    with (output / "samples.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=(*FIELDS, "repeat"))
        writer.writeheader()
        writer.writerows(samples)

    summary = []
    for scenario in SCENARIOS:
        for size in sizes:
            for variant in VARIANTS:
                group = [row for row in samples if row["scenario"] == scenario
                         and row["variant"] == variant and row["count"] == size]
                checksums = {row["checksum"] for row in group}
                if len(checksums) != 1:
                    raise ValueError(f"Checksums differ in {scenario}/{variant}/{size}")
                summary.append({
                    "scenario": scenario, "variant": variant, "count": size,
                    "create_ns": statistics.median(row["create_ns"] for row in group),
                    "release_ns": statistics.median(row["release_ns"] for row in group),
                    "live_rss_bytes": int(statistics.median(row["live_rss_bytes"] for row in group)),
                    "repeats": args.repeats,
                })
    with (output / "summary.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=(*FIELDS[:-1], "repeats"))
        writer.writeheader()
        writer.writerows(summary)

    write_chart(summary, sizes, lambda row: row["create_ns"],
                "Время создания", "нс/элемент", output / "time.svg")
    write_chart(summary, sizes, lambda row: row["live_rss_bytes"] / 1048576,
                "Память при одновременно живых объектах", "МиБ", output / "memory.svg")
    write_comparison_chart(samples, "owners", sizes[-1], output / "unique_comparison.svg")
    write_comparison_chart(samples, "descriptors", sizes[-1], output / "shared_comparison.svg")

    lines = ["# Результаты бенчмарков", "",
             f"Система: {platform.platform()}. Повторов: {args.repeats}; в таблицах медиана.",
             "Каждый замер выполняется в отдельном процессе; сборка: -O2 -DNDEBUG.",
             "Память — прирост RSS при одновременно живых объектах, включая массив указателей.",
             "`raw` в сценарии descriptors — заимствованный указатель без управления временем жизни.",
             "Наш счётчик однопоточный; `std::shared_ptr` поддерживает многопоточное копирование.", ""]
    lines += [f"## Сравнение по прогонам (N = {sizes[-1]:,})", "",
              "Столбец показывает сумму времени создания и освобождения всей партии.",
              "В отличие от теста коллеги, все элементы партии одновременно живы.", "",
              "![Сравнение UnqPtr и std::unique_ptr](unique_comparison.svg)", "",
              "![Сравнение ShrdPtr и std::shared_ptr](shared_comparison.svg)", ""]
    for scenario in SCENARIOS:
        lines += [f"## {TITLES[scenario]}", "",
                  "| N | Вариант | Создание, нс/шт | Освобождение, нс/шт | Память, МиБ |",
                  "|---:|---|---:|---:|---:|"]
        for row in summary:
            if row["scenario"] == scenario:
                lines.append(f"| {row['count']:,} | {row['variant']} | {row['create_ns']:.2f} | "
                             f"{row['release_ns']:.2f} | {row['live_rss_bytes'] / 1048576:.2f} |")
        lines.append("")
    (output / "report.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"Saved tables and charts to {output}")


if __name__ == "__main__":
    main()
