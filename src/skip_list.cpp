#include "skip_list.hpp"

#include <optional>
#include <iostream>
#include <random>

/*
 * NODE
 */
Node::Node(Key key, Element element) : backLink(nullptr), down(nullptr), towerRoot(this),
                                       entry(std::make_pair(key, element)),
                                       up(nullptr) {}

Node::Node(Key key, Node *down, Node *towerRoot) : backLink(nullptr), down(down), towerRoot(towerRoot),
                                                   entry(std::make_pair(key, 0)), up(nullptr) {}


/*
* SUCCESSOR
*/
Successor::Successor(Node *right, bool marked, bool flagged) : internal64BitData(reinterpret_cast<uint64_t>(right)) {
    if (marked) {
        internal64BitData = internal64BitData | markedBits;
    } else if (flagged) {
        internal64BitData = internal64BitData | flaggedBits;
    }
}

Node *Successor::right() {
    return reinterpret_cast<Node *>(internal64BitData & pointerMask);
}

bool Successor::marked() const {
    return (internal64BitData & markedBits);
}

bool Successor::flagged() const {
    return (internal64BitData & flaggedBits);
}

bool Successor::operator==(const Successor &other) const {
    return internal64BitData == other.internal64BitData;
}

/*
 * SKIP LIST ITERATOR
 */
SkipList::Iterator::SkipListIterator(Node *ptr) : m_ptr(ptr) {}

SkipList::Iterator::pointer SkipList::Iterator::operator->() { return &(m_ptr->entry); }

SkipList::Entry &SkipList::Iterator::operator*() const { return m_ptr->entry; }

SkipList::Iterator &SkipList::Iterator::operator++() { m_ptr = m_ptr->successor.load().right(); return *this; }

SkipList::SkipListIterator SkipList::SkipListIterator::operator++(int) { SkipListIterator tmp = *this; ++(*this); return tmp; }

bool operator!=(const SkipList::SkipListIterator &a, const SkipList::SkipListIterator &b) { return a.m_ptr != b.m_ptr; }

bool operator==(const SkipList::SkipListIterator &a, const SkipList::SkipListIterator &b) { return a.m_ptr == b.m_ptr; }

/*
 * SKIPLIST
 */
SkipList::SkipList() {
    head = new Node(MIN_KEY, 0);
    tail = new Node(MAX_KEY, 0);

    head->successor.store({tail, false, false});

    Node *iteratorHead = head;
    Node *iteratorTail = tail;
    for (int i = 0; i < MAX_LEVEL; i++) {
        Node *headNode = new Node(MIN_KEY, iteratorHead, head);
        Node *tailNode = new Node(MAX_KEY, iteratorTail, tail);

        headNode->successor.store({tailNode, false, false});

        iteratorHead->up = headNode;
        iteratorTail->up = tailNode;

        iteratorHead = headNode;
        iteratorTail = tailNode;
    }
    // In the paper it states that top head should reference itself with up, but is not needed
    // iteratorHead->up = iteratorHead;
}

/*
 * Insert new Node/Tower into Skip List
 */
bool SkipList::insert(Key key, Element element) {
    // search correct place to insert Node/Tower
    std::vector<std::pair<Node*, Node*>> cache = searchToLevelAndCacheResults(key);

    Node *prevNode;
    Node *nextNode;

    // level 1 result
    std::tie(prevNode, nextNode) = cache[1];

    // check if tower already exists
    if (prevNode->key() == key) {
        // key is already in list -> DUPLICATE_KEYS
        return false;
    }

    // create the new root node
    Node *newRNode = new Node(key, element);
    Node *newNode = newRNode; // pointer to node currently inserted into tower

    // determine the desired height of the tower
    Level towerHeight = 1;
    while (flipCoin() && towerHeight <= MAX_LEVEL - 1) {
        towerHeight++;
    }

    // the level at which newNode will be inserted
    Level currV = 1;
    Node *result;
    // for each iteration increase the height of the new tower by 1
    while (true) {
        std::tie(prevNode, result) = insertNode(newNode, prevNode, nextNode);

        // did not even insert root node
        if (result == nullptr && currV == 1) {
            // key is already in list -> DUPLICATE_KEYS
            return false;
        }

        // check if tower became superfluous
        // root node was already inserted, but will now be deleted
        if (newRNode->successor.load().marked()) {
            // if not a root node, delete it
            if (result == newNode && newNode != newRNode) {
                deleteNode(prevNode, newNode);
            }
            return true;
        }

        currV++;
        // stop building the tower -> got desired height can stop now and return successful insert
        if (currV == towerHeight + 1) {
            return true;
        }

        auto lastNode = newNode;
        // create new node with correct down and towerRoot pointers
        newNode = new Node(key, lastNode, newRNode);

        // search correct interval to insert on next level
        if (cache[currV].first == nullptr) {
            std::tie(prevNode, nextNode) = searchToLevel(key, currV);
        } else {
            std::tie(prevNode, nextNode) = cache[currV];
        }
    }
}

/*
 * finds and returns the element of desired key or empty result
 */
