// Нагрузочный тест: один замер = один процесс.
//     benchmark <scenario> <variant> <count> [repeats]
// scenario: unique | shared | copy | move | list;  variant: raw | std | custom.
// Печатает одну строку JSON: время фаз в нс на элемент и память.
//
// Устройство файла:
//   1. варианты владения — для каждого сценария три одинаковых по интерфейсу структуры
//      (raw / std / custom): как создать указатель, прочитать значение, освободить;
//   2. измерение — три функции (measure_owners, measure_move, measure_list), которые
//      засекают время фаз; код замера один на все три варианта, отличается только тип;
//   3. запуск — таблица BENCHMARKS выбирает функцию по аргументам командной строки.

#include "unq_ptr.h"
#include "shrd_ptr.h"
#include "support/alloc_counter.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <sched.h>
#include <unistd.h>
#endif

using Clock = std::chrono::steady_clock;

// Результат проверяется снаружи, поэтому компилятор не может выбросить вычисления
inline volatile unsigned long long sink = 0;

size_t process_rss() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        throw std::runtime_error("Cannot read process memory");
    }
    return counters.WorkingSetSize;
#else
    std::ifstream stat("/proc/self/statm");
    size_t pages = 0;
    size_t resident = 0;
    if (!(stat >> pages >> resident)) throw std::runtime_error("Cannot read process memory");
    return resident * static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
}

struct CacheSizes {
    size_t l2 = 0;
    size_t l3 = 0;
};

// Кэши L2 и L3 ядра, на котором начат замер: у гибридных процессоров они у ядер разные.
// Нули — если ОС не сообщила размеры; тогда отметки кэшей на графиках не рисуются.
CacheSizes detect_caches() {
    CacheSizes sizes;
#ifdef _WIN32
    DWORD length = 0;
    GetLogicalProcessorInformation(nullptr, &length);
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(
        length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    if (info.empty() || !GetLogicalProcessorInformation(info.data(), &length)) return sizes;

    DWORD processor = GetCurrentProcessorNumber();
    if (processor >= sizeof(ULONG_PTR) * 8) return sizes;
    ULONG_PTR current = ULONG_PTR(1) << processor;
    for (const auto& item : info) {
        if (item.Relationship != RelationCache || (item.ProcessorMask & current) == 0) continue;
        if (item.Cache.Level == 2) sizes.l2 = item.Cache.Size;
        if (item.Cache.Level == 3) sizes.l3 = item.Cache.Size;
    }
#else
    int processor = sched_getcpu();
    if (processor < 0) processor = 0;
    std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(processor) + "/cache/index";
    for (int index = 0; index < 8; index++) {
        // В sysfs размер записан как «2048K» или «30M»
        std::ifstream level_file(base + std::to_string(index) + "/level");
        std::ifstream size_file(base + std::to_string(index) + "/size");
        int level = 0;
        size_t amount = 0;
        char unit = 'K';
        if (!(level_file >> level) || !(size_file >> amount >> unit)) continue;
        size_t bytes = amount * (unit == 'M' ? 1024 * 1024 : 1024);
        if (level == 2) sizes.l2 = bytes;
        if (level == 3) sizes.l3 = bytes;
    }
#endif
    return sizes;
}

// ---------------------------------------------------------------- варианты владения

// unique: N независимых владельцев одного int
struct RawUnique {
    using Pointer = int*;
    Pointer create(size_t index) { return new int(static_cast<int>(index)); }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { delete pointer; }
};

struct StdUnique {
    using Pointer = std::unique_ptr<int>;
    Pointer create(size_t index) { return std::make_unique<int>(static_cast<int>(index)); }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer.reset(); }
};

struct CustomUnique {
    using Pointer = UnqPtr<int>;
    Pointer create(size_t index) { return UnqPtr<int>::make(static_cast<int>(index)); }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer.reset(); }
};

// shared: N независимых объектов с общим владением; raw — те же new / delete
using RawShared = RawUnique;

struct StdShared {
    using Pointer = std::shared_ptr<int>;
    Pointer create(size_t index) { return std::make_shared<int>(static_cast<int>(index)); }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer.reset(); }
};

