"""Запускает benchmark в отдельных процессах и строит таблицы и графики.

Из папки hw1 (после mingw32-make benchmark):
    python benchmarks/run.py                         # N от 10 до 10^8, около 15 минут
    python benchmarks/run.py --sizes 10 1000 1000000 # быстрый прогон
    python benchmarks/run.py --from-samples          # перестроить отчёт без новых замеров

Порядок работы: каждый замер (сценарий, вариант, N) запускается отдельным процессом
benchmark, сырые результаты сохраняются в samples.json, по ним считаются медианы
(summary.csv), строятся графики (*.png) и собирается report.md. Текст выводов в отчёте
берётся из benchmarks/notes.md.
"""

import argparse
import csv
import json
import os
import platform
import random
import statistics
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.patches  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
PROGRAM = ROOT / ("benchmark.exe" if os.name == "nt" else "benchmark")
OUTPUT = ROOT / "benchmarks" / "results"
NOTES = ROOT / "benchmarks" / "notes.md"

SCENARIOS = {
    "unique": ("create", "access", "release"),
    "shared": ("create", "access", "release"),
    "copy": ("create", "access", "release"),
    "move": ("move",),
    "list": ("create", "access", "release"),
}
TITLES = {
    "unique": "N независимых владельцев: new/delete, unique_ptr, UnqPtr",
    "shared": "N объектов в общем владении: new/delete, shared_ptr, ShrdPtr",
    "copy": "N ссылок на один объект: копия адреса, shared_ptr, ShrdPtr",
    "move": "Передача владения N объектами",
    "list": "Односвязный список из N узлов",
}
PHASE_NAMES = {"create": "создание", "access": "доступ", "release": "освобождение",
               "move": "передача"}
VARIANTS = ("raw", "std", "custom")
LABELS = {"raw": "сырой указатель", "std": "STL", "custom": "UnqPtr / ShrdPtr"}
COLORS = {"raw": "#677489", "std": "#268a80", "custom": "#bb5c3e"}
DEFAULT_SIZES = [10, 100, 1000, 10_000, 100_000, 1_000_000, 10_000_000, 100_000_000]


def sample(scenario, variant, count):
    """Один замер в отдельном процессе; контрольная сумма проверяет, что работа сделана."""
    completed = subprocess.run([str(PROGRAM), scenario, variant, str(count)],
                               check=True, capture_output=True, text=True)
    row = json.loads(completed.stdout)
    expected = 42 * count if scenario == "copy" else count * (count - 1) // 2
    if row["checksum"] != expected:
        raise ValueError(f"Wrong checksum: {row}")
    return row


def median_rows(samples, sizes):
    """Сводит повторы одного (сценарий, вариант, N) в строку медиан; total — сумма фаз."""
    summary = []
    for scenario, phases in SCENARIOS.items():
        for size in sizes:
            for variant in VARIANTS:
                group = [row for row in samples if row["scenario"] == scenario
                         and row["variant"] == variant and row["count"] == size]
                entry = {"scenario": scenario, "variant": variant, "count": size,
                         "runs": len(group)}
                for phase in phases:
                    entry[phase] = statistics.median(row["phases"][phase] for row in group)
                entry["total"] = sum(entry[phase] for phase in phases)
                for key in ("pointer_bytes", "heap_bytes", "allocations", "rss_bytes"):
                    entry[key] = int(statistics.median(row[key] for row in group))
                summary.append(entry)
    return summary


def find(summary, scenario, variant, size):
    return next(row for row in summary if row["scenario"] == scenario
                and row["variant"] == variant and row["count"] == size)


def machine_caches(samples):
    """Кэши L2 и L3 машины замера: [("L2", байт), ("L3", байт)].

    У гибридных процессоров ядра разные, поэтому берётся самый частый вариант.
    Пустой список — ОС не сообщила размеры.
    """
    l2, l3 = statistics.mode((row["caches"]["L2"], row["caches"]["L3"]) for row in samples)
    return [(name, size) for name, size in (("L2", l2), ("L3", l3)) if size > 0]


def mark_caches(axis, summary, scenario, sizes, caches):
    """Пунктир на графике там, где данные варианта raw перестают помещаться в кэш.

    Объём на элемент — RSS при самом большом N: в нём указатели, объекты и служебные
    данные аллокатора.
    """
    row = find(summary, scenario, "raw", sizes[-1])
    per_item = row["rss_bytes"] / sizes[-1]
    if per_item <= 0:
        return
    for name, capacity in caches:
        boundary = capacity / per_item
        if sizes[0] < boundary < sizes[-1]:
            axis.axvline(boundary, color="#999999", linestyle=":", linewidth=1.2)
            axis.text(boundary, 0.97, f" {name}", transform=axis.get_xaxis_transform(),
                      fontsize=8, color="#777777", va="top")


