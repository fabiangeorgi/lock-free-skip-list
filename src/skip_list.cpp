#include "skip_list.hpp"

#include <optional>
#include <iostream>

SkipList::SkipList() {
    head = new Node(MIN_KEY, 0);
    head->up = head;
    tail = new Node(MAX_KEY, 0);

    head->successor.store({tail, false, false});

    Node* iteratorHead = head;
    Node* iteratorTail = tail;
    for (int i = 0; i < MAX_LEVEL; i++) {
        Node* headNode = new Node(MIN_KEY, iteratorHead, head);
        Node* tailNode = new Node(MAX_KEY, iteratorTail, tail);

        headNode->successor.store({tailNode, false, false});

        iteratorHead->up = headNode;
        iteratorTail->up = tailNode;

        iteratorHead = headNode;
        iteratorTail = tailNode;
    }
}

bool SkipList::insert(Key key, Element element) {
    Node *prevNode;
    Node *nextNode;
    std::tie(prevNode, nextNode) = searchToLevel(key, 1);

    if (prevNode->key == key) {
        // key is already in list -> DUPLICATE_KEYS
        return false;
    }
    // create the new root node
    Node *newRNode = new Node(key, element);
    Node *newNode = newRNode;

    // determine the desired height of the tower
    uint64_t towerHeight = 1;
    while (rand() % 2 && towerHeight <= MAX_LEVEL - 1) {
        towerHeight++;
    }

    // the level at which newNode will be inserted
    uint64_t currV = 1;
    while (true) {
        Node *result;
        std::tie(prevNode, result) = insertNode(newNode, prevNode, nextNode);

        if (result == nullptr && currV == 1) {
            // key is already in list -> DUPLICATE_KEYS
            return false;
        }
        // check if tower became superfluous
        if (newRNode->successor.load().marked()) {
            if (result == newNode && newNode != newRNode) {
                deleteNode(prevNode, newNode);
            }
            return true;
        }
        currV++;
        if (currV == towerHeight + 1) { // stop building the tower
            return true;
        }
        auto lastNode = newNode;
        newNode = new Node(key, lastNode, newRNode);

        std::tie(prevNode, nextNode) = searchToLevel(key, currV);
    }
}

std::optional<Element> SkipList::find(Key key) {
    Node *currNode;
    Node *nextNode;
    std::tie(currNode, nextNode) = searchToLevel(key, 1);

    if (currNode->key == key) {
        return currNode->element;
    } else {
        return {}; // element not found
    }
}

std::optional<Element> SkipList::remove(Key key) {
    Node *prevNode;
    Node *delNode;
    std::tie(prevNode, delNode) = searchToLevel(key - 1, 1);

    if (delNode->key != key) {
        return {}; // NO SUCH KEY
    }
    Node *result = deleteNode(prevNode, delNode);
    if (result == nullptr) {
        return {}; // NO SUCH KEY
    }
    searchToLevel(key, 2); // deletes the nodes at the higher levels of the tower
    return delNode->element;
}

SkipList::Iterator SkipList::begin() const { return Iterator(head->successor.load().right()); }

SkipList::Iterator SkipList::end() const { return Iterator(tail); }

std::pair<Node *, Node *> SkipList::searchToLevel(Key k, Level v) {
    // we declare here to unroll in while loop directly
    Node *currNode;
    Level currV;

    std::tie(currNode, currV) = findStart(v);
    while (currV > v) {
        Node *nextNode;
        std::tie(currNode, nextNode) = searchRight(k, currNode);
        currNode = currNode->down;
        currV--;
    }
    auto result = searchRight(k, currNode);
    return result;
}

std::pair<Node *, Level> SkipList::findStart(Level v) {
    auto *currNode = head;
    Level currV = 1;

    // TODO check nullptr (if up does not reference itself)
    while (currNode->up->successor.load().right()->key != MAX_KEY || currV < v) {
        currNode = currNode->up;
        currV++;
    }

    return std::make_pair(currNode, currV);
}

std::pair<Node *, Node *> SkipList::searchRight(Key k, Node *currNode) {
    Node *nextNode = currNode->successor.load().right();

    while (nextNode->key <= k) {
        // NOTE: ADDED towerRoot pointers for tail nodes, because otherwise we get a nullptr for the tail node, which does not have a successor
        while (nextNode->towerRoot->successor.load().marked()) {
            bool status;
            bool _result; // don't need it

            std::tie(currNode, status, _result) = tryFlagNode(currNode, nextNode);
            if (status == true) { // INSERTED
                helpFlagged(currNode, nextNode);
            }
            nextNode = currNode->successor.load().right();
        }
        if (nextNode->key <= k) {
            currNode = nextNode;
            nextNode = currNode->successor.load().right();
        }
    }
    return std::make_pair(currNode, nextNode);
}

