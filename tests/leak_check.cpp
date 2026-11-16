#include "support/alloc_counter.h"
#include <gtest/gtest.h>

// После каждого теста число живых выделений кучи должно вернуться к исходному
class LeakListener : public ::testing::EmptyTestEventListener {
    private:
        size_t live_at_start = 0;

        void OnTestStart(const ::testing::TestInfo&) override {
            live_at_start = alloc_snapshot().live();
        }

        void OnTestEnd(const ::testing::TestInfo& info) override {
            fail_allocation_after(0);
            // У упавшего теста GoogleTest сам хранит сообщения об ошибках в куче
            if (!info.result()->Passed()) return;
            size_t live = alloc_snapshot().live();
            EXPECT_EQ(live, live_at_start) << "Heap allocations leaked by the test";
        }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::UnitTest::GetInstance()->listeners().Append(new LeakListener);
    return RUN_ALL_TESTS();
}