def plot_time(summary, sizes, caches):
    """time.png: строка на сценарий, столбец на фазу, линия на вариант; нс на элемент."""
    figure, axes = plt.subplots(len(SCENARIOS), 3, figsize=(15, 19), squeeze=False)
    for line, (scenario, phases) in enumerate(SCENARIOS.items()):
        for column in range(3):
            axis = axes[line][column]
            if column >= len(phases):
                axis.axis("off")
                continue
            phase = phases[column]
            for variant in VARIANTS:
                values = [find(summary, scenario, variant, size)[phase] for size in sizes]
                axis.plot(sizes, values, marker="o", color=COLORS[variant],
                          label=LABELS[variant])
            mark_caches(axis, summary, scenario, sizes, caches)
            axis.set_xscale("log")
            axis.set_ylim(bottom=0)
            axis.grid(True, alpha=0.3)
            axis.set_title(f"{scenario}: {PHASE_NAMES[phase]}", fontsize=11)
            axis.set_xlabel("N")
            axis.set_ylabel("нс на элемент")
        axes[line][0].legend(fontsize=9)
    figure.suptitle("Время на один элемент (медиана); пунктир — данные перестают "
                    "помещаться в кэш", fontsize=14)
    figure.tight_layout(rect=(0, 0, 1, 0.98))
    figure.savefig(OUTPUT / "time.png", dpi=110)
    plt.close(figure)


def plot_batch(summary, sizes):
    """batch.png: сколько миллисекунд занимает полный цикл для всех N элементов."""
    figure, axes = plt.subplots(1, len(SCENARIOS), figsize=(20, 4.8))
    for axis, scenario in zip(axes, SCENARIOS):
        for variant in VARIANTS:
            totals = [find(summary, scenario, variant, size)["total"] * size / 1e6
                      for size in sizes]
            axis.plot(sizes, totals, marker="o", color=COLORS[variant], label=LABELS[variant])
        axis.set_xscale("log")
        axis.set_yscale("log")
        axis.grid(True, which="major", alpha=0.3)
        axis.set_title(scenario)
        axis.set_xlabel("N")
    axes[0].set_ylabel("мс на всю партию")
    axes[0].legend(fontsize=9)
    figure.suptitle("Время полного цикла для всех N элементов (обе оси логарифмические: "
                    "прямая с наклоном 1 — линейный рост)", fontsize=13)
    figure.tight_layout(rect=(0, 0, 1, 0.92))
    figure.savefig(OUTPUT / "batch.png", dpi=110)
    plt.close(figure)


def plot_totals(summary, sizes):
    """totals.png: столбцы «нс на элемент» при N = 1000 и при самом большом N.

    Столбец — вариант, внутри него фазы сложены друг на друга (штриховка — фаза).
    """
    panels = [size for size in (1000, sizes[-1]) if size in sizes] or [sizes[-1]]
    hatches = {"create": "", "move": "", "access": "//", "release": ".."}
    figure, axes = plt.subplots(1, len(panels), figsize=(8 * len(panels), 5.8), squeeze=False)
    width = 0.27
    for axis, size in zip(axes[0], panels):
        for offset, variant in enumerate(VARIANTS):
            for position, (scenario, phases) in enumerate(SCENARIOS.items()):
                row = find(summary, scenario, variant, size)
                x = position + (offset - 1) * width
                bottom = 0.0
                for phase in phases:
                    axis.bar(x, row[phase], width, bottom=bottom, color=COLORS[variant],
                             hatch=hatches[phase], edgecolor="white", linewidth=0.6)
                    bottom += row[phase]
                axis.text(x, bottom, f"{bottom:.1f}", ha="center", va="bottom", fontsize=8)
        axis.set_xticks(range(len(SCENARIOS)), list(SCENARIOS))
        axis.set_ylabel("нс на элемент")
        axis.set_title(f"N = {size:,}")
        axis.grid(True, axis="y", alpha=0.3)
    variant_legend = [matplotlib.patches.Patch(color=COLORS[variant], label=LABELS[variant])
                      for variant in VARIANTS]
    phase_legend = [matplotlib.patches.Patch(facecolor="#bbbbbb", hatch=hatches[phase],
                                             edgecolor="white", label=PHASE_NAMES[phase])
                    for phase in ("create", "access", "release")]
    axes[0][0].legend(handles=variant_legend + phase_legend, fontsize=9)
    figure.suptitle("Полный цикл на один элемент: создание + доступ + освобождение "
                    "(в move — только передача)", fontsize=13)
    figure.tight_layout(rect=(0, 0, 1, 0.95))
    figure.savefig(OUTPUT / "totals.png", dpi=110)
    plt.close(figure)


