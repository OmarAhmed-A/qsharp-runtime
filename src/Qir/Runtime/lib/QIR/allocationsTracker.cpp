// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "allocationsTracker.hpp"

#include "QirRuntime.hpp"

namespace Microsoft
{
namespace Quantum
{
    void AllocationsTracker::OnAllocate(void* object)
    {
        auto inserted = this->allocatedObjects.insert(std::make_pair(object, 1));
        if (inserted.second)
        {
            // first time we are allocating an object at this address, nothing to do
        }
        else
        {
            if (inserted.first->second > 0)
            {
                __quantum__rt__fail_cstr("Allocating an object over an existing object!");
            }
            else
            {
                inserted.first->second = 1;
            }
        }
    }

    void AllocationsTracker::OnAddRef(void* object)
    {
        auto tracked = this->allocatedObjects.find(object);
        if (tracked == this->allocatedObjects.end())
        {
            __quantum__rt__fail_cstr("Attempting to addref an object that isn't tracked!");
        }
        else
        {
            if (tracked->second <= 0)
            {
                __quantum__rt__fail_cstr("Attempting to ressurect a previously released object!");
            }
            else
            {
                tracked->second += 1;
            }
        }
    }

    void AllocationsTracker::OnRelease(void* object)
    {
        auto tracked = this->allocatedObjects.find(object);
        if (tracked == this->allocatedObjects.end())
        {
            __quantum__rt__fail_cstr("Attempting to release an object that isn't tracked!");
        }
        else
        {
            if (tracked->second <= 0)
            {
                __quantum__rt__fail_cstr("Attempting to release a previously released object!");
            }
            else
            {
                tracked->second -= 1;
            }
        }
    }

    void AllocationsTracker::CheckForLeaks() const
    {
        for (auto& tracked : this->allocatedObjects)
        {
            if (tracked.second > 0)
            {
                __quantum__rt__fail_cstr("Found a potentially leaked object!");
            }
        }
    }
} // namespace Quantum
} // namespace Microsoft