struct CustomShared {
    using Pointer = ShrdPtr<int>;
    Pointer create(size_t index) { return ShrdPtr<int>::make(static_cast<int>(index)); }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer.reset(); }
};

// copy: N ссылок на один объект; raw — простое копирование адреса без учёта владельцев
struct RawCopy {
    using Pointer = int*;
    int source = 42;
    Pointer create(size_t) { return &source; }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer = nullptr; }
};

struct StdCopy {
    using Pointer = std::shared_ptr<int>;
    Pointer source = std::make_shared<int>(42);
    Pointer create(size_t) { return source; }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer.reset(); }
};

struct CustomCopy {
    using Pointer = ShrdPtr<int>;
    Pointer source = ShrdPtr<int>::make(42);
    Pointer create(size_t) { return source; }
    static int read(const Pointer& pointer) { return *pointer; }
    static void release(Pointer& pointer) { pointer.reset(); }
};

// move: передача владения; raw — копия адреса и обнуление источника вручную
struct RawMove : RawUnique {
    static Pointer take(Pointer& source) {
        Pointer taken = source;
        source = nullptr;
        return taken;
    }
};

struct StdMove : StdUnique {
    static Pointer take(Pointer& source) { return std::move(source); }
};

struct CustomMove : CustomUnique {
    static Pointer take(Pointer& source) { return std::move(source); }
};

// list: односвязный список, узлы владеют следующими (как LinkedList из sequence/)
class RawList {
    private:
        struct Node {
            int value;
            Node* next;
        };
        Node* head = nullptr;
        Node* tail = nullptr;
    public:
        void push_back(int value) {
            Node* node = new Node{value, nullptr};
            if (head == nullptr) head = node;
            else tail->next = node;
            tail = node;
        }
        unsigned long long sum() const {
            unsigned long long total = 0;
            for (const Node* node = head; node != nullptr; node = node->next) total += node->value;
            return total;
        }
        void clear() {
            while (head != nullptr) {
                Node* next = head->next;
                delete head;
                head = next;
            }
            tail = nullptr;
        }
        ~RawList() { clear(); }
};

// Тот же список, но узлами владеет умный указатель: std::unique_ptr или UnqPtr
template <template <class> class Owner>
class SmartList {
    private:
        struct Node {
            int value;
            Owner<Node> next;

            explicit Node(int value) : value(value) {}
        };
        Owner<Node> head;
        Node* tail = nullptr;

        static Owner<Node> new_node(int value) {
            if constexpr (std::is_same_v<Owner<Node>, UnqPtr<Node>>) {
                return UnqPtr<Node>::make(value);
            } else {
                return std::make_unique<Node>(value);
            }
        }
    public:
        void push_back(int value) {
            Owner<Node> node = new_node(value);
            Node* raw = node.get();
            if (!head) head = std::move(node);
            else tail->next = std::move(node);
            tail = raw;
        }
        unsigned long long sum() const {
            unsigned long long total = 0;
            for (const Node* node = head.get(); node != nullptr; node = node->next.get()) {
                total += node->value;
            }
            return total;
        }
        // Итеративно: рекурсивный деструктор цепочки из 10^8 узлов переполнил бы стек
        void clear() {
            while (head) {
                head = std::move(head->next);
            }
            tail = nullptr;
        }
        ~SmartList() { clear(); }
};

using StdList = SmartList<std::unique_ptr>;
using CustomList = SmartList<UnqPtr>;

// ---------------------------------------------------------------- измерение

struct Phase {
    const char* name;
    double total_ns = 0;
};

struct Measurement {
    std::vector<Phase> phases;
    size_t pointer_bytes = 0;
    size_t heap_bytes = 0;
    size_t allocations = 0;
    size_t rss_bytes = 0;
    unsigned long long checksum = 0;
};