def plot_overhead(summary, sizes):
    """overhead.png: во сколько раз std и custom медленнее raw (1 — одинаково)."""
    figure, axes = plt.subplots(1, len(SCENARIOS), figsize=(20, 4.6))
    for axis, scenario in zip(axes, SCENARIOS):
        for variant in ("std", "custom"):
            ratios = [find(summary, scenario, variant, size)["total"]
                      / find(summary, scenario, "raw", size)["total"] for size in sizes]
            axis.plot(sizes, ratios, marker="o", color=COLORS[variant], label=LABELS[variant])
        axis.axhline(1.0, color=COLORS["raw"], linestyle="--", label="сырой указатель = 1")
        axis.set_xscale("log")
        axis.set_ylim(bottom=0)
        axis.grid(True, alpha=0.3)
        axis.set_title(scenario)
        axis.set_xlabel("N")
    axes[0].set_ylabel("время полного цикла / время raw")
    axes[0].legend(fontsize=9)
    figure.suptitle("Во сколько раз медленнее варианта без умных указателей", fontsize=14)
    figure.tight_layout(rect=(0, 0, 1, 0.93))
    figure.savefig(OUTPUT / "overhead.png", dpi=110)
    plt.close(figure)


def plot_memory(summary, sizes):
    """memory.png: байт на элемент — запрошено у new (слева) и реально занято (справа)."""
    largest = sizes[-1]
    figure, axes = plt.subplots(1, 2, figsize=(16, 5.5))
    width = 0.27
    positions = range(len(SCENARIOS))
    for offset, variant in enumerate(VARIANTS):
        exact = []
        measured = []
        for scenario in SCENARIOS:
            row = find(summary, scenario, variant, largest)
            exact.append(row["pointer_bytes"] + row["heap_bytes"] / largest)
            measured.append(row["rss_bytes"] / largest)
        shifted = [position + (offset - 1) * width for position in positions]
        for axis, values in ((axes[0], exact), (axes[1], measured)):
            bars = axis.bar(shifted, values, width, color=COLORS[variant], label=LABELS[variant])
            axis.bar_label(bars, fmt="%.0f", fontsize=8)
    axes[0].set_title("Запрошено памяти: указатель + куча, байт на элемент")
    axes[1].set_title(f"Прирост рабочего набора процесса при N = {largest:,}, байт на элемент")
    for axis in axes:
        axis.set_xticks(list(positions), list(SCENARIOS))
        axis.grid(True, axis="y", alpha=0.3)
        axis.legend(fontsize=9)
    figure.tight_layout()
    figure.savefig(OUTPUT / "memory.png", dpi=110)
    plt.close(figure)


