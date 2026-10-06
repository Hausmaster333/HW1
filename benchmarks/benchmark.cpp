#include "unq_ptr.h"
#include "shrd_ptr.h"
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <unistd.h>
#endif

struct Result {
    double create_ns;
    double release_ns;
    size_t live_rss_bytes;
    unsigned long long checksum;
};

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

int* create_raw_owner(size_t index, int*) {
    return new int(static_cast<int>(index));
}

std::unique_ptr<int> create_std_owner(size_t index, const std::unique_ptr<int>&) {
    return std::make_unique<int>(static_cast<int>(index));
}

UnqPtr<int> create_custom_owner(size_t index, const UnqPtr<int>&) {
    return UnqPtr<int>::make(static_cast<int>(index));
}

int* clone_raw_owner(size_t, int* source) {
    return new int(*source);
}

std::unique_ptr<int> clone_std_owner(size_t, const std::unique_ptr<int>& source) {
    return std::make_unique<int>(*source);
}

UnqPtr<int> clone_custom_owner(size_t, const UnqPtr<int>& source) {
    return source.clone();
}

int* borrow_raw(size_t, int* source) {
    return source;
}

std::shared_ptr<int> copy_std_shared(size_t, const std::shared_ptr<int>& source) {
    return source;
}

ShrdPtr<int> copy_custom_shared(size_t, const ShrdPtr<int>& source) {
    return source;
}

template <class Pointer>
void release_items(std::vector<Pointer>& items) {
    items.clear();
}

void release_raw_owners(std::vector<int*>& items) {
    for (int* item : items) delete item;
    items.clear();
}

template <class Pointer, auto Create, auto Release>
Result measure(size_t count, const Pointer& source = Pointer{}) {
    using Clock = std::chrono::steady_clock;
    std::vector<Pointer> items;
    size_t before = process_rss();
    items.reserve(count);

    auto start = Clock::now();
    for (size_t index = 0; index < count; index++) items.push_back(Create(index, source));
    auto created = Clock::now();

    unsigned long long checksum = 0;
    for (const auto& item : items) checksum += static_cast<unsigned long long>(*item);
    size_t live = process_rss();

    auto release_start = Clock::now();
    Release(items);
    auto released = Clock::now();

    double create_ns = std::chrono::duration<double, std::nano>(created - start).count() / count;
    double release_ns = std::chrono::duration<double, std::nano>(released - release_start).count() / count;
    return {create_ns, release_ns, live > before ? live - before : 0, checksum};
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: benchmark <owners|clones|descriptors> <raw|std|custom> <count>\n";
        return 2;
    }

    try {
        std::string scenario = argv[1];
        std::string variant = argv[2];
        std::string count_text = argv[3];
        size_t used = 0;
        unsigned long long parsed = std::stoull(count_text, &used);
        if (count_text[0] == '-' || used != count_text.size() || parsed == 0 ||
            parsed > std::numeric_limits<size_t>::max()) {
            throw std::invalid_argument("Count must be a positive size_t");
        }
        size_t count = static_cast<size_t>(parsed);
        Result result;

        if (scenario == "owners" && variant == "raw") {
            result = measure<int*, create_raw_owner, release_raw_owners>(count);
        } else if (scenario == "owners" && variant == "std") {
            result = measure<std::unique_ptr<int>, create_std_owner,
                release_items<std::unique_ptr<int>>>(count);
        } else if (scenario == "owners" && variant == "custom") {
            result = measure<UnqPtr<int>, create_custom_owner, release_items<UnqPtr<int>>>(count);
        } else if (scenario == "clones" && variant == "raw") {
            int source = 42;
            result = measure<int*, clone_raw_owner, release_raw_owners>(count, &source);
        } else if (scenario == "clones" && variant == "std") {
            auto source = std::make_unique<int>(42);
            result = measure<std::unique_ptr<int>, clone_std_owner,
                release_items<std::unique_ptr<int>>>(count, source);
        } else if (scenario == "clones" && variant == "custom") {
            auto source = UnqPtr<int>::make(42);
            result = measure<UnqPtr<int>, clone_custom_owner, release_items<UnqPtr<int>>>(count, source);
        } else if (scenario == "descriptors" && variant == "raw") {
            int source = 42;
            result = measure<int*, borrow_raw, release_items<int*>>(count, &source);
        } else if (scenario == "descriptors" && variant == "std") {
            auto source = std::make_shared<int>(42);
            result = measure<std::shared_ptr<int>, copy_std_shared,
                release_items<std::shared_ptr<int>>>(count, source);
        } else if (scenario == "descriptors" && variant == "custom") {
            auto source = ShrdPtr<int>::make(42);
            result = measure<ShrdPtr<int>, copy_custom_shared, release_items<ShrdPtr<int>>>(count, source);
        } else {
            throw std::invalid_argument("Unknown scenario or variant");
        }

        std::cout << scenario << ',' << variant << ',' << count << ','
                  << std::fixed << std::setprecision(2) << result.create_ns << ','
                  << result.release_ns << ',' << result.live_rss_bytes << ','
                  << result.checksum << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
