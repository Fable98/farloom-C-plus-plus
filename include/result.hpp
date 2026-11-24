#pragma once

#include <variant>
#include <utility>
#include <stdexcept>
#include <string>

namespace farloom {

template <typename T, typename E>
class Result {
public:
    Result(T val) : data_(std::in_place_index<0>, std::move(val)) {}
    Result(E err) : data_(std::in_place_index<1>, std::move(err)) {}

    static Result ok(T val) { return Result(std::move(val)); }
    static Result err(E err) { return Result(std::move(err)); }

    bool is_ok() const noexcept { return data_.index() == 0; }
    bool is_err() const noexcept { return data_.index() == 1; }
    explicit operator bool() const noexcept { return is_ok(); }

    T& value() & {
        if (!is_ok()) throw std::runtime_error("Called value() on error Result");
        return std::get<0>(data_);
    }
    const T& value() const & {
        if (!is_ok()) throw std::runtime_error("Called value() on error Result");
        return std::get<0>(data_);
    }
    T&& value() && {
        if (!is_ok()) throw std::runtime_error("Called value() on error Result");
        return std::get<0>(std::move(data_));
    }

    E& error() & {
        if (!is_err()) throw std::runtime_error("Called error() on ok Result");
        return std::get<1>(data_);
    }
    const E& error() const & {
        if (!is_err()) throw std::runtime_error("Called error() on ok Result");
        return std::get<1>(data_);
    }
    E&& error() && {
        if (!is_err()) throw std::runtime_error("Called error() on ok Result");
        return std::get<1>(std::move(data_));
    }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    T& operator*() & { return value(); }
    const T& operator*() const & { return value(); }
    T&& operator*() && { return std::move(value()); }

private:
    std::variant<T, E> data_;
};

template <typename E>
class Result<void, E> {
public:
    Result() : has_err_(false) {}
    Result(E err) : has_err_(true), err_(std::move(err)) {}

    static Result ok() { return Result(); }
    static Result err(E err) { return Result(std::move(err)); }

    bool is_ok() const noexcept { return !has_err_; }
    bool is_err() const noexcept { return has_err_; }
    explicit operator bool() const noexcept { return is_ok(); }

    void value() const {
        if (!is_ok()) throw std::runtime_error("Called value() on error Result");
    }

    E& error() & {
        if (!is_err()) throw std::runtime_error("Called error() on ok Result");
        return err_;
    }
    const E& error() const & {
        if (!is_err()) throw std::runtime_error("Called error() on ok Result");
        return err_;
    }
    E&& error() && {
        if (!is_err()) throw std::runtime_error("Called error() on ok Result");
        return std::move(err_);
    }

private:
    bool has_err_{false};
    E err_{};
};

} // namespace farloom
