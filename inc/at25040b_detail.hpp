/**
 * @file at25040b_detail.hpp
 * @brief Compile-time helpers of the AT25040B driver: exact hook matching, typed register fields
 * and enumeration checks.
 * @copyright Copyright (c) 2026 The driver-at25040b authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is `constexpr` or used only in unevaluated operands, so it adds no code to the
 * run-time path (handbook C++20 profile, KB-CPP20-006, -008, -020, -021, -025, -026).
 */
#ifndef AT25040B_DETAIL_HPP
#define AT25040B_DETAIL_HPP

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace at25040b::detail {

/**
 * @brief Matches a port hook against exactly one signature, `R(Args...) noexcept`, as a member
 * function of `Port` (`const` or not) (KB-CPP20-006).
 *
 * `ExactHook<Port, Sig>::Take(&Port::Hook)` is well-formed only when `&Port::Hook` names a member
 * function of exactly that type: the address of an overloaded or template hook resolves to the
 * one that matches, and a hook with a narrower or different parameter, another result type or no
 * `noexcept` has no match. No value is converted. Declared only: it appears in
 * requires-expressions, never in evaluated code.
 *
 * @tparam Port The port class.
 * @tparam Sig The hook's function type, `noexcept`; a `Sig` without `noexcept` has no definition.
 */
template <typename Port, typename Sig>
  requires std::is_class_v<Port> && std::is_function_v<Sig>
struct ExactHook;

/// The `noexcept` signatures: the only ones a hook may have.
template <typename Port, typename R, typename... Args>
  requires std::is_class_v<Port>
struct ExactHook<Port, R(Args...) noexcept> {
  /// Takes the address of a non-`const` hook of exactly this signature.
  static void Take(R (Port::*hook)(Args...) noexcept) noexcept;
  /// Takes the address of a `const` hook of exactly this signature.
  static void Take(R (Port::*hook)(Args...) const noexcept) noexcept;
};

/// A class type: what `kAlwaysFalse` depends on, the port or another class of the branch.
template <typename T>
concept ClassType = std::is_class_v<T>;

/// `false` for every class `T`: the condition of a `static_assert` in a branch that must not be
/// instantiated (KB-CPP20-008).
template <ClassType>
inline constexpr bool kAlwaysFalse = false;

/// A scoped or unscoped enumeration: what the enumeration helpers take (KB-CPP20-005, -026).
template <typename E>
concept Enumeration = std::is_enum_v<E>;

/**
 * @brief The value of an enumerator in its underlying type, our stand-in for C++23
 * `std::to_underlying` (KB-CPP20-002, -026).
 */
template <Enumeration E>
[[nodiscard]] constexpr std::underlying_type_t<E> ToUnderlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

/**
 * @brief The last enumerator of a contiguous enumeration that starts at 0; specialised next to each
 * enumeration that `IsEnumerator()` checks (KB-CPP20-026).
 */
template <Enumeration E>
struct EnumLast;

/**
 * @brief Whether `e` holds an enumerator of a contiguous enumeration that starts at 0. A value made
 * by `static_cast` from an integer can be anything its underlying type holds (KB-CPP20-025).
 */
template <Enumeration E>
[[nodiscard]] constexpr bool IsEnumerator(E e) noexcept {
  return static_cast<uint32_t>(ToUnderlying(e)) <=
         static_cast<uint32_t>(ToUnderlying(EnumLast<E>::kValue));
}

/**
 * @brief A bit field of a register: `Width` bits from bit `Shift` of a `Reg`. Every shift is a
 * constant below the width and every operand is widened to `uint32_t` once, by construction
 * (KB-CPP20-020, -021).
 */
template <typename Reg, uint32_t Shift, uint32_t Width>
  requires std::unsigned_integral<Reg> && (Width > 0U) && (Width < 32U) &&
           ((Shift + Width) <= static_cast<uint32_t>(std::numeric_limits<Reg>::digits))
struct Field {
  /// The largest value the field holds.
  static constexpr uint32_t kMax = (uint32_t{1} << Width) - 1U;
  /// The field's bits in the register.
  static constexpr Reg kMask = static_cast<Reg>(kMax << Shift);

  /// `reg` with the field set to `value` (masked to the field's width); the other bits unchanged.
  [[nodiscard]] static constexpr Reg Insert(Reg reg, uint32_t value) noexcept {
    return static_cast<Reg>((static_cast<uint32_t>(reg) & ~static_cast<uint32_t>(kMask)) |
                            ((value & kMax) << Shift));
  }
  /// The field's value in `reg`.
  [[nodiscard]] static constexpr uint32_t Extract(Reg reg) noexcept {
    return (static_cast<uint32_t>(reg) >> Shift) & kMax;
  }
};

/**
 * @name Typed integer literals
 * `0x0C_u8`, `0x1FF_u16`, `3300_u32`: a constant of exactly the parameter's type, so passing it
 * converts nothing (MISRA C++:2023 Rule 7.0.6). `U` alone is `unsigned int`, which is not
 * `uint32_t` on every target. A value the type cannot hold does not compile. Brought in with
 * `using detail::operator""_u8;` where used.
 * @{
 */

/// Not `constexpr`: a literal operator that reaches it is not a constant expression, which turns
/// an out-of-range literal into a compile error.
inline void LiteralOutOfRange() noexcept {}

/// A `uint8_t` constant; a value above 0xFF does not compile.
consteval uint8_t operator""_u8(unsigned long long value) noexcept { // NOLINT(google-runtime-int)
  if (value > static_cast<unsigned long long>(std::numeric_limits<uint8_t>::max())) {
    LiteralOutOfRange();
  }
  return static_cast<uint8_t>(value);
}

/// A `uint16_t` constant; a value above 0xFFFF does not compile.
consteval uint16_t operator""_u16(unsigned long long value) noexcept { // NOLINT(google-runtime-int)
  if (value > static_cast<unsigned long long>(std::numeric_limits<uint16_t>::max())) {
    LiteralOutOfRange();
  }
  return static_cast<uint16_t>(value);
}

/// A `uint32_t` constant; a value above 0xFFFF'FFFF does not compile.
consteval uint32_t operator""_u32(unsigned long long value) noexcept { // NOLINT(google-runtime-int)
  if (value > static_cast<unsigned long long>(std::numeric_limits<uint32_t>::max())) {
    LiteralOutOfRange();
  }
  return static_cast<uint32_t>(value);
}

/** @} */

static_assert(0xFF_u8 == std::numeric_limits<uint8_t>::max() && 0xFFFF_u16 == 0xFFFFU &&
              0xFFFF'FFFF_u32 == std::numeric_limits<uint32_t>::max());

} // namespace at25040b::detail

#endif // AT25040B_DETAIL_HPP