// Стоимость самого замера времени: вычитается из каждого интервала
double clock_overhead_ns() {
    std::vector<double> samples;
    for (int index = 0; index < 1001; index++) {
        auto start = Clock::now();
        auto stop = Clock::now();
        samples.push_back(std::chrono::duration<double, std::nano>(stop - start).count());
    }
    std::nth_element(samples.begin(), samples.begin() + 500, samples.end());
    return samples[500];
}

class Stopwatch {
    private:
        double overhead;
        Clock::time_point start;
    public:
        explicit Stopwatch(double overhead) : overhead(overhead) {}
        void begin() { start = Clock::now(); }
        void end(Phase& phase) {
            double elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
            phase.total_ns += std::max(0.0, elapsed - overhead);
        }
};

// Снимок памяти сразу после создания всех N элементов (только в первом повторе)
void record_memory(Measurement& result, const AllocSnapshot& heap_before,
                   size_t rss_before) {
    auto heap_after = alloc_snapshot();
    result.heap_bytes = heap_after.bytes - heap_before.bytes;
    result.allocations = heap_after.allocations - heap_before.allocations;
    size_t rss = process_rss();
    result.rss_bytes = rss > rss_before ? rss - rss_before : 0;
}

// unique / shared / copy: создать N живых указателей, прочитать все, освободить все
template <class Policy>
Measurement measure_owners(size_t count, size_t repeats, double overhead) {
    using Pointer = typename Policy::Pointer;
    Measurement result;
    result.phases = {{"create"}, {"access"}, {"release"}};
    result.pointer_bytes = sizeof(Pointer);
    Policy policy;
    Stopwatch watch(overhead);

    size_t rss_before = process_rss();
    std::vector<Pointer> items;
    items.reserve(count);
    for (size_t repeat = 0; repeat < repeats; repeat++) {
        auto heap_before = alloc_snapshot();
        watch.begin();
        for (size_t index = 0; index < count; index++) items.push_back(policy.create(index));
        watch.end(result.phases[0]);
        if (repeat == 0) record_memory(result, heap_before, rss_before);

        unsigned long long sum = 0;
        watch.begin();
        for (const Pointer& item : items) sum += Policy::read(item);
        watch.end(result.phases[1]);
        sink = sum;
        if (repeat == 0) result.checksum = sum;

        watch.begin();
        for (Pointer& item : items) Policy::release(item);
        items.clear();
        watch.end(result.phases[2]);
    }
    return result;
}

// move: передать владение N объектами из одного массива в другой
template <class Policy>
Measurement measure_move(size_t count, size_t repeats, double overhead) {
    using Pointer = typename Policy::Pointer;
    Measurement result;
    result.phases = {{"move"}};
    result.pointer_bytes = sizeof(Pointer);
    Policy policy;
    Stopwatch watch(overhead);

    std::vector<Pointer> source;
    std::vector<Pointer> target;
    source.reserve(count);
    target.reserve(count);
    for (size_t repeat = 0; repeat < repeats; repeat++) {
        for (size_t index = 0; index < count; index++) source.push_back(policy.create(index));

        auto heap_before = alloc_snapshot();
        size_t rss_before = process_rss();
        watch.begin();
        for (Pointer& item : source) target.push_back(Policy::take(item));
        watch.end(result.phases[0]);
        if (repeat == 0) record_memory(result, heap_before, rss_before);

        unsigned long long sum = 0;
        for (const Pointer& item : target) sum += Policy::read(item);
        sink = sum;
        if (repeat == 0) result.checksum = sum;
        for (Pointer& item : target) Policy::release(item);
        source.clear();
        target.clear();
    }
    return result;
}

// list: построить список из N узлов, пройти его, разрушить
template <class List>
Measurement measure_list(size_t count, size_t repeats, double overhead) {
    Measurement result;
    result.phases = {{"create"}, {"access"}, {"release"}};
    result.pointer_bytes = 0;
    Stopwatch watch(overhead);

    size_t rss_before = process_rss();
    for (size_t repeat = 0; repeat < repeats; repeat++) {
        List list;
        auto heap_before = alloc_snapshot();
        watch.begin();
        for (size_t index = 0; index < count; index++) list.push_back(static_cast<int>(index));
        watch.end(result.phases[0]);
        if (repeat == 0) record_memory(result, heap_before, rss_before);

        watch.begin();
        unsigned long long sum = list.sum();
        watch.end(result.phases[1]);
        sink = sum;
        if (repeat == 0) result.checksum = sum;

        watch.begin();
        list.clear();
        watch.end(result.phases[2]);
    }
    return result;
}