std::optional<Element> SkipList::find(Key key) {
    Node *currNode;
    Node *nextNode;
    // find root note with firstNode <= key < secondNode
    std::tie(currNode, nextNode) = searchToLevel(key, 1);

    if (currNode->key() == key) {
        return currNode->element();
    } else {
        return {}; // element not found
    }
}

/*
 * removes key from skip list and returns element if successful or empty result else
 */
std::optional<Element> SkipList::remove(Key key) {
    Node *prevNode;
    Node *delNode;
    std::tie(prevNode, delNode) = searchToLevel(key - 1, 1);

    // key is not found in the list
    if (delNode->key() != key) {
        return {}; // NO SUCH KEY
    }

    // try to delete
    Node *result = deleteNode(prevNode, delNode);
    if (result == nullptr) {
        // deletion was not successful
        return {}; // NO SUCH KEY
    }
    // deletes the nodes at the higher levels of the tower, because search deletes superfluous nodes
    searchToLevel(key, 2);
    return delNode->element();
}

SkipList::Iterator SkipList::begin() const { return Iterator(head->successor.load().right()); }

SkipList::Iterator SkipList::end() const { return Iterator(tail); }

/*
 * Performs the searches in the skip list
 */
std::pair<Node *, Node *> SkipList::searchToLevel(Key k, Level v) {
    // we declare here to unroll in while loop directly
    Node *currNode;
    Level currV;

    // lowest node in head tower that points to tail tower AND is of level v or higher
    std::tie(currNode, currV) = findStart(v);
    // searches on different levels (using the skip connections in skip list)
    while (currV > v) {
        Node *nextNode;
        std::tie(currNode, nextNode) = searchRight(k, currNode);
        currNode = currNode->down;
        currV--;
    }
    // searches on level v and returns result
    auto result = searchRight(k, currNode);
    return result;
}

/*
 * Finds lowest node in head tower that points to tail tower AND is of level v or higher
 */
std::pair<Node *, Level> SkipList::findStart(Level v) {
    auto *currNode = head;
    Level currV = 1;

    while (currNode->up->successor.load().right()->key() != MAX_KEY || currV < v) {
        currNode = currNode->up;
        currV++;
    }

    return std::make_pair(currNode, currV);
}

/*
 * Searches Linked List on Level of currNode
 * returns currNode and nextNode with following properties
 * 1. currNode.next = nextNode
 * 2. currNode.key <= k < nextNode
 */
std::pair<Node *, Node *> SkipList::searchRight(Key k, Node *currNode) {
    Node *nextNode = currNode->successor.load().right();
    bool status;
    bool _result; // don't need it

    while (nextNode->key() <= k) {
        // routine to delete superfluous nodes along the way when searching
        // NOTE: ADDED towerRoot pointers for tail nodes, because otherwise we get a nullptr for the tail node, which does not have a successor
        while (nextNode->towerRoot->successor.load().marked()) {
            // flag currNode (nextNode's predecessor)
            std::tie(currNode, status, _result) = tryFlagNode(currNode, nextNode);
            // check if currNode (nextNode's predecessor) was flagged
            if (status == true) { // STILL IN LIST
                // physically delete nextNode
                helpFlagged(currNode, nextNode);
            }
            nextNode = currNode->successor.load().right();
        }

        if (nextNode->key() <= k) {
            currNode = nextNode;
            nextNode = currNode->successor.load().right();
        }
    }
    return std::make_pair(currNode, nextNode);
}

/*
 * Tries to flag predecessor of node
 * returns non-null pointer of node it tried to flag
 */
std::tuple<Node *, bool, bool> SkipList::tryFlagNode(Node *prevNode, Node *targetNode) {
    while (true) {
        // check if predecessor is already flagged
        Successor flaggedPredecessor = {targetNode, false, true};
        if (prevNode->successor.load() == flaggedPredecessor) {
            // IN List = true (2nd argument)
            return std::make_tuple(prevNode, true, false);
        }

        // flag it
        Successor oldSuccessor = {targetNode, false, false};
        Successor result = CAS(prevNode->successor, oldSuccessor, flaggedPredecessor);

        // if it worked return
        if (result == flaggedPredecessor) {
            return std::make_tuple(prevNode, true, true);
        }
        // compare and swap was not successful -> handle error
        // use back link if prevNode is marked
        while (prevNode->successor.load().marked()) {
            prevNode = prevNode->backLink.load();
        }

        // check if we can still find target node
        Node *delNode;
        std::tie(prevNode, delNode) = searchRight(targetNode->key() - 1, prevNode);

        // check if target node was deleted from the list
        if (delNode != targetNode) {
            return std::make_tuple(prevNode, false, false);
        }
    }
}

/*
 * get called with three nodes that had at some points the following properties:
 * - prevNode.key <= newNode.key < nextNode.key
 * Keep in mind: This information might be outdated now!
 * Return values:
 * - first Node is prevNode's last information (in case of successful insert it is nextNode predecessor)
 * - second Node is either newNode (in case of successful insert) or nullptr (if failed)
 */
