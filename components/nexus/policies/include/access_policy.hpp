#pragma once

namespace demiplane::nexus {

    /**
     * Enum class representing possible access policies for managing instances in the `demiplane::nexus::Nexus` system.
     *
     * - Default: Indicates returning a raw pointer.
     * - View: Represents a constant view without a reference counter.
     * - Unique: Provides exclusive ownership to a unique handle (single pylon).
     * - Copy: Creates a deep or shallow copy of the instance.
     * - Safe: Implement a weak_ptr with runtime validation.
     */
    enum class AccessPolicy :uint8_t{
        Alias,
        View,
        Unique,
        Copy,
        Safe
    };
    constexpr auto DefaultAccessPolicy = AccessPolicy::Alias;
} // namespace demiplane::nexus
