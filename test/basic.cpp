#include <algorithm>
#include <array>
#include <barrier>
#include <numeric>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "skip_list.hpp"

#define matches_array(sl, expected)                                                   \
  ({                                                                                  \
    const std::vector<SkipList::Entry> result((sl).begin(), (sl).end());              \
                                                                                      \
    if (result.size() != (expected).size()) {                                         \
      GTEST_FAIL() << "Number of output elements do not match. Got " << result.size() \
                   << ", but expected: " << (expected).size();                        \
    }                                                                                 \
                                                                                      \
    if (!std::is_sorted(result.begin(), result.end())) {                              \
      GTEST_FAIL() << "Output is not sorted.";                                        \
    }                                                                                 \
                                                                                      \
    testing::Matcher<SkipList> element_matcher = testing::ElementsAreArray(expected); \
    if (!element_matcher.Matches(sl)) {                                               \
      GTEST_FAIL() << "Output matches in size and is sorted but certain elements "    \
                      "do not match.";                                                \
    }                                                                                 \
  })

#define matches_element(element, expected) \
  ASSERT_TRUE((element).has_value());      \
  ASSERT_EQ((element), (expected))

/// This is a quick test to see if your address sanitizer setup is correct.
/// If this does not fail with an Asan build, something is wrong.
// TEST(ExampleTest, TestASAN) {
//   std::array<int, 10> x{};
//   x.fill(1);
//   x[10] = 1;
//   int sum = 0;
//   for (int i = 0; i < 11; ++i) {
//     sum += x[i];
//   }
//   EXPECT_EQ(sum, 11);
// }

/// This is a quick test to see if your thread sanitizer setup is correct.
/// If this does not fail with a Tsan build, something is wrong.
// TEST(ExampleTest, TestTSAN) {
//   int counter = 0;
//
//   std::thread t1{[&] { counter++; }};
//   std::thread t2{[&] { counter++; }};
//
//   t1.join();
//   t2.join();
//
//   EXPECT_EQ(counter, 2);
// }
/////////////////////////////
///    SUCCESSOR TEST     ///
/////////////////////////////
TEST(SuccessorTest, Successor) {
    Successor emptySuccessor{};

    ASSERT_FALSE(emptySuccessor.marked());
    ASSERT_FALSE(emptySuccessor.flagged());

    Successor notEmptySuccessor{new Node(1), false, false};

    ASSERT_FALSE(notEmptySuccessor.marked());
    ASSERT_FALSE(notEmptySuccessor.flagged());

    Node* test = new Node(10);
    Successor markedSuccessor{test, true, false};

    ASSERT_TRUE(markedSuccessor.marked());
    ASSERT_FALSE(markedSuccessor.flagged());

    Successor flaggedSuccessor{test, false, true};

    ASSERT_FALSE(flaggedSuccessor.marked());
    ASSERT_TRUE(flaggedSuccessor.flagged());
}


/////////////////////////////
/// SINGLE-THREADED TESTS ///
/////////////////////////////

TEST(SingleThreadedSkipListTest, SimpleInsertAndFind) {
  SkipList sl{};

  ASSERT_TRUE(sl.insert(42, 100));
  std::optional<Element> element = sl.find(42);
  matches_element(element, 100);
}

TEST(SingleThreadedSkipListTest, InsertAndFind) {
  const int num_entries = 10;
  SkipList sl{};

  for (Key key = 0; key < num_entries; ++key) {
    ASSERT_TRUE(sl.insert(key, key * 10));
  }

  for (Key key = 0; key < num_entries; ++key) {
    std::optional<Element> element = sl.find(key);
    matches_element(element, key * 10);
  }
}

// Special test to check if your iterator interface is implemented correctly. We rely on this in the advanced tests, so
// we make sure that it works here.
TEST(SingleThreadedSkipListTest, IteratorInterface) {
  const int num_entries = 100;
  SkipList sl{};

  std::vector<SkipList::Entry> expected{};
  for (Key key = 0; key < num_entries; ++key) {
    ASSERT_TRUE(sl.insert(key, key));
    expected.emplace_back(key, key);
  }

  matches_array(sl, expected);

  for (Key key = 1; key < num_entries; key += 2) {
    ASSERT_TRUE(sl.remove(key).has_value());
  }

  expected.erase(
      std::remove_if(expected.begin(), expected.end(), [&](const SkipList::Entry& entry) { return entry.first % 2; }),
      expected.end());
  matches_array(sl, expected);
}