std::tuple<Node *, bool, bool> SkipList::tryFlagNode(Node *prevNode, Node *targetNode) {
    while (true) {
        Successor flaggedPredecessor = {targetNode, false, true};
        if (prevNode->successor.load() == flaggedPredecessor) {
            // INSERTED = true (2nd argument)
            return std::make_tuple(prevNode, true, false);
        }

        Successor oldSuccessor = {targetNode, false, false};
        auto result = CAS(prevNode->successor, oldSuccessor, flaggedPredecessor);

        if (result == flaggedPredecessor) { // if compare and swap was successful
            return std::make_tuple(prevNode, true, true);
        }
        // compare and swap was not successful -> handle error
        // get new value of node and check if flagged
//        auto currentSuccessor = prevNode->successor.load();
//        if (currentSuccessor == flaggedPredecessor) {
//            return std::make_tuple(prevNode, true, false);
//        }
        while (prevNode->successor.load().marked()) { // possibly failure due to marking -> use back_links
            prevNode = prevNode->backLink.load();
        }
        Node *delNode;
        std::tie(prevNode, delNode) = searchRight(targetNode->key - 1, prevNode);

        if (delNode != targetNode) {
            return std::make_tuple(prevNode, false, false); // target node was deleted from the list
        }
    }
}

// use pair to get node and bool
// if second node is nullptr -> duplicate keys
std::pair<Node *, Node *> SkipList::insertNode(Node *newNode, Node *prevNode, Node *nextNode) {
    if (prevNode->key == newNode->key) {
        return std::make_pair(prevNode, nullptr);
    }
    while (true) {
        std::atomic<Successor> &prevSuccessor = prevNode->successor;
        if (prevSuccessor.load().flagged()) {
            helpFlagged(prevNode, prevSuccessor.load().right());
        } else {
            newNode->successor = {nextNode, false, false};
            Successor newSuccessor = {newNode, false, false};
            auto result = CAS(prevSuccessor, {nextNode, false, false}, newSuccessor);

            if (result == newSuccessor) {
                return std::make_pair(prevNode, newNode);
            } else {
                if (result.flagged()) {
                    helpFlagged(prevNode, result.right());
                }
                while (prevNode->successor.load().marked()) {
                    prevNode = prevNode->backLink.load();
                }

            }
        }

        std::tie(prevNode, nextNode) = searchRight(newNode->key, prevNode);

        if (prevNode->key == newNode->key) {
            return std::make_pair(prevNode, nullptr);
        }
    }
}

Node *SkipList::deleteNode(Node *prevNode, Node *delNode) {
    bool status;
    bool result;

    std::tie(prevNode, status, result) = tryFlagNode(prevNode, delNode);

    if (status) {
        helpFlagged(prevNode, delNode);
    }
    if (result == false) {
        // NO SUCH NODE
        return nullptr;
    }
    return delNode;
}

void SkipList::helpMarked(Node *prevNode, Node *delNode) {
    Node* nextNode = delNode->successor.load().right();
    CAS(prevNode->successor, {delNode, false, true}, {nextNode, false, false});
}

void SkipList::helpFlagged(Node *prevNode, Node *delNode) {
    delNode->backLink.store(prevNode);
    if (delNode->successor.load().marked() == false) {
        tryMark(delNode);
    }
    helpMarked(prevNode, delNode);
}

void SkipList::tryMark(Node *delNode) {
    do {
        Node* nextNode = delNode->successor.load().right();
        Successor result = CAS(delNode->successor, {nextNode, false, false}, {nextNode, true, false});

        if (result.flagged()) {
            helpFlagged(delNode, result.right());
        }
    } while (delNode->successor.load().marked() != true);
}

void SkipList::print() {
    auto headIterator = head;

    while (headIterator->up != headIterator) {
        auto listIterator = headIterator->successor.load().right();
        if (listIterator->key == MAX_KEY) {
            std::cout << std::endl;
            break; // don't show empty trees
        }
        std::cout << "HEAD => ";
        while (listIterator->key != MAX_KEY) {
            std::cout << listIterator->key << " => ";
            listIterator = listIterator->successor.load().right();
        }
        std::cout << "END" << std::endl;
        headIterator = headIterator->up;
    }
    std::cout << std::endl;
}
