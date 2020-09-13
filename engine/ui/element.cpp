#include "element.h"
#include "engine/task/ui_element_init.h"
#include "glm/gtc/matrix_transform.hpp"

namespace lotus::ui
{
    Element::Element()
    {

    }

    std::vector<UniqueWork> Element::Init(std::shared_ptr<Element> self)
    {
        std::vector<UniqueWork> ret;
        ret.push_back(std::make_unique<UiElementInitTask>(self));
        return ret;
    }

    glm::ivec2 Element::GetAbsolutePos()
    {
        return screen_pos;
    }

    glm::ivec2 Element::GetRelativePos()
    {
        return pos;
    }

    glm::ivec2 Element::GetAnchorOffset(AnchorPoint anchor)
    {
        glm::ivec2 pos{};

        if (anchor == AnchorPoint::TopRight || anchor == AnchorPoint::Right || anchor == AnchorPoint::BottomRight)
        {
            pos.x = width;
        }
        else if(anchor == AnchorPoint::Top || anchor == AnchorPoint::Center || anchor == AnchorPoint::Bottom)
        {
            pos.x = (width/2);
        }

        if (anchor == AnchorPoint::BottomLeft || anchor == AnchorPoint::Bottom || anchor == AnchorPoint::BottomRight)
        {
            pos.y = height;
        }
        else if(anchor == AnchorPoint::Left || anchor == AnchorPoint::Center || anchor == AnchorPoint::Right)
        {
            pos.y = (height/2);
        }
        return pos;
    }

    uint32_t Element::GetWidth()
    {
        return width;
    }

    uint32_t Element::GetHeight()
    {
        return height;
    }

    void Element::SetPos(glm::ivec2 _pos)
    {
        pos = _pos;
        RecalculateScreenPos();
    }

    void Element::SetWidth(uint32_t _width)
    {
        width = _width;
        RecalculateScreenPos();
    }

    void Element::SetHeight(uint32_t _height)
    {
        height = _height;
        RecalculateScreenPos();
    }

    void Element::RecalculateScreenPos()
    {
        if (parent)
        {
            RecalculateScreenPosFromParent(parent->GetAbsolutePos());
            for (auto& child : children)
            {
                child->RecalculateScreenPosFromParent(screen_pos);
            }
        }
    }

    void Element::RecalculateScreenPosFromParent(glm::ivec2 parent_pos)
    {
        screen_pos = parent_pos + parent->GetAnchorOffset(parent_anchor) + pos - GetAnchorOffset(anchor);
    }

    void Element::AddChild(std::shared_ptr<Element> child)
    {
        child->parent = this;
        auto iter = std::ranges::upper_bound(children, child->GetZ(), {}, &std::shared_ptr<Element>::element_type::GetZ);
        children.insert(iter, child);
        child->RecalculateScreenPosFromParent(screen_pos);
    }
}
