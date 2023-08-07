#include "skip_list.hpp"

#include <optional>
#include <iostream>

SkipList::SkipList() {
    // TODO REFACTOR THIS -> MY HEAD IS HURTING RIGHT NOW D:
    Node *topHead = new Node(MIN_KEY);
    Node *topTail = new Node(MAX_KEY);
    topHead->successor.store({topTail, false, false});

    auto previousHead = topHead;
    auto previousTail = topTail;
    for (int i = 0;
         i < MAX_LEVEL - 1; i++) { // create for the whole length // -1 because we created the top nodes already
        Node *head = new Node(MIN_KEY, previousHead);
        Node *tail = new Node(MAX_KEY, previousTail);
        head->successor.store({tail, false, false});
        previousHead = head;
        previousTail = tail;
    }
    head = previousHead; // head is root head node
    tail = previousTail;

    // set down path
    auto iterator = head;
    // TODO check if they reference themselves or nullptr if root node (probably does not matter, since we are using levels and don't call down on level 1)
    auto previousIterator = head;
    // for head
    iterator->down = previousIterator;
    // for tail
    iterator->successor.load().right()->down = previousIterator->successor.load().right()->down;

    iterator = iterator->up;
    for (int i = 0; i < MAX_LEVEL - 1; i++) {
        // for head
        iterator->down = previousIterator;
        // for tail
        iterator->successor.load().right()->down = previousIterator->successor.load().right()->down;
        // iterate up
        iterator = iterator->up;
        previousIterator = previousIterator->up;
    }
}

bool SkipList::insert(Key key, Element element) {
    auto pair = searchToLevel(key, 1);
    auto prevNode = pair.first;
    auto nextNode = pair.second;

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
        auto prevNodeResultPair = insertNode(newNode, prevNode, nextNode);
        prevNode = prevNodeResultPair.first;
        auto result = prevNodeResultPair.second;
        if (result == nullptr && currV == 1) {
            // TODO might delete/free newRNode
            // key is already in list -> DUPLICATE_KEYS
            return false;
        }
        // check if tower became superfluous
        if (newRNode->successor.load().marked() == true) {
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

        auto returnPair = searchToLevel(key, currV);
        prevNode = returnPair.first;
        nextNode = returnPair.second;
    }
}

std::optional<Element> SkipList::find(Key key) {
    auto pair = searchToLevel(key, 1);
    auto currNode = pair.first;
    auto nextNode = pair.second;

    if (currNode->key == key) {
        return currNode->element;
    } else {
        return {}; // element not found
    }
}

std::optional<Element> SkipList::remove(Key key) {
    // TODO don't know if we can just use epsilon = 1 here
    auto pair = searchToLevel(key - 1, 1);
    auto prevNode = pair.first;
    auto delNode = pair.second;

    if (delNode->key != key) {
        return {}; // NO SUCH KEY
    }
    auto result = deleteNode(prevNode, delNode);
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
    Node *nextNode;
    Node *currNode;
    Level currV;

    std::tie(currNode, currV) = findStart(v);
    while (currV > v) {
        std::tie(currNode, nextNode) = searchRight(k, currNode);
        // TODO currNode down is not defined
        currNode = currNode->down;
        currV--;
    }
    std::tie(currNode, nextNode) = searchRight(k, currNode);
    return std::make_pair(currNode, nextNode);
}

std::pair<Node *, Level> SkipList::findStart(Level v) {
    auto *currNode = head;
    Level currV = 1;

    while (currNode->up->successor.load().right()->key != MAX_KEY || currV < v) {
        currNode = currNode->up;
        currV++;
    }

    return std::make_pair(currNode, currV);
}

std::pair<Node *, Node *> SkipList::searchRight(Key k, Node *currNode) {
    Node *nextNode = currNode->successor.load().right();
    bool status;
    bool _result; // don't need it

    while (nextNode->key <= k) {
        // NOTE: ADDED nextNode->towerRoot != nullptr, because otherwise we get a nullptr for the tail node, which does not have a successor
        while (nextNode->towerRoot != nullptr && nextNode->towerRoot->successor.load().marked() == true) {
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
            prevNode = prevNode->backLink;
        }
        // TODO don't know if we can just decrease by one (epsilon), but theoretically should be fine
        auto returnPair = searchRight(targetNode->key - 1, prevNode);
        prevNode = returnPair.first;
        auto delNode = returnPair.second;
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
        auto prevSuccessor = prevNode->successor.load();
        if (prevSuccessor.flagged() == true) {
            helpFlagged(prevNode, prevSuccessor.right());
        } else {
            // TODO don't think that works
            newNode->successor.store({nextNode, false, false});
            Successor newSuccessor = {newNode, false, false};
            auto result = CAS(prevNode->successor, {nextNode, false, false}, {newNode, false, false});

            if (result == newSuccessor) {
                return std::make_pair(prevNode, newNode);
            } else {

                if (result.flagged() == true) {
                    helpFlagged(prevNode, result.right());
                }
                while (prevNode->successor.load().marked() == true) {
                    prevNode = prevNode->backLink;
                }
            }
        }
        auto pair = searchRight(newNode->key, prevNode);
        prevNode = pair.first;
        nextNode = pair.second;

        if (prevNode->key == newNode->key) {
            return std::make_pair(prevNode, nullptr);
        }
    }
}

Node *SkipList::deleteNode(Node *prevNode, Node *delNode) {
    auto pair = tryFlagNode(prevNode, delNode);
    prevNode = std::get<0>(pair);
    bool status = std::get<1>(pair);
    bool result = std::get<2>(pair);

    if (status == true) {
        helpFlagged(prevNode, delNode);
    }
    if (result == false) {
        // NO SUCH NODE
        return nullptr;
    }
    return delNode;
}

void SkipList::helpMarked(Node *prevNode, Node *delNode) {
    auto nextNode = delNode->successor.load().right();
    CAS(prevNode->successor, {delNode, false, true}, {nextNode, false, false});
}

void SkipList::helpFlagged(Node *prevNode, Node *delNode) {
    delNode->backLink = prevNode;
    if (delNode->successor.load().marked() == false) {
        tryMark(delNode);
    }
    helpMarked(prevNode, delNode);
}

void SkipList::tryMark(Node *delNode) {
    do {
        auto nextNode = delNode->successor.load().right();
        auto result = CAS(delNode->successor, {nextNode, false, false}, {nextNode, true, false});

        if (result.flagged() == true) {
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
