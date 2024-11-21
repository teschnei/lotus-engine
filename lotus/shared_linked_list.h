#pragma once
#include <memory>
#include <optional>
#include <vector>

namespace lotus
{
// Atomic queue
//  Does support getting single objects, but cannot report size
template <typename T> class SharedLinkedList
{
public:
    SharedLinkedList() {}
    SharedLinkedList(const SharedLinkedList<T>&) = delete;
    SharedLinkedList& operator=(const SharedLinkedList<T>&) = delete;
    SharedLinkedList(SharedLinkedList<T>&& o) noexcept
    {
        head = o.head.exchange({}, std::memory_order::relaxed);
        tail = o.tail.exchange({}, std::memory_order::relaxed);
    }
    SharedLinkedList& operator=(SharedLinkedList<T>&& o) noexcept
    {
        head = o.head.exchange({}, std::memory_order::seq_cst);
        tail = o.tail.exchange({}, std::memory_order::seq_cst);
        return *this;
    }

    struct Node
    {
        Node(T _val) : val(std::move(_val)) {}
        ~Node()
        {
            // pre-emptively delete the remaining nodes in order to save stack space
            //  note that this means once the head reference is gone, the children are also
            //  immediately deleted, so the head must remain somewhere while children are iterated
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
        if (auto tmp_head = head.load())
        {
            while (head.compare_exchange_weak(tmp_head, tmp_head->next, std::memory_order::seq_cst, std::memory_order::relaxed))
                ;
            if (tmp_head)
                return tmp_head->val;
        }
        return {};
    }

    std::vector<T> getAll()
    {
        auto head_reserve = head.exchange({}, std::memory_order::seq_cst);
        auto tail_reserve = tail.exchange({}, std::memory_order::seq_cst);
        std::vector<T> values;
        auto node = head_reserve;
        while (node)
        {
            values.push_back(std::move(node->val));
            node = node->next;
        }
        return values;
    }

    T& queue(T val)
    {
        auto new_node = std::make_shared<Node>(std::move(val));
        queueNode(new_node);
        return new_node->val;
    }

    void queue(SharedLinkedList<T>& o)
    {
        auto head_reserve = o.head.exchange({}, std::memory_order::seq_cst);
        auto tail_reserve = o.tail.exchange({}, std::memory_order::seq_cst);

        queueNode(head_reserve);
    }

private:
    void queueNode(std::shared_ptr<Node>& node)
    {
        do
        {
            auto tail_tmp = tail.load(std::memory_order::relaxed);
            if (tail.compare_exchange_weak(tail_tmp, node, std::memory_order::seq_cst, std::memory_order::relaxed))
            {
                if (tail_tmp)
                    tail_tmp->next = node;
                else
                    head.exchange(node, std::memory_order::relaxed);
                break;
            }
        } while (true);
    }
    std::atomic<std::shared_ptr<Node>> head;
    std::atomic<std::shared_ptr<Node>> tail;
};
} // namespace lotus
