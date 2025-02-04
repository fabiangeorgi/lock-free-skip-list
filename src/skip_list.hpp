#include <cstdint>
#include <limits>
#include <optional>
#include <vector>
#include <tuple>
#include <math.h>
#include <atomic>
#include <ctime>

using Key = int64_t;
using Element = int64_t;
using Level = uint64_t;

// Invalid keys for head and tail towers. We will never insert a key == MIN/MAX_KEY.
constexpr Key MIN_KEY = std::numeric_limits<Key>::min();
constexpr Key MAX_KEY = std::numeric_limits<Key>::max();

// Maximum of 8 Million keys will be inserted -> can calculate tower height
constexpr uint64_t MAX_NUMBER_OF_KEYS = 8000000;
// maxLevel of the tower -> log_(1/p)_(N) -> all not head or tail towers will be strictly smaller
// because p=0.5, -> log2(M)
// TODO constexpr does not work with log operations -> find a fix
// log2(MAX_NUMBER_OF_KEYS) = 22
// use p=0.25 -> log4(M) = 11
constexpr uint64_t MAX_LEVEL = 22;

// forward declare
struct Node;

struct Successor {
    Successor() = default;

    Successor(Node *right, bool marked, bool flagged);;

    Node *right();

    bool marked() const;

    bool flagged() const;

    bool operator==(Successor const &other) const;

private:
    uint64_t internal64BitData;

    // mostly copy and paste from the buffer manager assignment -> using swips, because an atomic datatype can only store 64 bits (e.g. one pointer for node)
    // so need to do pointer tagging

    // x10
    static constexpr uint64_t markedBits = uint64_t(2);
    // x01
    static constexpr uint64_t flaggedBits = uint64_t(1);

    // 3 = x11
    static constexpr uint64_t comparisonMask = uint64_t(3);

    // we need this to get the address of our node, regardless if its marked or flagged -> so we just zero it out
    static constexpr uint64_t pointerMask = ~comparisonMask;
};

struct alignas(8) Node {
    // constructs a root node
    Node(Key key, Element element);

    // one node in tower
    Node(Key key, Node *down, Node *towerRoot);

    // Pointer to the previous Node
    std::atomic<Node *> backLink;
    // Stores next node (right), and if node is marked OR flagged
    std::atomic<Successor> successor;
    // A pointer to the node below, or null if root node (lowest level)
    Node *down;
    // A pointer to the root of the tower. Root Nodes will reference themselves.
    Node *towerRoot;

    std::pair<Key, Element> entry;

    // ONLY FOR HEAD-NODES
    // A points to the node above in tower or on itself if top of tower
    Node *up;

    Key key() const {
        return entry.first;
    }

    Element element() const {
        return entry.second;
    }
};


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

    struct SkipListIterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = SkipList::Entry;
        using pointer = SkipList::Entry *;
        using reference = SkipList::Entry &;

        SkipListIterator(Node *ptr);

        reference operator*() const;

        pointer operator->();

        SkipListIterator &operator++();

        SkipListIterator operator++(int);

        friend bool operator==(const SkipListIterator &a, const SkipListIterator &b);;

        friend bool operator!=(const SkipListIterator &a, const SkipListIterator &b);;

    private:
        Node *m_ptr;
    };

    using Iterator = SkipListIterator;

    /// Begin and end iterators for the skip list to allow iterating over all entries.
    Iterator begin() const;

    Iterator end() const;

    void print();

private:
    // starts from the head tower and searches for two consecutive nodes on level v, such that the first has a key less than or euqal to k, and the second has a key stricly greater than k
    std::pair<Node *, Node *> searchToLevel(Key k, Level v);

    // caches all the search results on every level
    void searchToLevelAndCacheResults(Key k, std::vector<std::pair<Node *, Node *>> &cache);

    // Searches the head tower for the lowest node that points to the tail tower
    std::pair<Node *, Level> findStart(Level v);

    // starts from currentNode and searches the level for two consecutive nodes such that the first has a key less or equal to k, and the second has a key strictly greater than k
    std::pair<Node *, Node *> searchRight(Key k, Node *currNode);

    // attempts to flag the predecessor of targetNode
    std::tuple<Node *, bool, bool> tryFlagNode(Node *prevNode, Node *targetNode);

    // attempts to insert node newNode into the list. Nodes prevNode and nextNode specify the position where insertNode will attempt to insert newNode
    std::pair<Node *, Node *> insertNode(Node *newNode, Node *prevNode, Node *nextNode);

    // attempts to delete node delNode
    Node *deleteNode(Node *prevNode, Node *delNode);

    // attempts to physically delete the marked node delNode
    void helpMarked(Node *prevNode, Node *delNode);

    // attempts to mark and physically delete the successor of the flagged node prevNode
    void helpFlagged(Node *prevNode, Node *delNode);

    // attempts to mark the node delNode
    void tryMark(Node *delNode);

    Successor CAS(std::atomic<Successor> &address, Successor old, Successor newValue) const;

    int flipCoin();

    Node *head;

    Node *tail;
};
