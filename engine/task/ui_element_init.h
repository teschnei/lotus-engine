#pragma once

#include "engine/work_item.h"
#include "engine/ui/element.h"

namespace lotus
{
    class UiElementInitTask : public WorkItem
    {
    public:
        UiElementInitTask(std::shared_ptr<ui::Element> _element);

        virtual void Process(WorkerThread*) override;
    private:
        std::shared_ptr<ui::Element> element;
    };
}
