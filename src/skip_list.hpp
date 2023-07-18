#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

using Key = int64_t;
using Element = int64_t;

// Invalid keys for head and tail towers. We will never insert a key == MIN/MAX_KEY.
constexpr Key MIN_KEY = std::numeric_limits<Key>::min();
constexpr Key MAX_KEY = std::numeric_limits<Key>::max();

/**
 * Main skip list class as described my Mikhail Fomitchev and Eric Ruppert in "Lock-Free Linked Lists and Skip Lists",
 * published at PODC '04, and the accompanying master thesis by Mikhail Fomitchev with the same name from 2003.
 *
 * The skip list maintains a sorted order of keys and offers a basic interface with the methods: insert, find, remove.
 * Your implementation should also provide a C++ iterator that allows us to use it as a container.
 * For implementation details, check out Chapter 4 in the master thesis. As this builds on some concepts introduced in
 * Chapter 3, some pointers are:
 *  - Section 3.1.2 contains info on the _mark_ bit.
 *  - Section 3.1.4 contains info on the _flag_ bit.
 *  - Section 4.4.2 contains a few optimization hints, which are useful for the optimized baseline.
 */
class SkipList {
 public:
  /** Construct the SkipList with all the members that you need. */
  SkipList();

  /** Get the Element associated with `key`. If the key is not found, return an empty optional. */
  std::optional<Element> find(Key key);

  /**
   * Insert the `element` associated with `key`. Return true on success, false otherwise.
   * Inserting a duplicate key returns false.
   **/
  bool insert(Key key, Element element);

  /**
   * Remove the `element` and `key` from the skip list. If the `element` was removed, return it. Otherwise, return an
   * empty optional to indicate that the key was not removed.
   */
  std::optional<Element> remove(Key key);

  // DO NOT CHANGE THESE.
  // These types are needed for the iterator interface.
  using Entry = std::pair<Key, Element>;
  // This says that the value type of a SkipList iterator is an `Entry`.
  using value_type = Entry;

  // TODO: Change this to the type of your custom iterator.
  //       We only use std::vector here as a placeholder to compile.
  //       Check for details on how to implement an iterator:
  //         - https://www.internalpointers.com/post/writing-custom-iterators-modern-cpp
  //         - https://en.cppreference.com/w/cpp/named_req/ForwardIterator
  //         - https://en.cppreference.com/w/cpp/iterator/iterator_traits
  //       Note: We only require a ForwardIterator.
  using Iterator = std::vector<Entry>::iterator;

  /// Begin and end iterators for the skip list to allow iterating over all entries.
  Iterator begin() const;
  Iterator end() const;

 private:
  // TODO: Add members and helper methods here.
  //       If you need a variable that each thread needs it's own copy of, have a look at thread_local variables. Note
  //       that you cannot have thread_local members, so you need to put them in the function where you need them.
  // ...
};