// ---------------------------------------------------------------- запуск

using Runner = Measurement (*)(size_t count, size_t repeats, double overhead);

struct Benchmark {
    const char* scenario;
    const char* variant;
    Runner run;
};

// Все 15 замеров: сценарий и вариант из командной строки -> функция замера
const Benchmark BENCHMARKS[] = {
    {"unique", "raw", measure_owners<RawUnique>},
    {"unique", "std", measure_owners<StdUnique>},
    {"unique", "custom", measure_owners<CustomUnique>},
    {"shared", "raw", measure_owners<RawShared>},
    {"shared", "std", measure_owners<StdShared>},
    {"shared", "custom", measure_owners<CustomShared>},
    {"copy", "raw", measure_owners<RawCopy>},
    {"copy", "std", measure_owners<StdCopy>},
    {"copy", "custom", measure_owners<CustomCopy>},
    {"move", "raw", measure_move<RawMove>},
    {"move", "std", measure_move<StdMove>},
    {"move", "custom", measure_move<CustomMove>},
    {"list", "raw", measure_list<RawList>},
    {"list", "std", measure_list<StdList>},
    {"list", "custom", measure_list<CustomList>},
};

Runner find_benchmark(const std::string& scenario, const std::string& variant) {
    for (const Benchmark& benchmark : BENCHMARKS) {
        if (scenario == benchmark.scenario && variant == benchmark.variant) return benchmark.run;
    }
    throw std::invalid_argument("Unknown scenario or variant: " + scenario + " " + variant);
}

void print_json(const std::string& scenario, const std::string& variant, size_t count,
                size_t repeats, const Measurement& result, const CacheSizes& caches) {
    double items = static_cast<double>(count) * static_cast<double>(repeats);
    std::printf("{\"scenario\": \"%s\", \"variant\": \"%s\", \"count\": %zu, \"repeats\": %zu, ",
                scenario.c_str(), variant.c_str(), count, repeats);
    std::printf("\"phases\": {");
    for (size_t index = 0; index < result.phases.size(); index++) {
        std::printf("%s\"%s\": %.4f", index == 0 ? "" : ", ", result.phases[index].name,
                    result.phases[index].total_ns / items);
    }
    std::printf("}, \"pointer_bytes\": %zu, \"heap_bytes\": %zu, \"allocations\": %zu, ",
                result.pointer_bytes, result.heap_bytes, result.allocations);
    std::printf("\"rss_bytes\": %zu, \"checksum\": %llu, ", result.rss_bytes, result.checksum);
    std::printf("\"caches\": {\"L2\": %zu, \"L3\": %zu}}\n", caches.l2, caches.l3);
}

size_t parse_positive(const std::string& text) {
    size_t used = 0;
    unsigned long long value = std::stoull(text, &used);
    if (text[0] == '-' || used != text.size() || value == 0 ||
        value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("Expected a positive number up to INT_MAX: " + text);
    }
    return static_cast<size_t>(value);
}

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 5) {
        std::fprintf(stderr, "Usage: benchmark <unique|shared|copy|move|list> "
                             "<raw|std|custom> <count> [repeats]\n");
        return 2;
    }

    try {
        std::string scenario = argv[1];
        std::string variant = argv[2];
        Runner run = find_benchmark(scenario, variant);
        size_t count = parse_positive(argv[3]);
        // Малые N повторяются, чтобы на замер приходилось не меньше ~2·10^6 элементов
        size_t repeats = argc == 5 ? parse_positive(argv[4])
                                   : std::max<size_t>(1, 2'000'000 / count);
        CacheSizes caches = detect_caches();
        Measurement result = run(count, repeats, clock_overhead_ns());
        print_json(scenario, variant, count, repeats, result, caches);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
