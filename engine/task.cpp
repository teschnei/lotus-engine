#include "task.h"

namespace lotus
{
    Task<void> Promise<void>::get_return_object() noexcept
    {
        return Task<void>{coroutine_handle::from_promise(*this)};
    }
}