std::pair<Node *, Node *> SkipList::insertNode(Node *newNode, Node *prevNode, Node *nextNode) {
    if (prevNode->key() == newNode->key()) {
        // DUPLICATE KEYS
        return std::make_pair(prevNode, nullptr);
    }

    while (true) {
        std::atomic<Successor> &prevSuccessor = prevNode->successor;
        if (prevSuccessor.load().flagged()) {
            // successor will be deleted (prevNode is flagged) -> help
            helpFlagged(prevNode, prevSuccessor.load().right());
        } else {
            // set successor
            newNode->successor = {nextNode, false, false};
            Successor newSuccessor = {newNode, false, false};
            auto result = CAS(prevSuccessor, {nextNode, false, false}, newSuccessor);

            if (result == newSuccessor) {
                // successfully inserted node
                return std::make_pair(prevNode, newNode);
            } else {
                // could not insert (either flagged or marked)
                if (result.flagged()) {
                    // prevNode is flagged
                    helpFlagged(prevNode, result.right());
                }
                while (prevNode->successor.load().marked()) {
                    // move prevNode via backlinks until it is not marked
                    prevNode = prevNode->backLink.load();
                }

            }
        }

        // search new correct interval for insertion
        std::tie(prevNode, nextNode) = searchRight(newNode->key(), prevNode);

        // was already inserted
        if (prevNode->key() == newNode->key()) {
            return std::make_pair(prevNode, nullptr);
        }
    }
}

/*
 * get previousNode and the node that shall be deleted
 *
 */
Node *SkipList::deleteNode(Node *prevNode, Node *delNode) {
    bool status;
    bool result;

    // try to flag prevNode
    std::tie(prevNode, status, result) = tryFlagNode(prevNode, delNode);

    if (status) {
        // flagging worked -> physically delete delNode
        helpFlagged(prevNode, delNode);
    }

    if (result == false) {
        // it did not flag the node by itself -> return nullptr, node was not found
        // NO SUCH NODE
        return nullptr;
    }
    return delNode;
}

/*
 * physically deletes a node by swinging prevNode's successor pointer to delNode's successor pointer
 * will remove the flag tag in prevNode
 */
void SkipList::helpMarked(Node *prevNode, Node *delNode) {
    Node *nextNode = delNode->successor.load().right();
    CAS(prevNode->successor, {delNode, false, true}, {nextNode, false, false});
}

/*
 * prevNode is flagged and delNode should be deleted
 *
 */
void SkipList::helpFlagged(Node *prevNode, Node *delNode) {
    // setting backlink
    delNode->backLink.store(prevNode);
    if (delNode->successor.load().marked() == false) {
        // mark the delNode if it is not already marked
        tryMark(delNode);
    }
    // physically delete the node
    helpMarked(prevNode, delNode);
}

/*
 * mark delNode -> does not stop until it is marked (either itself or other process)
 */
void SkipList::tryMark(Node *delNode) {
    do {
        Node *nextNode = delNode->successor.load().right();
        Successor result = CAS(delNode->successor, {nextNode, false, false}, {nextNode, true, false});
        // C&S can fail if either result is flagged or delNode's right pointer changed
        if (result.flagged()) {
            // node that should be marked is currently flagged -> try to remove flag
            helpFlagged(delNode, result.right());
        }
    } while (delNode->successor.load().marked() != true);
}

void SkipList::print() {
    auto headIterator = head;

    while (headIterator->up != headIterator) {
        auto listIterator = headIterator->successor.load().right();
        if (listIterator->key() == MAX_KEY) {
            std::cout << std::endl;
            break; // don't show empty trees
        }
        std::cout << "HEAD => ";
        while (listIterator->key() != MAX_KEY) {
            std::cout << listIterator->key() << " => ";
            listIterator = listIterator->successor.load().right();
        }
        std::cout << "END" << std::endl;
        headIterator = headIterator->up;
    }
    std::cout << std::endl;
}

int SkipList::flipCoin() {
    static thread_local std::mt19937 generator;
    std::uniform_int_distribution<int> distribution(0, 1);
    return distribution(generator);
}

Successor SkipList::CAS(std::atomic<Successor> &address, Successor old, Successor newValue) const {
    if (address.compare_exchange_weak(old, newValue)) {
        return newValue;
    }
    return address;
}

std::vector<std::pair<Node *, Node *>> SkipList::searchToLevelAndCacheResults(Key k) {
    // we declare here to unroll in while loop directly
    Node *currNode = head;
    Level currV = 1;

    while (currNode->successor.load().right()->key() != MAX_KEY) {
        currV++;
        currNode = currNode->up;
    }

    std::vector<std::pair<Node*, Node*>> cache(MAX_LEVEL + 1);

    // searches on different levels (using the skip connections in skip list)
    while (currV >= 1) {
        Node *nextNode;
        std::tie(currNode, nextNode) = searchRight(k, currNode);
        cache[currV] = {currNode, nextNode};
        currNode = currNode->down;
        currV--;
    }

    return cache;
}
