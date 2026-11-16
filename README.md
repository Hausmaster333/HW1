# ДЗ-1: умные указатели

Свои `UnqPtr`, `ShrdPtr` и `WeakPtr` (объекты и массивы), а на них — `Sequence` из прошлого
семестра: массив и связный список, изменяемые и неизменяемые.

| Файлы | Что там |
|---|---|
| `unq_ptr.h`, `shrd_ptr.h`, `weak_ptr.h` (+ `.tpp`) | Указатели |
| `sequence/` | `DynamicArray`, `LinkedList`, `ArraySequence`, `ListSequence` на этих указателях |
| `tests/` | GoogleTest: 66 тестов указателей, 15 тестов `Sequence` |
| `benchmarks/` | Сравнение с сырыми указателями и `std::unique_ptr` / `std::shared_ptr` |

## Сборка

Нужны `g++` с C++17 и `make` (на Windows — MSYS2 ucrt64 и `mingw32-make`), GoogleTest
подключён сабмодулем:

```console
git submodule update --init
mingw32-make all       # Linux: make all
```

## Тесты

```console
./ptr_tests
./sequence_tests
make valgrind          # только Linux: те же тесты под Valgrind, утечки и ошибки памяти
```

## Бенчмарк

Один замер — один запуск: `benchmark <scenario> <variant> <count>`, где scenario —
`unique | shared | copy | move | list`, variant — `raw | std | custom`. Печатает строку JSON.

```console
./benchmark shared custom 1000000
```

Полный прогон с графиками и отчётом в `benchmarks/results/` (нужен `matplotlib`):

```console
python benchmarks/run.py --sizes 10 1000 1000000   # быстро
python benchmarks/run.py                           # N от 10 до 10^8, около 15 минут
```
