module;
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

export module firefly.core.types;

export namespace firefly {
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using f32 = float;
    using f64 = double;

    using String = std::string;
    using StringView = std::string_view;

    template<typename T>
    using Ptr = std::unique_ptr<T>;
    template<typename T>
    using SharedPtr = std::shared_ptr<T>;
    template<typename T>
    using WeakPtr = std::weak_ptr<T>;

    template<typename T, typename E = String>
    class Result {
    public:
        constexpr bool is_ok() const noexcept { return m_value.has_value(); }
        constexpr bool is_error() const noexcept { return !is_ok(); }
        constexpr auto& value() { return *m_value; }
        constexpr const auto& value() const { return *m_value; }
        constexpr auto& error() { return m_error; }
        constexpr const auto& error() const { return m_error; }

        static constexpr Result ok(T value) { return Result{std::move(value), {}}; }
        static constexpr Result error(E error) { return Result{{}, std::move(error)}; }
    private:
        constexpr Result(std::optional<T> val, E err)
            : m_value(std::move(val)), m_error(std::move(err)) {}
        std::optional<T> m_value;
        E m_error;
    };

    template<typename E>
    class Result<void, E> {
    public:
        constexpr bool is_ok() const noexcept { return m_ok; }
        constexpr bool is_error() const noexcept { return !m_ok; }
        constexpr auto& error() { return m_error; }
        constexpr const auto& error() const { return m_error; }

        static constexpr Result ok() { Result r; r.m_ok = true; return r; }
        static constexpr Result error(E err) { Result r; r.m_ok = false; r.m_error = std::move(err); return r; }
    private:
        bool m_ok = false;
        E m_error;
    };

    template<typename F>
    class ScopeExit {
    public:
        explicit ScopeExit(F&& f) : m_func(std::forward<F>(f)) {}
        ~ScopeExit() { m_func(); }
        ScopeExit(const ScopeExit&) = delete;
        ScopeExit& operator=(const ScopeExit&) = delete;
    private:
        F m_func;
    };

} // namespace firefly
