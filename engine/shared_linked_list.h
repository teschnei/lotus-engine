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
            head = o.head;
        }
        SharedLinkedList& operator=(SharedLinkedList<T>&& o)
        {
            head = atomic_exchange_explicit(&o.head, {}, std::memory_order::seq_cst);
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
                return atomic_exchange_explicit(&head, head->next, std::memory_order::seq_cst);
            }
            return {};
        }

        std::vector<T> getAll()
        {
            auto node = atomic_exchange_explicit(&head, {}, std::memory_order::seq_cst);
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
            std::shared_ptr<Node> expected_head;
            do
            {
                expected_head = atomic_load_explicit(&head, std::memory_order::seq_cst);
                node->next = expected_head;
            } while (!atomic_compare_exchange_weak_explicit(&head, &expected_head, node, std::memory_order::seq_cst, std::memory_order::relaxed));
        }
    private:
        std::shared_ptr<Node> head;
    };
}

