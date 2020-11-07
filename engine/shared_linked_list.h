#pragma once
#include <memory>
#include <optional>
#include <vector>

namespace lotus
{
    template<typename T>
    class SharedLinkedList
    {
    public:
        SharedLinkedList() {}
        SharedLinkedList(const SharedLinkedList<T>&) = delete;
        SharedLinkedList& operator=(const SharedLinkedList<T>&) = delete;
        SharedLinkedList(SharedLinkedList<T>&& o)
        {
            head = o.head.exchange({}, std::memory_order::relaxed);
        }
        SharedLinkedList& operator=(SharedLinkedList<T>&& o)
        {
            head = o.head.exchange({}, std::memory_order::seq_cst);
            return *this;
        }

        struct Node
        {
            Node(T _val) : val(std::move(_val)) {}
            T val;
            std::shared_ptr<Node> next;
        };

        std::optional<T> get()
        {
            if (head)
            {
                return head.exchange(head->next, std::memory_order::seq_cst);
            }
            return {};
        }

        std::vector<T> getAll()
        {
            auto node = head.exchange({}, std::memory_order::seq_cst);
            std::vector<T> values;
            while(node)
            {
                values.push_back(std::move(node->val));
                node = node->next;
            }
            return values;
        }

        void queue(T val)
        {
            auto node = std::make_shared<Node>(std::move(val));
            node->next = head.load(std::memory_order::relaxed);
            while (!head.compare_exchange_weak(node->next, node, std::memory_order::seq_cst, std::memory_order::relaxed));
        }
    private:
        std::atomic<std::shared_ptr<Node>> head;
    };
}