def write_report(summary, sizes, meta, caches):
    """report.md: условия замера, выводы из notes.md, графики и таблицы по сценариям."""
    largest = sizes[-1]
    cache_text = ", ".join(f"{name} {size / 1024 ** 2:g} МиБ" for name, size in caches)
    cache_line = (f"Кэши ядра, на котором шли замеры: {cache_text}." if caches
                  else "Размеры кэшей ОС не сообщила, отметки кэшей на графиках не рисуются.")
    lines = [
        "# Результаты бенчмарков", "",
        f"Система: {meta['system']}. {cache_line}",
        "Сборка: `g++ -O2 -DNDEBUG -static`. Каждый замер — отдельный процесс, порядок "
        "замеров случайный, в таблицах медиана.",
        f"Повторов: {meta['repeats']} для N ≤ 10^6, {meta['large_repeats']} для больших N. "
        "Малые N внутри "
        "процесса прогоняются многократно, чтобы на замер приходилось ≥ 2·10^6 элементов; "
        "стоимость вызова часов вычитается.", "",
        "Варианты: **raw** — сырой указатель и ручные `new`/`delete` (вариант без умных "
        "указателей), **std** — `std::unique_ptr` / `std::shared_ptr`, **custom** — "
        "`UnqPtr` / `ShrdPtr`. В сценарии `copy` raw просто копирует адрес и не следит за "
        "временем жизни.", "",
        NOTES.read_text(encoding="utf-8"),
        "![Вся партия](batch.png)", "", "![Полный цикл](totals.png)", "",
        "![Время по фазам](time.png)", "",
        "![Отношение к raw](overhead.png)", "",
        "![Память](memory.png)", "",
        "## Память на элемент", "",
        "Указатель — `sizeof`, куча — байты, запрошенные у `operator new` (точно, по "
        "счётчику выделений), RSS — фактический прирост рабочего набора с учётом служебных "
        f"данных аллокатора, N = {largest:,}.", "",
        "| Сценарий | Вариант | Указатель, Б | Куча, Б/эл | Выделений/эл | RSS, Б/эл |",
        "|---|---|---:|---:|---:|---:|",
    ]
    for scenario in SCENARIOS:
        for variant in VARIANTS:
            row = find(summary, scenario, variant, largest)
            lines.append(f"| {scenario} | {variant} | {row['pointer_bytes']} | "
                         f"{row['heap_bytes'] / largest:.1f} | "
                         f"{row['allocations'] / largest:.2f} | "
                         f"{row['rss_bytes'] / largest:.1f} |")
    lines.append("")

    for scenario, phases in SCENARIOS.items():
        header = " | ".join(f"{PHASE_NAMES[phase]}, нс" for phase in phases)
        lines += [f"## {TITLES[scenario]}", "",
                  f"| N | Вариант | {header} | итого, нс | к raw |",
                  "|---:|---|" + "---:|" * (len(phases) + 2)]
        for size in sizes:
            raw_total = find(summary, scenario, "raw", size)["total"]
            for variant in VARIANTS:
                row = find(summary, scenario, variant, size)
                cells = " | ".join(f"{row[phase]:.2f}" for phase in phases)
                lines.append(f"| {size:,} | {variant} | {cells} | {row['total']:.2f} | "
                             f"×{row['total'] / raw_total:.2f} |")
        lines.append("")
    (OUTPUT / "report.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sizes", nargs="+", type=int, default=DEFAULT_SIZES)
    parser.add_argument("--repeats", type=int, default=5, help="повторов для N ≤ 10^6")
    parser.add_argument("--large-repeats", type=int, default=3, help="повторов для N > 10^6")
    parser.add_argument("--from-samples", action="store_true",
                        help="не запускать замеры, перестроить отчёт из samples.json")
    args = parser.parse_args()

    # 1. Замеры: новые или сохранённые ранее
    if args.from_samples:
        # Описание машины берётся из файла: отчёт можно перестроить и на другом компьютере
        stored = json.loads((OUTPUT / "samples.json").read_text(encoding="utf-8"))
        meta, samples = stored["meta"], stored["samples"]
        sizes = sorted({row["count"] for row in samples})
    else:
        sizes = sorted(set(args.sizes))
        if sizes[0] <= 0 or args.repeats <= 0 or args.large_repeats <= 0:
            parser.error("Sizes and repeats must be positive")
        if not PROGRAM.is_file():
            parser.error("Build the executable first: mingw32-make benchmark")

        # Случайный порядок: фоновая нагрузка не попадёт систематически в один вариант
        jobs = [(scenario, variant, size)
                for scenario in SCENARIOS for variant in VARIANTS for size in sizes
                for _ in range(args.repeats if size <= 1_000_000 else args.large_repeats)]
        random.Random(42).shuffle(jobs)
        samples = []
        for number, (scenario, variant, size) in enumerate(jobs, 1):
            print(f"{number}/{len(jobs)} {scenario} {variant} N={size:,}", flush=True)
            samples.append(sample(scenario, variant, size))

        meta = {"system": f"{platform.platform()}, {platform.processor()}",
                "repeats": args.repeats, "large_repeats": args.large_repeats}
        OUTPUT.mkdir(exist_ok=True)
        with (OUTPUT / "samples.json").open("w", encoding="utf-8") as file:
            json.dump({"meta": meta, "samples": samples}, file, ensure_ascii=False, indent=1)

    # 2. Медианы, 3. графики и отчёт
    summary = median_rows(samples, sizes)
    fields = ["scenario", "variant", "count", "runs", "create", "access", "release", "move",
              "total", "pointer_bytes", "heap_bytes", "allocations", "rss_bytes"]
    with (OUTPUT / "summary.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary)

    plot_batch(summary, sizes)
    plot_totals(summary, sizes)
    caches = machine_caches(samples)
    plot_time(summary, sizes, caches)
    plot_overhead(summary, sizes)
    plot_memory(summary, sizes)
    write_report(summary, sizes, meta, caches)
    print(f"Saved tables and charts to {OUTPUT}")


if __name__ == "__main__":
    main()
