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
        SharedLinkedList(SharedLinkedList<T>&& o) noexcept
        {
            head = o.head.exchange({}, std::memory_order::relaxed);
        }
        SharedLinkedList& operator=(SharedLinkedList<T>&& o) noexcept
        {
            head = o.head.exchange({}, std::memory_order::seq_cst);
            return *this;
        }

        struct Node
        {
            Node(T _val) : val(std::move(_val)) {}
            ~Node()
            {
                //pre-emptively delete the remaining nodes in order to save stack space
                // note that this means once the head reference is gone, the children are also
                // immediately deleted, so the head must remain somewhere while children are iterated
                if (next)
                {
                    auto n = next;
                    while (n)
                    {
                        auto t = n;
                        n = t->next;
                        t->next.reset();
                    }
                }
            }
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
            auto head_reserve = head.exchange({}, std::memory_order::seq_cst);
            std::vector<T> values;
            auto node = head_reserve;
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

