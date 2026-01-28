#pragma once

namespace SDL_Client
{
    class DoublyLinkedList {
    public:
        class Node {
        public:
            Node* prev = nullptr;
            Node* next = nullptr;

            virtual ~Node() = default;

            void unlink() {
                if (next != nullptr) {
                    next->prev = prev;
                    prev->next = next;
                    prev = nullptr;
                    next = nullptr;
                }
            }

            bool isLinked() const {
                return next != nullptr;
            }
        };

    private:
        Node head;
        Node* peeked = nullptr;

    public:
        DoublyLinkedList() {
            head.prev = &head;
            head.next = &head;
        }

        ~DoublyLinkedList() {
            clear();
        }

        // Delete copy operations - copying a linked list with raw pointers is dangerous
        DoublyLinkedList(const DoublyLinkedList&) = delete;
        DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;

        // Move operations
        DoublyLinkedList(DoublyLinkedList&& other) noexcept {
            head. prev = &head;
            head. next = &head;

            if (! other.isEmpty()) {
                // Take ownership of other's nodes
                head.next = other.head. next;
                head.prev = other. head.prev;
                head.next->prev = &head;
                head. prev->next = &head;

                // Reset other to empty state
                other.head.prev = &other.head;
                other.head. next = &other. head;
            }
            peeked = other. peeked;
            other.peeked = nullptr;
        }

        DoublyLinkedList& operator=(DoublyLinkedList&& other) noexcept {
            if (this != &other) {
                clear();

                if (!other.isEmpty()) {
                    head.next = other. head.next;
                    head.prev = other.head. prev;
                    head.next->prev = &head;
                    head.prev->next = &head;

                    other.head.prev = &other.head;
                    other.head.next = &other.head;
                }
                peeked = other. peeked;
                other.peeked = nullptr;
            }
            return *this;
        }

        void pushBack(Node* node) {
            if (node->next != nullptr) {
                node->unlink();
            }
            node->next = head.next;
            node->prev = &head;
            node->next->prev = node;
            node->prev->next = node;
        }

        void pushFront(Node* node) {
            if (node->next != nullptr) {
                node->unlink();
            }
            node->next = &head;
            node->prev = head. prev;
            node->next->prev = node;
            node->prev->next = node;
        }

        Node* pollFront() {
            Node* node = head.prev;
            if (node == &head) {
                return nullptr;
            } else {
                node->unlink();
                return node;
            }
        }

        Node* peekFront() {
            Node* node = head.prev;
            if (node == &head) {
                peeked = nullptr;
                return nullptr;
            } else {
                peeked = node->prev;
                return node;
            }
        }

        Node* peekBack() {
            Node* node = head.next;
            if (node == &head) {
                peeked = nullptr;
                return nullptr;
            } else {
                peeked = node->next;
                return node;
            }
        }

        Node* prev() {
            Node* node = peeked;
            if (node == &head) {
                peeked = nullptr;
                return nullptr;
            } else {
                peeked = node->prev;
                return node;
            }
        }

        Node* next() {
            Node* node = peeked;
            if (node == &head) {
                peeked = nullptr;
                return nullptr;
            }
            peeked = node->next;
            return node;
        }

        void clear() {
            while (head.prev != &head) {
                Node* node = head. prev;
                node->unlink();
                // Note: This does NOT delete the node - caller owns the memory
            }
        }

        bool isEmpty() const {
            return head.prev == &head;
        }
    };
}