TEST(SingleThreadedSkipListTest, SimpleInsertAndRemove) {
  SkipList sl{};
  ASSERT_TRUE(sl.insert(10, 100));
  ASSERT_TRUE(sl.insert(11, 110));
  ASSERT_TRUE(sl.insert(12, 120));
  {
    std::optional<Element> element = sl.remove(11);
    matches_element(element, 110);
  }
  {
    std::optional<Element> element = sl.find(11);
    ASSERT_FALSE(element.has_value());
  }
  std::optional<Element> element10 = sl.find(10);
  matches_element(element10, 100);

  std::optional<Element> element12 = sl.find(12);
  matches_element(element12, 120);
}

////////////////////////////
/// MULTI-THREADED TESTS ///
////////////////////////////

TEST(MultiThreadedSkipListTest, InsertAndFind) {
  const int num_entries = 10;
  const int num_threads = 2;

  SkipList sl{};

  std::array<bool, 2> no_crash{};
  std::barrier start_threads{num_threads};
  auto insert_fn = [&](int id) {
    start_threads.arrive_and_wait();  // Wait for all threads to be ready.
    for (Key key = id; key < num_entries; key += num_threads) {
      sl.insert(key, key);
    }
    no_crash[id] = true;
  };

  std::thread t0{insert_fn, 0};
  std::thread t1{insert_fn, 1};

  t0.join();
  t1.join();

  for (bool crash : no_crash) {
    ASSERT_TRUE(crash) << "A thread crashed during this test.";
  }

  for (Key key = 0; key < num_entries; ++key) {
    std::optional<Element> element = sl.find(key);
    matches_element(element, key);
  }
}

TEST(MultiThreadedSkipListTest, InsertRemoveFind) {
  const int num_entries = 1000;
  const int num_threads = 2;

  SkipList sl{};

  std::mt19937 shuffle_rng{765345357};
  std::vector<Key> keys(num_entries);
  std::iota(keys.begin(), keys.end(), 0);
  std::shuffle(keys.begin(), keys.end(), shuffle_rng);

  std::vector<Key> t0_keys(keys.begin(), keys.begin() + (num_entries / 2));
  std::vector<Key> t1_keys(keys.begin() + (num_entries / 2), keys.end());

  std::array<bool, num_threads> no_crashes{};
  std::barrier start_threads{num_threads};
  auto insert_fn = [&](const std::vector<Key>& keys, bool* crash) {
    std::mt19937 rng(keys[0]);
    const size_t num_keys = keys.size();
    std::vector<Key> removed{};
    removed.reserve(num_keys);

    start_threads.arrive_and_wait();  // Wait for all threads to be ready.

    sl.insert(keys[0], keys[0]);
    size_t pos = 1;

    while (pos < num_keys) {
      uint32_t op = rng() % 4;

      switch (op) {
        case 0:
        case 1:
          sl.insert(keys[pos], keys[pos]);
          pos++;
          break;
        case 2:
          sl.find(keys[rng() % pos]);
          break;
        case 3:
          Key key = keys[rng() % pos];
          sl.remove(key);
          removed.push_back(key);
          break;
      }
    }

    std::unordered_set<Key> remaining_keys{keys.begin(), keys.end()};
    for (Key key_to_remove : removed) {
      remaining_keys.erase(key_to_remove);
    }

    for (Key key : remaining_keys) {
      std::optional<Element> element = sl.find(key);
      matches_element(element, key);
    }

    for (Key removed_key : removed) {
      ASSERT_FALSE(sl.find(removed_key).has_value());
    }

    *crash = true;
  };

  std::thread t0{insert_fn, t0_keys, &no_crashes[0]};
  std::thread t1{insert_fn, t1_keys, &no_crashes[1]};

  t0.join();
  t1.join();

  for (bool no_crash : no_crashes) {
    ASSERT_TRUE(no_crash) << "A thread crashed during this test.";
  }

  EXPECT_TRUE(std::is_sorted(sl.begin(), sl.end()));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
