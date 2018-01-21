//
//  peglib.h
//
//  Copyright (c) 2015-17 Yuji Hirose. All rights reserved.
//  MIT License
//

#ifndef CPPPEGLIB_PEGLIB_H
#define CPPPEGLIB_PEGLIB_H

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// guard for older versions of VC++
#ifdef _MSC_VER
// VS2013 has no constexpr
#if (_MSC_VER == 1800)
#define PEGLIB_NO_CONSTEXPR_SUPPORT
#elif (_MSC_VER >= 1800)
// good to go
#else (_MSC_VER < 1800)
#error "Requires C+11 support"
#endif
#endif

// define if the compiler doesn't support unicode characters reliably in the
// source code
//#define PEGLIB_NO_UNICODE_CHARS

namespace peg {

#if __clang__ == 1 && __clang_major__ == 5 && __clang_minor__ == 0 && __clang_patchlevel__ == 0
static void* enabler = nullptr; // workaround for Clang 5.0.0
#else
extern void* enabler;
#endif

/*-----------------------------------------------------------------------------
 *  any
 *---------------------------------------------------------------------------*/

class any
{
public:
    any() : content_(nullptr) {}

    any(const any& rhs) : content_(rhs.clone()) {}

    any(any&& rhs) : content_(rhs.content_) {
        rhs.content_ = nullptr;
    }

    template <typename T>
    any(const T& value) : content_(new holder<T>(value)) {}

    any& operator=(const any& rhs) {
        if (this != &rhs) {
            if (content_) {
                delete content_;
            }
            content_ = rhs.clone();
        }
        return *this;
    }

    any& operator=(any&& rhs) {
        if (this != &rhs) {
            if (content_) {
                delete content_;
            }
            content_ = rhs.content_;
            rhs.content_ = nullptr;
        }
        return *this;
    }

    ~any() {
        delete content_;
    }

    bool is_undefined() const {
        return content_ == nullptr;
    }

    template <
        typename T,
        typename std::enable_if<!std::is_same<T, any>::value>::type*& = enabler
    >
    T& get() {
        if (!content_) {
            throw std::bad_cast();
        }
        auto p = dynamic_cast<holder<T>*>(content_);
        assert(p);
        if (!p) {
            throw std::bad_cast();
        }
        return p->value_;
    }

    template <
        typename T,
        typename std::enable_if<std::is_same<T, any>::value>::type*& = enabler
    >
    T& get() {
        return *this;
    }

    template <
        typename T,
        typename std::enable_if<!std::is_same<T, any>::value>::type*& = enabler
    >
    const T& get() const {
        assert(content_);
        auto p = dynamic_cast<holder<T>*>(content_);
        assert(p);
        if (!p) {
            throw std::bad_cast();
        }
        return p->value_;
    }

    template <
        typename T,
        typename std::enable_if<std::is_same<T, any>::value>::type*& = enabler
    >
    const any& get() const {
        return *this;
    }

private:
    struct placeholder {
        virtual ~placeholder() {}
        virtual placeholder* clone() const = 0;
    };

    template <typename T>
    struct holder : placeholder {
        holder(const T& value) : value_(value) {}
        placeholder* clone() const override {
            return new holder(value_);
        }
        T value_;
    };

    placeholder* clone() const {
        return content_ ? content_->clone() : nullptr;
    }

    placeholder* content_;
};

/*-----------------------------------------------------------------------------
 *  scope_exit
 *---------------------------------------------------------------------------*/

// This is based on "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4189".

template <typename EF>
struct scope_exit
{
    explicit scope_exit(EF&& f)
        : exit_function(std::move(f))
        , execute_on_destruction{true} {}

    scope_exit(scope_exit&& rhs)
        : exit_function(std::move(rhs.exit_function))
        , execute_on_destruction{rhs.execute_on_destruction} {
            rhs.release();
    }

    ~scope_exit() {
        if (execute_on_destruction) {
            this->exit_function();
        }
    }

    void release() {
        this->execute_on_destruction = false;
    }

private:
    scope_exit(const scope_exit&) = delete;
    void operator=(const scope_exit&) = delete;
    scope_exit& operator=(scope_exit&&) = delete;

    EF   exit_function;
    bool execute_on_destruction;
};

template <typename EF>
auto make_scope_exit(EF&& exit_function) -> scope_exit<EF> {
    return scope_exit<typename std::remove_reference<EF>::type>(std::forward<EF>(exit_function));
}

/*-----------------------------------------------------------------------------
 *  PEG
 *---------------------------------------------------------------------------*/

/*
* Line information utility function
*/
inline std::pair<size_t, size_t> line_info(const char* start, const char* cur) {
    auto p = start;
    auto col_ptr = p;
    auto no = 1;

    while (p < cur) {
        if (*p == '\n') {
            no++;
            col_ptr = p + 1;
        }
        p++;
    }

    auto col = p - col_ptr + 1;

    return std::make_pair(no, col);
}

/*
* Semantic values
*/
struct SemanticValues : protected std::vector<any>
{
    // Input text
    const char* path;
    const char* ss;

    // Matched string
    const char* c_str() const { return s_; }
    size_t      length() const { return n_; }

    std::string str() const {
        return std::string(s_, n_);
    }

    // Line number and column at which the matched string is
    std::pair<size_t, size_t> line_info() const {
        return peg::line_info(ss, s_);
    }

    // Choice number (0 based index)
    size_t      choice() const { return choice_; }

    // Tokens
    std::vector<std::pair<const char*, size_t>> tokens;

    std::string token(size_t id = 0) const {
        if (!tokens.empty()) {
            assert(id < tokens.size());
            const auto& tok = tokens[id];
            return std::string(tok.first, tok.second);
        }
        return std::string(s_, n_);
    }

    // Transform the semantic value vector to another vector
    template <typename T>
    auto transform(size_t beg = 0, size_t end = static_cast<size_t>(-1)) const -> vector<T> {
        return this->transform(beg, end, [](const any& v) { return v.get<T>(); });
    }

    SemanticValues() : s_(nullptr), n_(0), choice_(0) {}

    using std::vector<any>::iterator;
    using std::vector<any>::const_iterator;
    using std::vector<any>::size;
    using std::vector<any>::empty;
    using std::vector<any>::assign;
    using std::vector<any>::begin;
    using std::vector<any>::end;
    using std::vector<any>::rbegin;
    using std::vector<any>::rend;
    using std::vector<any>::operator[];
    using std::vector<any>::at;
    using std::vector<any>::resize;
    using std::vector<any>::front;
    using std::vector<any>::back;
    using std::vector<any>::push_back;
    using std::vector<any>::pop_back;
    using std::vector<any>::insert;
    using std::vector<any>::erase;
    using std::vector<any>::clear;
    using std::vector<any>::swap;
    using std::vector<any>::emplace;
    using std::vector<any>::emplace_back;

private:
    friend class Context;
    friend class PrioritizedChoice;
    friend class Holder;

    const char* s_;
    size_t      n_;
    size_t      choice_;

    template <typename F>
    auto transform(F f) const -> vector<typename std::remove_const<decltype(f(any()))>::type> {
        vector<typename std::remove_const<decltype(f(any()))>::type> r;
        for (const auto& v: *this) {
            r.emplace_back(f(v));
        }
        return r;
    }

    template <typename F>
    auto transform(size_t beg, size_t end, F f) const -> vector<typename std::remove_const<decltype(f(any()))>::type> {
        vector<typename std::remove_const<decltype(f(any()))>::type> r;
        end = (std::min)(end, size());
        for (size_t i = beg; i < end; i++) {
            r.emplace_back(f((*this)[i]));
        }
        return r;
    }
};

/*
 * Semantic action
 */
template <
    typename R, typename F,
    typename std::enable_if<std::is_void<R>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    fn(std::forward<Args>(args)...);
    return any();
}

template <
    typename R, typename F,
    typename std::enable_if<std::is_same<typename std::remove_cv<R>::type, any>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return fn(std::forward<Args>(args)...);
}

template <
    typename R, typename F,
    typename std::enable_if<
        !std::is_void<R>::value &&
        !std::is_same<typename std::remove_cv<R>::type, any>::value>::type*& = enabler,
    typename... Args>
any call(F fn, Args&&... args) {
    return any(fn(std::forward<Args>(args)...));
}

class Action
{
public:
    Action() = default;

    Action(const Action& rhs) : fn_(rhs.fn_) {}

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value && !std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    Action(F fn) : fn_(make_adaptor(fn, &F::operator())) {}

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    Action(F fn) : fn_(make_adaptor(fn, fn)) {}

    template <typename F, typename std::enable_if<std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    Action(F /*fn*/) {}

    template <typename F, typename std::enable_if<!std::is_pointer<F>::value && !std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, &F::operator());
    }

    template <typename F, typename std::enable_if<std::is_pointer<F>::value>::type*& = enabler>
    void operator=(F fn) {
        fn_ = make_adaptor(fn, fn);
    }

    template <typename F, typename std::enable_if<std::is_same<F, std::nullptr_t>::value>::type*& = enabler>
    void operator=(F /*fn*/) {}

    Action& operator=(const Action& rhs) = default;

    operator bool() const {
        return bool(fn_);
    }

    any operator()(const SemanticValues& sv, any& dt) const {
        return fn_(sv, dt);
    }

private:
    template <typename R>
    struct TypeAdaptor {
        TypeAdaptor(std::function<R (const SemanticValues& sv)> fn)
            : fn_(fn) {}
        any operator()(const SemanticValues& sv, any& /*dt*/) {
            return call<R>(fn_, sv);
        }
        std::function<R (const SemanticValues& sv)> fn_;
    };

    template <typename R>
    struct TypeAdaptor_c {
        TypeAdaptor_c(std::function<R (const SemanticValues& sv, any& dt)> fn)
            : fn_(fn) {}
        any operator()(const SemanticValues& sv, any& dt) {
            return call<R>(fn_, sv, dt);
        }
        std::function<R (const SemanticValues& sv, any& dt)> fn_;
    };

    typedef std::function<any (const SemanticValues& sv, any& dt)> Fty;

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::* /*mf*/)(const SemanticValues& sv) const) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::* /*mf*/)(const SemanticValues& sv)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (* /*mf*/)(const SemanticValues& sv)) {
        return TypeAdaptor<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::* /*mf*/)(const SemanticValues& sv, any& dt) const) {
        return TypeAdaptor_c<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R (F::* /*mf*/)(const SemanticValues& sv, any& dt)) {
        return TypeAdaptor_c<R>(fn);
    }

    template<typename F, typename R>
    Fty make_adaptor(F fn, R(* /*mf*/)(const SemanticValues& sv, any& dt)) {
        return TypeAdaptor_c<R>(fn);
    }

    Fty fn_;
};

/*
 * Semantic predicate
 */
// Note: 'parse_error' exception class should be be used in sematic action handlers to reject the rule.
struct parse_error {
    parse_error() = default;
    parse_error(const char* s) : s_(s) {}
    const char* what() const { return s_.empty() ? nullptr : s_.c_str(); }
private:
    std::string s_;
};

/*
 * Match action
 */
typedef std::function<void (const char* s, size_t n, size_t id, const std::string& name)> MatchAction;

/*
 * Result
 */
inline bool success(size_t len) {
    return len != static_cast<size_t>(-1);
}

inline bool fail(size_t len) {
    return len == static_cast<size_t>(-1);
}

/*
 * Context
 */
class Ope;
class Context;
class Definition;

typedef std::function<void (const char* name, const char* s, size_t n, const SemanticValues& sv, const Context& c, const any& dt)> Tracer;

class Context
{
public:
    const char*                                  path;
    const char*                                  s;
    const size_t                                 l;

    const char*                                  error_pos;
    const char*                                  message_pos;
    std::string                                  message; // TODO: should be `int`.

    std::vector<std::shared_ptr<SemanticValues>> value_stack;
    size_t                                       value_stack_size;

    size_t                                       nest_level;

    bool                                         in_token;

    std::shared_ptr<Ope>                         whitespaceOpe;
    bool                                         in_whitespace;

    const size_t                                 def_count;
    const bool                                   enablePackratParsing;
    std::vector<bool>                            cache_registered;
    std::vector<bool>                            cache_success;

    std::map<std::pair<size_t, size_t>, std::tuple<size_t, any>> cache_values;

    std::function<void (const char*, const char*, size_t, const SemanticValues&, const Context&, const any&)> tracer;

    Context(
        const char*          a_path,
        const char*          a_s,
        size_t               a_l,
        size_t               a_def_count,
        std::shared_ptr<Ope> a_whitespaceOpe,
        bool                 a_enablePackratParsing,
        Tracer               a_tracer)
        : path(a_path)
        , s(a_s)
        , l(a_l)
        , error_pos(nullptr)
        , message_pos(nullptr)
        , value_stack_size(0)
        , nest_level(0)
        , in_token(false)
        , whitespaceOpe(a_whitespaceOpe)
        , in_whitespace(false)
        , def_count(a_def_count)
        , enablePackratParsing(a_enablePackratParsing)
        , cache_registered(enablePackratParsing ? def_count * (l + 1) : 0)
        , cache_success(enablePackratParsing ? def_count * (l + 1) : 0)
        , tracer(a_tracer)
    {
    }

    template <typename T>
    void packrat(const char* a_s, size_t def_id, size_t& len, any& val, T fn) {
        if (!enablePackratParsing) {
            fn(val);
            return;
        }

        auto col = a_s - s;
        auto idx = def_count * static_cast<size_t>(col) + def_id;

        if (cache_registered[idx]) {
            if (cache_success[idx]) {
                auto key = std::make_pair(col, def_id);
                std::tie(len, val) = cache_values[key];
                return;
            } else {
                len = static_cast<size_t>(-1);
                return;
            }
        } else {
            fn(val);
            cache_registered[idx] = true;
            cache_success[idx] = success(len);
            if (success(len)) {
                auto key = std::make_pair(col, def_id);
                cache_values[key] = std::make_pair(len, val);
            }
            return;
        }
    }

    SemanticValues& push() {
        assert(value_stack_size <= value_stack.size());
        if (value_stack_size == value_stack.size()) {
            value_stack.emplace_back(std::make_shared<SemanticValues>());
        }
        auto& sv = *value_stack[value_stack_size++];
        if (!sv.empty()) {
            sv.clear();
        }
        sv.path = path;
        sv.ss = s;
        sv.s_ = nullptr;
        sv.n_ = 0;
        sv.tokens.clear();
        return sv;
    }

    void pop() {
        value_stack_size--;
    }

    void set_error_pos(const char* a_s) {
        if (error_pos < a_s) error_pos = a_s;
    }

    void trace(const char* name, const char* a_s, size_t n, SemanticValues& sv, any& dt) const {
        if (tracer) tracer(name, a_s, n, sv, *this, dt);
    }
};

/*
 * Parser operators
 */
class Ope
{
public:
    struct Visitor;

    virtual ~Ope() {}
    virtual size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const = 0;
    virtual void accept(Visitor& v) = 0;
};

class Sequence : public Ope
{
public:
    Sequence(const Sequence& rhs) : opes_(rhs.opes_) {}

#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
    // NOTE: Compiler Error C2797 on Visual Studio 2013
    // "The C++ compiler in Visual Studio does not implement list
    // initialization inside either a member initializer list or a non-static
    // data member initializer. Before Visual Studio 2013 Update 3, this was
    // silently converted to a function call, which could lead to bad code
    // generation. Visual Studio 2013 Update 3 reports this as an error."
    template <typename... Args>
    Sequence(const Args& ...args) {
        opes_ = std::vector<std::shared_ptr<Ope>>{ static_cast<std::shared_ptr<Ope>>(args)... };
    }
#else
    template <typename... Args>
    Sequence(const Args& ...args) : opes_{ static_cast<std::shared_ptr<Ope>>(args)... } {}
#endif

    Sequence(const std::vector<std::shared_ptr<Ope>>& opes) : opes_(opes) {}
    Sequence(std::vector<std::shared_ptr<Ope>>&& opes) : opes_(opes) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("Sequence", s, n, sv, dt);
        size_t i = 0;
        for (const auto& ope : opes_) {
            c.nest_level++;
            auto se = make_scope_exit([&]() { c.nest_level--; });
            const auto& rule = *ope;
            auto len = rule.parse(s + i, n - i, sv, c, dt);
            if (fail(len)) {
                return static_cast<size_t>(-1);
            }
            i += len;
        }
        return i;
    }

    void accept(Visitor& v) override;

    std::vector<std::shared_ptr<Ope>> opes_;
};

class PrioritizedChoice : public Ope
{
public:
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
    // NOTE: Compiler Error C2797 on Visual Studio 2013
    // "The C++ compiler in Visual Studio does not implement list
    // initialization inside either a member initializer list or a non-static
    // data member initializer. Before Visual Studio 2013 Update 3, this was
    // silently converted to a function call, which could lead to bad code
    // generation. Visual Studio 2013 Update 3 reports this as an error."
    template <typename... Args>
    PrioritizedChoice(const Args& ...args) {
        opes_ = std::vector<std::shared_ptr<Ope>>{ static_cast<std::shared_ptr<Ope>>(args)... };
    }
#else
    template <typename... Args>
    PrioritizedChoice(const Args& ...args) : opes_{ static_cast<std::shared_ptr<Ope>>(args)... } {}
#endif

    PrioritizedChoice(const std::vector<std::shared_ptr<Ope>>& opes) : opes_(opes) {}
    PrioritizedChoice(std::vector<std::shared_ptr<Ope>>&& opes) : opes_(opes) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("PrioritizedChoice", s, n, sv, dt);
        size_t id = 0;
        for (const auto& ope : opes_) {
            c.nest_level++;
            auto& chldsv = c.push();
            auto se = make_scope_exit([&]() {
                c.nest_level--;
                c.pop();
            });
            const auto& rule = *ope;
            auto len = rule.parse(s, n, chldsv, c, dt);
            if (success(len)) {
                if (!chldsv.empty()) {
                    sv.insert(sv.end(), chldsv.begin(), chldsv.end());
                }
                sv.s_ = chldsv.c_str();
                sv.n_ = chldsv.length();
                sv.choice_ = id;
                sv.tokens.insert(sv.tokens.end(), chldsv.tokens.begin(), chldsv.tokens.end());
                return len;
            }
            id++;
        }
        return static_cast<size_t>(-1);
    }

    void accept(Visitor& v) override;

    size_t size() const { return opes_.size();  }

    std::vector<std::shared_ptr<Ope>> opes_;
};

class ZeroOrMore : public Ope
{
public:
    ZeroOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("ZeroOrMore", s, n, sv, dt);
        auto save_error_pos = c.error_pos;
        size_t i = 0;
        while (n - i > 0) {
            c.nest_level++;
            auto se = make_scope_exit([&]() { c.nest_level--; });
            auto save_sv_size = sv.size();
            auto save_tok_size = sv.tokens.size();
            const auto& rule = *ope_;
            auto len = rule.parse(s + i, n - i, sv, c, dt);
            if (fail(len)) {
                if (sv.size() != save_sv_size) {
                    sv.erase(sv.begin() + static_cast<std::ptrdiff_t>(save_sv_size));
                }
                if (sv.tokens.size() != save_tok_size) {
                    sv.tokens.erase(sv.tokens.begin() + static_cast<std::ptrdiff_t>(save_tok_size));
                }
                c.error_pos = save_error_pos;
                break;
            }
            i += len;
        }
        return i;
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

class OneOrMore : public Ope
{
public:
    OneOrMore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("OneOrMore", s, n, sv, dt);
        size_t len = 0;
        {
            c.nest_level++;
            auto se = make_scope_exit([&]() { c.nest_level--; });
            const auto& rule = *ope_;
            len = rule.parse(s, n, sv, c, dt);
            if (fail(len)) {
                return static_cast<size_t>(-1);
            }
        }
        auto save_error_pos = c.error_pos;
        auto i = len;
        while (n - i > 0) {
            c.nest_level++;
            auto se = make_scope_exit([&]() { c.nest_level--; });
            auto save_sv_size = sv.size();
            auto save_tok_size = sv.tokens.size();
            const auto& rule = *ope_;
            len = rule.parse(s + i, n - i, sv, c, dt);
            if (fail(len)) {
                if (sv.size() != save_sv_size) {
                    sv.erase(sv.begin() + static_cast<std::ptrdiff_t>(save_sv_size));
                }
                if (sv.tokens.size() != save_tok_size) {
                    sv.tokens.erase(sv.tokens.begin() + static_cast<std::ptrdiff_t>(save_tok_size));
                }
                c.error_pos = save_error_pos;
                break;
            }
            i += len;
        }
        return i;
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

class Option : public Ope
{
public:
    Option(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("Option", s, n, sv, dt);
        auto save_error_pos = c.error_pos;
        c.nest_level++;
        auto save_sv_size = sv.size();
        auto save_tok_size = sv.tokens.size();
        auto se = make_scope_exit([&]() { c.nest_level--; });
        const auto& rule = *ope_;
        auto len = rule.parse(s, n, sv, c, dt);
        if (success(len)) {
            return len;
        } else {
            if (sv.size() != save_sv_size) {
                sv.erase(sv.begin() + static_cast<std::ptrdiff_t>(save_sv_size));
            }
            if (sv.tokens.size() != save_tok_size) {
                sv.tokens.erase(sv.tokens.begin() + static_cast<std::ptrdiff_t>(save_tok_size));
            }
            c.error_pos = save_error_pos;
            return 0;
        }
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

class AndPredicate : public Ope
{
public:
    AndPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("AndPredicate", s, n, sv, dt);
        c.nest_level++;
        auto& chldsv = c.push();
        auto se = make_scope_exit([&]() {
            c.nest_level--;
            c.pop();
        });
        const auto& rule = *ope_;
        auto len = rule.parse(s, n, chldsv, c, dt);
        if (success(len)) {
            return 0;
        } else {
            return static_cast<size_t>(-1);
        }
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

class NotPredicate : public Ope
{
public:
    NotPredicate(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("NotPredicate", s, n, sv, dt);
        auto save_error_pos = c.error_pos;
        c.nest_level++;
        auto& chldsv = c.push();
        auto se = make_scope_exit([&]() {
            c.nest_level--;
            c.pop();
        });
        const auto& rule = *ope_;
        auto len = rule.parse(s, n, chldsv, c, dt);
        if (success(len)) {
            c.set_error_pos(s);
            return static_cast<size_t>(-1);
        } else {
            c.error_pos = save_error_pos;
            return 0;
        }
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

class LiteralString : public Ope
{
public:
    LiteralString(const std::string& s) : lit_(s) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override;

    void accept(Visitor& v) override;

    std::string lit_;
};

class CharacterClass : public Ope
{
public:
    CharacterClass(const std::string& chars) : chars_(chars) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("CharacterClass", s, n, sv, dt);
        // TODO: UTF8 support
        if (n < 1) {
            c.set_error_pos(s);
            return static_cast<size_t>(-1);
        }
        auto ch = s[0];
        auto i = 0u;
        while (i < chars_.size()) {
            if (i + 2 < chars_.size() && chars_[i + 1] == '-') {
                if (chars_[i] <= ch && ch <= chars_[i + 2]) {
                    return 1;
                }
                i += 3;
            } else {
                if (chars_[i] == ch) {
                    return 1;
                }
                i += 1;
            }
        }
        c.set_error_pos(s);
        return static_cast<size_t>(-1);
    }

    void accept(Visitor& v) override;

    std::string chars_;
};

class Character : public Ope
{
public:
    Character(char ch) : ch_(ch) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("Character", s, n, sv, dt);
        // TODO: UTF8 support
        if (n < 1 || s[0] != ch_) {
            c.set_error_pos(s);
            return static_cast<size_t>(-1);
        }
        return 1;
    }

    void accept(Visitor& v) override;

    char ch_;
};

class AnyCharacter : public Ope
{
public:
    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        c.trace("AnyCharacter", s, n, sv, dt);
        // TODO: UTF8 support
        if (n < 1) {
            c.set_error_pos(s);
            return static_cast<size_t>(-1);
        }
        return 1;
    }

    void accept(Visitor& v) override;
};

class Capture : public Ope
{
public:
    Capture(const std::shared_ptr<Ope>& ope, MatchAction ma, size_t id, const std::string& name)
        : ope_(ope), match_action_(ma), id_(id), name_(name) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        const auto& rule = *ope_;
        auto len = rule.parse(s, n, sv, c, dt);
        if (success(len) && match_action_) {
            match_action_(s, len, id_, name_);
        }
        return len;
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;

private:
    MatchAction          match_action_;
    size_t               id_;
    std::string          name_;
};

class TokenBoundary : public Ope
{
public:
    TokenBoundary(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override;

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

class Ignore : public Ope
{
public:
    Ignore(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& /*sv*/, Context& c, any& dt) const override {
        const auto& rule = *ope_;
        auto& chldsv = c.push();
        auto se = make_scope_exit([&]() {
            c.pop();
        });
        return rule.parse(s, n, chldsv, c, dt);
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

typedef std::function<size_t (const char* s, size_t n, SemanticValues& sv, any& dt)> Parser;

class WeakHolder : public Ope
{
public:
    WeakHolder(const std::shared_ptr<Ope>& ope) : weak_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        auto ope = weak_.lock();
        assert(ope);
        const auto& rule = *ope;
        return rule.parse(s, n, sv, c, dt);
    }

    void accept(Visitor& v) override;

    std::weak_ptr<Ope> weak_;
};

class Holder : public Ope
{
public:
    Holder(Definition* outer)
       : outer_(outer) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override;

    void accept(Visitor& v) override;

    any reduce(const SemanticValues& sv, any& dt) const;

    std::shared_ptr<Ope> ope_;
    Definition*          outer_;

    friend class Definition;
};

class DefinitionReference : public Ope
{
public:
    DefinitionReference(
        const std::unordered_map<std::string, Definition>& grammar, const std::string& name, const char* s)
        : grammar_(grammar)
        , name_(name)
        , s_(s) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override;

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> get_rule() const;

    const std::unordered_map<std::string, Definition>& grammar_;
    const std::string                                  name_;
    const char*                                        s_;

private:
    mutable std::once_flag                             init_;
    mutable std::shared_ptr<Ope>                       rule_;
};

class Whitespace : public Ope
{
public:
    Whitespace(const std::shared_ptr<Ope>& ope) : ope_(ope) {}

    size_t parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const override {
        if (c.in_whitespace) {
            return 0;
        }
        c.in_whitespace = true;
        auto se = make_scope_exit([&]() { c.in_whitespace = false; });
        const auto& rule = *ope_;
        return rule.parse(s, n, sv, c, dt);
    }

    void accept(Visitor& v) override;

    std::shared_ptr<Ope> ope_;
};

/*
 * Visitor
 */
struct Ope::Visitor
{
    virtual ~Visitor() {}
    virtual void visit(Sequence& /*ope*/) {}
    virtual void visit(PrioritizedChoice& /*ope*/) {}
    virtual void visit(ZeroOrMore& /*ope*/) {}
    virtual void visit(OneOrMore& /*ope*/) {}
    virtual void visit(Option& /*ope*/) {}
    virtual void visit(AndPredicate& /*ope*/) {}
    virtual void visit(NotPredicate& /*ope*/) {}
    virtual void visit(LiteralString& /*ope*/) {}
    virtual void visit(CharacterClass& /*ope*/) {}
    virtual void visit(Character& /*ope*/) {}
    virtual void visit(AnyCharacter& /*ope*/) {}
    virtual void visit(Capture& /*ope*/) {}
    virtual void visit(TokenBoundary& /*ope*/) {}
    virtual void visit(Ignore& /*ope*/) {}
    virtual void visit(WeakHolder& /*ope*/) {}
    virtual void visit(Holder& /*ope*/) {}
    virtual void visit(DefinitionReference& /*ope*/) {}
    virtual void visit(Whitespace& /*ope*/) {}
};

struct AssignIDToDefinition : public Ope::Visitor
{
    using Ope::Visitor::visit;

    void visit(Sequence& ope) override {
        for (auto op: ope.opes_) {
            op->accept(*this);
        }
    }
    void visit(PrioritizedChoice& ope) override {
        for (auto op: ope.opes_) {
            op->accept(*this);
        }
    }
    void visit(ZeroOrMore& ope) override { ope.ope_->accept(*this); }
    void visit(OneOrMore& ope) override { ope.ope_->accept(*this); }
    void visit(Option& ope) override { ope.ope_->accept(*this); }
    void visit(AndPredicate& ope) override { ope.ope_->accept(*this); }
    void visit(NotPredicate& ope) override { ope.ope_->accept(*this); }
    void visit(Capture& ope) override { ope.ope_->accept(*this); }
    void visit(TokenBoundary& ope) override { ope.ope_->accept(*this); }
    void visit(Ignore& ope) override { ope.ope_->accept(*this); }
    void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
    void visit(Holder& ope) override;
    void visit(DefinitionReference& ope) override { ope.get_rule()->accept(*this); }

    std::unordered_map<void*, size_t> ids;
};

struct IsToken : public Ope::Visitor
{
    IsToken() : has_token_boundary(false), has_rule(false) {}

    using Ope::Visitor::visit;

    void visit(Sequence& ope) override {
        for (auto op: ope.opes_) {
            op->accept(*this);
        }
    }
    void visit(PrioritizedChoice& ope) override {
        for (auto op: ope.opes_) {
            op->accept(*this);
        }
    }
    void visit(ZeroOrMore& ope) override { ope.ope_->accept(*this); }
    void visit(OneOrMore& ope) override { ope.ope_->accept(*this); }
    void visit(Option& ope) override { ope.ope_->accept(*this); }
    void visit(Capture& ope) override { ope.ope_->accept(*this); }
    void visit(TokenBoundary& /*ope*/) override { has_token_boundary = true; }
    void visit(Ignore& ope) override { ope.ope_->accept(*this); }
    void visit(WeakHolder& ope) override { ope.weak_.lock()->accept(*this); }
    void visit(DefinitionReference& /*ope*/) override { has_rule = true; }

    bool is_token() const {
        return has_token_boundary || !has_rule;
    }

    bool has_token_boundary;
    bool has_rule;
};

static const char* WHITESPACE_DEFINITION_NAME = "%whitespace";

/*
 * Definition
 */
class Definition
{
public:
    struct Result {
        bool              ret;
        size_t            len;
        const char*       error_pos;
        const char*       message_pos;
        const std::string message;
    };

    Definition()
        : ignoreSemanticValue(false)
        , enablePackratParsing(false)
        , is_token(false)
        , has_token_boundary(false)
        , holder_(std::make_shared<Holder>(this)) {}

    Definition(const Definition& rhs)
        : name(rhs.name)
        , ignoreSemanticValue(false)
        , enablePackratParsing(false)
        , is_token(false)
        , has_token_boundary(false)
        , holder_(rhs.holder_)
    {
        holder_->outer_ = this;
    }

    Definition(Definition&& rhs)
        : name(std::move(rhs.name))
        , ignoreSemanticValue(rhs.ignoreSemanticValue)
        , whitespaceOpe(rhs.whitespaceOpe)
        , enablePackratParsing(rhs.enablePackratParsing)
        , is_token(rhs.is_token)
        , has_token_boundary(rhs.has_token_boundary)
        , holder_(std::move(rhs.holder_))
    {
        holder_->outer_ = this;
    }

    Definition(const std::shared_ptr<Ope>& ope)
        : ignoreSemanticValue(false)
        , enablePackratParsing(false)
        , is_token(false)
        , has_token_boundary(false)
        , holder_(std::make_shared<Holder>(this))
    {
        *this <= ope;
    }

    operator std::shared_ptr<Ope>() {
        return std::make_shared<WeakHolder>(holder_);
    }

    Definition& operator<=(const std::shared_ptr<Ope>& ope) {
        IsToken isToken;
        ope->accept(isToken);
        is_token = isToken.is_token();
        has_token_boundary = isToken.has_token_boundary;

        holder_->ope_ = ope;

        return *this;
    }

    Result parse(const char* s, size_t n, const char* path = nullptr) const {
        SemanticValues sv;
        any dt;
        return parse_core(s, n, sv, dt, path);
    }

    Result parse(const char* s, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse(s, n, path);
    }

    Result parse(const char* s, size_t n, any& dt, const char* path = nullptr) const {
        SemanticValues sv;
        return parse_core(s, n, sv, dt, path);
    }

    Result parse(const char* s, any& dt, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse(s, n, dt, path);
    }

    template <typename T>
    Result parse_and_get_value(const char* s, size_t n, T& val, const char* path = nullptr) const {
        SemanticValues sv;
        any dt;
        auto r = parse_core(s, n, sv, dt, path);
        if (r.ret && !sv.empty() && !sv.front().is_undefined()) {
            val = sv[0].get<T>();
        }
        return r;
    }

    template <typename T>
    Result parse_and_get_value(const char* s, T& val, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse_and_get_value(s, n, val, path);
    }

    template <typename T>
    Result parse_and_get_value(const char* s, size_t n, any& dt, T& val, const char* path = nullptr) const {
        SemanticValues sv;
        auto r = parse_core(s, n, sv, dt, path);
        if (r.ret && !sv.empty() && !sv.front().is_undefined()) {
            val = sv[0].get<T>();
        }
        return r;
    }

    template <typename T>
    Result parse_and_get_value(const char* s, any& dt, T& val, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse_and_get_value(s, n, dt, val, path);
    }

    Definition& operator=(Action a) {
        action = a;
        return *this;
    }

    template <typename T>
    Definition& operator,(T fn) {
        operator=(fn);
        return *this;
    }

    Definition& operator~() {
        ignoreSemanticValue = true;
        return *this;
    }

    void accept(Ope::Visitor& v) {
        holder_->accept(v);
    }

    std::shared_ptr<Ope> get_core_operator() {
        return holder_->ope_;
    }

    std::string                    name;
    size_t                         id;
    Action                         action;
    std::function<void (any& dt)>  enter;
    std::function<void (any& dt)>  leave;
    std::function<std::string ()>  error_message;
    bool                           ignoreSemanticValue;
    std::shared_ptr<Ope>           whitespaceOpe;
    bool                           enablePackratParsing;
    bool                           is_token;
    bool                           has_token_boundary;
    Tracer                         tracer;

private:
    friend class DefinitionReference;

    Definition& operator=(const Definition& rhs);
    Definition& operator=(Definition&& rhs);

    Result parse_core(const char* s, size_t n, SemanticValues& sv, any& dt, const char* path) const {
        AssignIDToDefinition assignId;
        holder_->accept(assignId);

        std::shared_ptr<Ope> ope = holder_;
        if (whitespaceOpe) {
            ope = std::make_shared<Sequence>(whitespaceOpe, ope);
        }

        Context cxt(path, s, n, assignId.ids.size(), whitespaceOpe, enablePackratParsing, tracer);
        auto len = ope->parse(s, n, sv, cxt, dt);
        return Result{ success(len), len, cxt.error_pos, cxt.message_pos, cxt.message };
    }

    std::shared_ptr<Holder> holder_;
};

/*
 * Implementations
 */

inline size_t LiteralString::parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const {
    c.trace("LiteralString", s, n, sv, dt);

    size_t i = 0;
    for (; i < lit_.size(); i++) {
        if (i >= n || s[i] != lit_[i]) {
            c.set_error_pos(s);
            return static_cast<size_t>(-1);
        }
    }

    // Skip whiltespace
    if (!c.in_token) {
        if (c.whitespaceOpe) {
            auto len = c.whitespaceOpe->parse(s + i, n - i, sv, c, dt);
            if (fail(len)) {
                return static_cast<size_t>(-1);
            }
            i += len;
        }
    }

    return i;
}

inline size_t TokenBoundary::parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const {
	c.in_token = true;
    auto se = make_scope_exit([&]() { c.in_token = false; });
    const auto& rule = *ope_;
    auto len = rule.parse(s, n, sv, c, dt);
    if (success(len)) {
        sv.tokens.push_back(std::make_pair(s, len));

        if (c.whitespaceOpe) {
            auto l = c.whitespaceOpe->parse(s + len, n - len, sv, c, dt);
            if (fail(l)) {
                return static_cast<size_t>(-1);
            }
            len += l;
        }
    }
    return len;
}

inline size_t Holder::parse(const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const {
    if (!ope_) {
        throw std::logic_error("Uninitialized definition ope was used...");
    }

    c.trace(outer_->name.c_str(), s, n, sv, dt);
    c.nest_level++;
    auto se = make_scope_exit([&]() { c.nest_level--; });

    size_t      len;
    any         val;

    c.packrat(s, outer_->id, len, val, [&](any& a_val) {
        auto& chldsv = c.push();

        if (outer_->enter) {
            outer_->enter(dt);
        }

        auto se2 = make_scope_exit([&]() {
            c.pop();

            if (outer_->leave) {
                outer_->leave(dt);
            }
        });

        const auto& rule = *ope_;
        len = rule.parse(s, n, chldsv, c, dt);

        // Invoke action
        if (success(len)) {
            chldsv.s_ = s;
            chldsv.n_ = len;

            try {
                a_val = reduce(chldsv, dt);
            } catch (const parse_error& e) {
                if (e.what()) {
                    if (c.message_pos < s) {
                        c.message_pos = s;
                        c.message = e.what();
                    }
                }
                len = static_cast<size_t>(-1);
            }
        }
    });

    if (success(len)) {
        if (!outer_->ignoreSemanticValue) {
            sv.emplace_back(val);
        }
    } else {
        if (outer_->error_message) {
            if (c.message_pos < s) {
                c.message_pos = s;
                c.message = outer_->error_message();
            }
        }
    }

    return len;
}

inline any Holder::reduce(const SemanticValues& sv, any& dt) const {
    if (outer_->action) {
        return outer_->action(sv, dt);
    } else if (sv.empty()) {
        return any();
    } else {
        return sv.front();
    }
}

inline size_t DefinitionReference::parse(
    const char* s, size_t n, SemanticValues& sv, Context& c, any& dt) const {
    const auto& rule = *get_rule();
    return rule.parse(s, n, sv, c, dt);
}

inline std::shared_ptr<Ope> DefinitionReference::get_rule() const {
    if (!rule_) {
        std::call_once(init_, [this]() {
            rule_ = grammar_.at(name_).holder_;
        });
    }
    assert(rule_);
    return rule_;
}

inline void Sequence::accept(Visitor& v) { v.visit(*this); }
inline void PrioritizedChoice::accept(Visitor& v) { v.visit(*this); }
inline void ZeroOrMore::accept(Visitor& v) { v.visit(*this); }
inline void OneOrMore::accept(Visitor& v) { v.visit(*this); }
inline void Option::accept(Visitor& v) { v.visit(*this); }
inline void AndPredicate::accept(Visitor& v) { v.visit(*this); }
inline void NotPredicate::accept(Visitor& v) { v.visit(*this); }
inline void LiteralString::accept(Visitor& v) { v.visit(*this); }
inline void CharacterClass::accept(Visitor& v) { v.visit(*this); }
inline void Character::accept(Visitor& v) { v.visit(*this); }
inline void AnyCharacter::accept(Visitor& v) { v.visit(*this); }
inline void Capture::accept(Visitor& v) { v.visit(*this); }
inline void TokenBoundary::accept(Visitor& v) { v.visit(*this); }
inline void Ignore::accept(Visitor& v) { v.visit(*this); }
inline void WeakHolder::accept(Visitor& v) { v.visit(*this); }
inline void Holder::accept(Visitor& v) { v.visit(*this); }
inline void DefinitionReference::accept(Visitor& v) { v.visit(*this); }
inline void Whitespace::accept(Visitor& v) { v.visit(*this); }

inline void AssignIDToDefinition::visit(Holder& ope) {
    auto p = static_cast<void*>(ope.outer_);
    if (ids.count(p)) {
        return;
    }
    auto id = ids.size();
    ids[p] = id;
    ope.outer_->id = id;
    ope.ope_->accept(*this);
}

/*
 * Factories
 */
template <typename... Args>
std::shared_ptr<Ope> seq(Args&& ...args) {
    return std::make_shared<Sequence>(static_cast<std::shared_ptr<Ope>>(args)...);
}

template <typename... Args>
std::shared_ptr<Ope> cho(Args&& ...args) {
    return std::make_shared<PrioritizedChoice>(static_cast<std::shared_ptr<Ope>>(args)...);
}

inline std::shared_ptr<Ope> zom(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<ZeroOrMore>(ope);
}

inline std::shared_ptr<Ope> oom(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<OneOrMore>(ope);
}

inline std::shared_ptr<Ope> opt(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Option>(ope);
}

inline std::shared_ptr<Ope> apd(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<AndPredicate>(ope);
}

inline std::shared_ptr<Ope> npd(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<NotPredicate>(ope);
}

inline std::shared_ptr<Ope> lit(const std::string& lit) {
    return std::make_shared<LiteralString>(lit);
}

inline std::shared_ptr<Ope> cls(const std::string& chars) {
    return std::make_shared<CharacterClass>(chars);
}

inline std::shared_ptr<Ope> chr(char dt) {
    return std::make_shared<Character>(dt);
}

inline std::shared_ptr<Ope> dot() {
    return std::make_shared<AnyCharacter>();
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope>& ope, MatchAction ma, size_t n, const std::string& s) {
    return std::make_shared<Capture>(ope, ma, n, s);
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope>& ope, MatchAction ma) {
    return std::make_shared<Capture>(ope, ma, static_cast<size_t>(-1), std::string());
}

inline std::shared_ptr<Ope> tok(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<TokenBoundary>(ope);
}

inline std::shared_ptr<Ope> ign(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Ignore>(ope);
}

inline std::shared_ptr<Ope> ref(const std::unordered_map<std::string, Definition>& grammar, const std::string& name, const char* s) {
    return std::make_shared<DefinitionReference>(grammar, name, s);
}

inline std::shared_ptr<Ope> wsp(const std::shared_ptr<Ope>& ope) {
    return std::make_shared<Whitespace>(std::make_shared<Ignore>(ope));
}

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

typedef std::unordered_map<std::string, Definition> Grammar;
typedef std::function<void (size_t, size_t, const std::string&)> Log;

class ParserGenerator
{
public:
    static std::shared_ptr<Grammar> parse(
        const char*  s,
        size_t       n,
        std::string& start,
        MatchAction  ma,
        Log          log)
    {
        return get_instance().perform_core(s, n, start, ma, log);
    }

    // For debuging purpose
    static Grammar& grammar() {
        return get_instance().g;
    }

private:
    static ParserGenerator& get_instance() {
        static ParserGenerator instance;
        return instance;
    }

    ParserGenerator() {
        make_grammar();
        setup_actions();
    }

    struct Data {
        std::shared_ptr<Grammar>                         grammar;
        std::string                                      start;
        MatchAction                                      match_action;
        std::vector<std::pair<std::string, const char*>> duplicates;
        std::unordered_map<std::string, const char*>     references;
        size_t                                           capture_count;

        Data()
            : grammar(std::make_shared<Grammar>())
            , capture_count(0)
            {}
    };

    struct DetectLeftRecursion : public Ope::Visitor {
        DetectLeftRecursion(const std::string& name)
            : s_(nullptr), name_(name), done_(false) {}

        using Ope::Visitor::visit;

        void visit(Sequence& ope) override {
            for (auto op: ope.opes_) {
                op->accept(*this);
                if (done_) {
                    break;
                } else if (s_) {
                    done_ = true;
                    break;
                }
            }
        }
        void visit(PrioritizedChoice& ope) override {
            for (auto op: ope.opes_) {
                op->accept(*this);
                if (s_) {
                    done_ = true;
                    break;
                }
            }
        }
        void visit(ZeroOrMore& ope) override {
            ope.ope_->accept(*this);
            done_ = false;
        }
        void visit(OneOrMore& ope) override {
            ope.ope_->accept(*this);
            done_ = true;
        }
        void visit(Option& ope) override {
            ope.ope_->accept(*this);
            done_ = false;
        }
        void visit(AndPredicate& ope) override {
            ope.ope_->accept(*this);
            done_ = false;
        }
        void visit(NotPredicate& ope) override {
            ope.ope_->accept(*this);
            done_ = false;
        }
        void visit(LiteralString& ope) override {
            done_ = !ope.lit_.empty();
        }
        void visit(CharacterClass& /*ope*/) override {
            done_ = true;
        }
        void visit(Character& /*ope*/) override {
            done_ = true;
        }
        void visit(AnyCharacter& /*ope*/) override {
            done_ = true;
        }
        void visit(Capture& ope) override {
            ope.ope_->accept(*this);
        }
        void visit(TokenBoundary& ope) override {
            ope.ope_->accept(*this);
        }
        void visit(Ignore& ope) override {
            ope.ope_->accept(*this);
        }
        void visit(WeakHolder& ope) override {
            ope.weak_.lock()->accept(*this);
        }
        void visit(Holder& ope) override {
            ope.ope_->accept(*this);
        }
        void visit(DefinitionReference& ope) override {
            if (ope.name_ == name_) {
                s_ = ope.s_;
            } else if (refs_.count(ope.name_)) {
                ;
            } else {
                refs_.insert(ope.name_);
                ope.get_rule()->accept(*this);
            }
            done_ = true;
        }

        const char* s_;

    private:
        std::string           name_;
        std::set<std::string> refs_;
        bool                  done_;
    };

    void make_grammar() {
        // Setup PEG syntax parser
        g["Grammar"]    <= seq(g["Spacing"], oom(g["Definition"]), g["EndOfFile"]);
        g["Definition"] <= seq(opt(g["IGNORE"]), g["Identifier"], g["LEFTARROW"], g["Expression"]);

        g["Expression"] <= seq(g["Sequence"], zom(seq(g["SLASH"], g["Sequence"])));
        g["Sequence"]   <= zom(g["Prefix"]);
        g["Prefix"]     <= seq(opt(cho(g["AND"], g["NOT"])), g["Suffix"]);
        g["Suffix"]     <= seq(g["Primary"], opt(cho(g["QUESTION"], g["STAR"], g["PLUS"])));
        g["Primary"]    <= cho(seq(opt(g["IGNORE"]), g["Identifier"], npd(g["LEFTARROW"])),
                               seq(g["OPEN"], g["Expression"], g["CLOSE"]),
                               seq(g["BeginTok"], g["Expression"], g["EndTok"]),
                               seq(g["BeginCap"], g["Expression"], g["EndCap"]),
                               g["Literal"], g["Class"], g["DOT"]);

        g["Identifier"] <= seq(g["IdentCont"], g["Spacing"]);
        g["IdentCont"]  <= seq(g["IdentStart"], zom(g["IdentRest"]));
        g["IdentStart"] <= cls("a-zA-Z_\x80-\xff%");
        g["IdentRest"]  <= cho(g["IdentStart"], cls("0-9"));

        g["Literal"]    <= cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))), cls("'"), g["Spacing"]),
                               seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))), cls("\""), g["Spacing"]));

        g["Class"]      <= seq(chr('['), tok(zom(seq(npd(chr(']')), g["Range"]))), chr(']'), g["Spacing"]);

        g["Range"]      <= cho(seq(g["Char"], chr('-'), g["Char"]), g["Char"]);
        g["Char"]       <= cho(seq(chr('\\'), cls("nrt'\"[]\\")),
                               seq(chr('\\'), cls("0-3"), cls("0-7"), cls("0-7")),
                               seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
                               seq(lit("\\x"), cls("0-9a-fA-F"), opt(cls("0-9a-fA-F"))),
                               seq(npd(chr('\\')), dot()));

#if !defined(PEGLIB_NO_UNICODE_CHARS)
        g["LEFTARROW"]  <= seq(cho(lit("<-"), lit(u8"←")), g["Spacing"]);
#else
        g["LEFTARROW"]  <= seq(lit("<-"), g["Spacing"]);
#endif
        ~g["SLASH"]     <= seq(chr('/'), g["Spacing"]);
        g["AND"]        <= seq(chr('&'), g["Spacing"]);
        g["NOT"]        <= seq(chr('!'), g["Spacing"]);
        g["QUESTION"]   <= seq(chr('?'), g["Spacing"]);
        g["STAR"]       <= seq(chr('*'), g["Spacing"]);
        g["PLUS"]       <= seq(chr('+'), g["Spacing"]);
        g["OPEN"]       <= seq(chr('('), g["Spacing"]);
        g["CLOSE"]      <= seq(chr(')'), g["Spacing"]);
        g["DOT"]        <= seq(chr('.'), g["Spacing"]);

        g["Spacing"]    <= zom(cho(g["Space"], g["Comment"]));
        g["Comment"]    <= seq(chr('#'), zom(seq(npd(g["EndOfLine"]), dot())), g["EndOfLine"]);
        g["Space"]      <= cho(chr(' '), chr('\t'), g["EndOfLine"]);
        g["EndOfLine"]  <= cho(lit("\r\n"), chr('\n'), chr('\r'));
        g["EndOfFile"]  <= npd(dot());

        g["BeginTok"]   <= seq(chr('<'), g["Spacing"]);
        g["EndTok"]     <= seq(chr('>'), g["Spacing"]);

        g["BeginCap"]   <= seq(chr('$'), tok(opt(g["Identifier"])), chr('<'), g["Spacing"]);
        g["EndCap"]     <= seq(lit(">"), g["Spacing"]);

        g["IGNORE"]     <= chr('~');

        // Set definition names
        for (auto& x: g) {
            x.second.name = x.first;
        }
    }

    void setup_actions() {
        g["Definition"] = [&](const SemanticValues& sv, any& dt) {
            Data& data = *dt.get<Data*>();

            auto ignore = (sv.size() == 4);
            auto baseId = ignore ? 1u : 0u;

            const auto& name = sv[baseId].get<std::string>();
            auto ope = sv[baseId + 2].get<std::shared_ptr<Ope>>();

            auto& grammar = *data.grammar;
            if (!grammar.count(name)) {
                auto& rule = grammar[name];
                rule <= ope;
                rule.name = name;
                rule.ignoreSemanticValue = ignore;

                if (data.start.empty()) {
                    data.start = name;
                }
            } else {
                data.duplicates.emplace_back(name, sv.c_str());
            }
        };

        g["Expression"] = [&](const SemanticValues& sv) {
            if (sv.size() == 1) {
                return sv[0].get<std::shared_ptr<Ope>>();
            } else {
                std::vector<std::shared_ptr<Ope>> opes;
                for (auto i = 0u; i < sv.size(); i++) {
                    opes.emplace_back(sv[i].get<std::shared_ptr<Ope>>());
                }
                const std::shared_ptr<Ope> ope = std::make_shared<PrioritizedChoice>(opes);
                return ope;
            }
        };

        g["Sequence"] = [&](const SemanticValues& sv) {
            if (sv.size() == 1) {
                return sv[0].get<std::shared_ptr<Ope>>();
            } else {
                std::vector<std::shared_ptr<Ope>> opes;
                for (const auto& x: sv) {
                    opes.emplace_back(x.get<std::shared_ptr<Ope>>());
                }
                const std::shared_ptr<Ope> ope = std::make_shared<Sequence>(opes);
                return ope;
            }
        };

        g["Prefix"] = [&](const SemanticValues& sv) {
            std::shared_ptr<Ope> ope;
            if (sv.size() == 1) {
                ope = sv[0].get<std::shared_ptr<Ope>>();
            } else {
                assert(sv.size() == 2);
                auto tok = sv[0].get<char>();
                ope = sv[1].get<std::shared_ptr<Ope>>();
                if (tok == '&') {
                    ope = apd(ope);
                } else { // '!'
                    ope = npd(ope);
                }
            }
            return ope;
        };

        g["Suffix"] = [&](const SemanticValues& sv) {
            auto ope = sv[0].get<std::shared_ptr<Ope>>();
            if (sv.size() == 1) {
                return ope;
            } else {
                assert(sv.size() == 2);
                auto tok = sv[1].get<char>();
                if (tok == '?') {
                    return opt(ope);
                } else if (tok == '*') {
                    return zom(ope);
                } else { // '+'
                    return oom(ope);
                }
            }
        };

        g["Primary"] = [&](const SemanticValues& sv, any& dt) -> std::shared_ptr<Ope> {
            Data& data = *dt.get<Data*>();

            switch (sv.choice()) {
                case 0: { // Reference
                    auto ignore = (sv.size() == 2);
                    auto baseId = ignore ? 1u : 0u;

                    const auto& ident = sv[baseId].get<std::string>();

                    if (!data.references.count(ident)) {
                        data.references[ident] = sv.c_str(); // for error handling
                    }

                    if (ignore) {
                        return ign(ref(*data.grammar, ident, sv.c_str()));
                    } else {
                        return ref(*data.grammar, ident, sv.c_str());
                    }
                }
                case 1: { // (Expression)
                    return sv[1].get<std::shared_ptr<Ope>>();
                }
                case 2: { // TokenBoundary
                    return tok(sv[1].get<std::shared_ptr<Ope>>());
                }
                case 3: { // Capture
                    const auto& name = sv[0].get<std::string>();
                    auto ope = sv[1].get<std::shared_ptr<Ope>>();
                    return cap(ope, data.match_action, ++data.capture_count, name);
                }
                default: {
                    return sv[0].get<std::shared_ptr<Ope>>();
                }
            }
        };

        g["IdentCont"] = [](const SemanticValues& sv) {
            return std::string(sv.c_str(), sv.length());
        };

        g["Literal"] = [this](const SemanticValues& sv) {
            const auto& tok = sv.tokens.front();
            return lit(resolve_escape_sequence(tok.first, tok.second));
        };
        g["Class"] = [this](const SemanticValues& sv) {
            const auto& tok = sv.tokens.front();
            return cls(resolve_escape_sequence(tok.first, tok.second));
        };

        g["AND"]      = [](const SemanticValues& sv) { return *sv.c_str(); };
        g["NOT"]      = [](const SemanticValues& sv) { return *sv.c_str(); };
        g["QUESTION"] = [](const SemanticValues& sv) { return *sv.c_str(); };
        g["STAR"]     = [](const SemanticValues& sv) { return *sv.c_str(); };
        g["PLUS"]     = [](const SemanticValues& sv) { return *sv.c_str(); };

        g["DOT"] = [](const SemanticValues& /*sv*/) { return dot(); };

        g["BeginCap"] = [](const SemanticValues& sv) { return sv.token(); };
    }

    std::shared_ptr<Grammar> perform_core(
        const char*  s,
        size_t       n,
        std::string& start,
        MatchAction  ma,
        Log          log)
    {
        Data data;
        data.match_action = ma;

        any dt = &data;
        auto r = g["Grammar"].parse(s, n, dt);

        if (!r.ret) {
            if (log) {
                if (r.message_pos) {
                    auto line = line_info(s, r.message_pos);
                    log(line.first, line.second, r.message);
                } else {
                    auto line = line_info(s, r.error_pos);
                    log(line.first, line.second, "syntax error");
                }
            }
            return nullptr;
        }

        auto& grammar = *data.grammar;

        // Check duplicated definitions
        bool ret = data.duplicates.empty();

        for (const auto& x: data.duplicates) {
            if (log) {
                const auto& name = x.first;
                auto ptr = x.second;
                auto line = line_info(s, ptr);
                log(line.first, line.second, "'" + name + "' is already defined.");
            }
        }

        // Check missing definitions
        for (const auto& x : data.references) {
            const auto& name = x.first;
            auto ptr = x.second;
            if (!grammar.count(name)) {
                if (log) {
                    auto line = line_info(s, ptr);
                    log(line.first, line.second, "'" + name + "' is not defined.");
                }
                ret = false;
            }
        }

        if (!ret) {
            return nullptr;
        }

        // Check left recursion
        ret = true;

        for (auto& x: grammar) {
            const auto& name = x.first;
            auto& rule = x.second;

            DetectLeftRecursion lr(name);
            rule.accept(lr);
            if (lr.s_) {
                if (log) {
                    auto line = line_info(s, lr.s_);
                    log(line.first, line.second, "'" + name + "' is left recursive.");
                }
                ret = false;;
            }
        }

        if (!ret) {
            return nullptr;
        }

        // Set root definition
        start = data.start;

        // Automatic whitespace skipping
        if (grammar.count(WHITESPACE_DEFINITION_NAME)) {
            auto& rule = (*data.grammar)[start];
            rule.whitespaceOpe = wsp((*data.grammar)[WHITESPACE_DEFINITION_NAME].get_core_operator());
        }

        return data.grammar;
    }

    bool is_hex(char c, int& v) {
        if ('0' <= c && c <= '9') {
            v = c - '0';
            return true;
        } else if ('a' <= c && c <= 'f') {
            v = c - 'a' + 10;
            return true;
        } else if ('A' <= c && c <= 'F') {
            v = c - 'A' + 10;
            return true;
        }
        return false;
    }

    bool is_digit(char c, int& v) {
        if ('0' <= c && c <= '9') {
            v = c - '0';
            return true;
        }
        return false;
    }

    std::pair<char, size_t> parse_hex_number(const char* s, size_t n, size_t i) {
        char ret = 0;
        int val;
        while (i < n && is_hex(s[i], val)) {
            ret = static_cast<char>(ret * 16 + val);
            i++;
        }
        return std::make_pair(ret, i);
    }

    std::pair<char, size_t> parse_octal_number(const char* s, size_t n, size_t i) {
        char ret = 0;
        int val;
        while (i < n && is_digit(s[i], val)) {
            ret = static_cast<char>(ret * 8 + val);
            i++;
        }
        return std::make_pair(ret, i);
    }

    std::string resolve_escape_sequence(const char* s, size_t n) {
        std::string r;
        r.reserve(n);

        size_t i = 0;
        while (i < n) {
            auto ch = s[i];
            if (ch == '\\') {
                i++;
                switch (s[i]) {
                    case 'n':  r += '\n'; i++; break;
                    case 'r':  r += '\r'; i++; break;
                    case 't':  r += '\t'; i++; break;
                    case '\'': r += '\''; i++; break;
                    case '"':  r += '"';  i++; break;
                    case '[':  r += '[';  i++; break;
                    case ']':  r += ']';  i++; break;
                    case '\\': r += '\\'; i++; break;
                    case 'x': {
                        std::tie(ch, i) = parse_hex_number(s, n, i + 1);
                        r += ch;
                        break;
                    }
                    default: {
                        std::tie(ch, i) = parse_octal_number(s, n, i);
                        r += ch;
                        break;
                    }
                }
            } else {
                r += ch;
                i++;
            }
        }
        return r;
    }

    Grammar g;
};

/*-----------------------------------------------------------------------------
 *  AST
 *---------------------------------------------------------------------------*/

const int AstDefaultTag = -1;

#ifndef PEGLIB_NO_CONSTEXPR_SUPPORT
inline constexpr unsigned int str2tag(const char* str, int h = 0) {
    return !str[h] ? 5381 : (str2tag(str, h + 1) * 33) ^ static_cast<unsigned char>(str[h]);
}

namespace udl {
inline constexpr unsigned int operator "" _(const char* s, size_t) {
    return str2tag(s);
}
}
#endif

template <typename Annotation>
struct AstBase : public Annotation
{
    AstBase(const char* a_path, size_t a_line, size_t a_column, const char* a_name, const std::vector<std::shared_ptr<AstBase>>& a_nodes)
        : path(a_path ? a_path : "")
        , line(a_line)
        , column(a_column)
        , name(a_name)
        , original_name(a_name)
#ifndef PEGLIB_NO_CONSTEXPR_SUPPORT
        , tag(str2tag(a_name))
        , original_tag(tag)
#endif
        , is_token(false)
        , nodes(a_nodes)
    {}

    AstBase(const char* a_path, size_t a_line, size_t a_column, const char* a_name, const std::string& a_token)
        : path(a_path ? a_path : "")
        , line(a_line)
        , column(a_column)
        , name(a_name)
        , original_name(a_name)
#ifndef PEGLIB_NO_CONSTEXPR_SUPPORT
        , tag(str2tag(a_name))
        , original_tag(tag)
#endif
        , is_token(true)
        , token(a_token)
    {}

    AstBase(const AstBase& ast, const char* a_original_name)
        : path(ast.path)
        , line(ast.line)
        , column(ast.column)
        , name(ast.name)
        , original_name(a_original_name)
#ifndef PEGLIB_NO_CONSTEXPR_SUPPORT
        , tag(ast.tag)
        , original_tag(str2tag(a_original_name))
#endif
        , is_token(ast.is_token)
        , token(ast.token)
        , nodes(ast.nodes)
        , parent(ast.parent)
    {}

    const std::string                 path;
    const size_t                      line;
    const size_t                      column;

    const std::string                 name;
    const std::string                 original_name;
#ifndef PEGLIB_NO_CONSTEXPR_SUPPORT
    const unsigned int                tag;
    const unsigned int                original_tag;
#endif

    const bool                        is_token;
    const std::string                 token;

    std::vector<std::shared_ptr<AstBase<Annotation>>> nodes;
    std::shared_ptr<AstBase<Annotation>>              parent;
    static peg::AstBase<Annotation> empty;
    AstBase<Annotation>& operator [] (const char* name)
    {
       for(std::shared_ptr<AstBase<Annotation>>& node : nodes)
       {
          if(node->name == name)
             return *node;
       }
       return empty;
    }
    operator const char *()
    {
       return token.c_str();
    }
};

template <typename T>
void ast_to_s_core(
    const std::shared_ptr<T>& ptr,
    std::string& s,
    int level,
    std::function<std::string (const T& ast, int level)> fn) {

    const auto& ast = *ptr;
    for (auto i = 0; i < level; i++) {
        s += "  ";
    }
    std::string name;
    if (ast.name == ast.original_name) {
        name = ast.name;
    } else {
        name = ast.original_name + "[" + ast.name + "]";
    }
    if (ast.is_token) {
        s += "- " + name + " (" + ast.token + ")\n";
    } else {
        s += "+ " + name + "\n";
    }
    if (fn) {
      s += fn(ast, level + 1);
    }
    for (auto node : ast.nodes) {
        ast_to_s_core(node, s, level + 1, fn);
    }
}

template <typename T>
std::string ast_to_s(
    const std::shared_ptr<T>& ptr,
    std::function<std::string (const T& ast, int level)> fn = nullptr) {

    std::string s;
    ast_to_s_core(ptr, s, 0, fn);
    return s;
}

struct AstOptimizer
{
    AstOptimizer(bool optimize_nodes, const std::vector<std::string>& filters = {})
        : optimize_nodes_(optimize_nodes)
        , filters_(filters) {}

    template <typename T>
    std::shared_ptr<T> optimize(std::shared_ptr<T> original, std::shared_ptr<T> parent = nullptr) {

        auto found = std::find(filters_.begin(), filters_.end(), original->name) != filters_.end();
        bool opt = optimize_nodes_ ? !found : found;

        if (opt && original->nodes.size() == 1) {
            auto child = optimize(original->nodes[0], parent);
            return std::make_shared<T>(*child, original->name.c_str());
        }

        auto ast = std::make_shared<T>(*original);
        ast->parent = parent;
        ast->nodes.clear();
        for (auto node : original->nodes) {
            auto child = optimize(node, ast);
            ast->nodes.push_back(child);
        }
        return ast;
    }

private:
    const bool                     optimize_nodes_;
    const std::vector<std::string> filters_;
};

template <typename T>
static std::shared_ptr<T> optimize_ast(
    std::shared_ptr<T> ast,
    const std::vector<std::string>& filters = {}) {
    return AstOptimizer(true, filters).optimize(ast);
}

struct EmptyType {};
typedef AstBase<EmptyType> Ast;

/*-----------------------------------------------------------------------------
 *  parser
 *---------------------------------------------------------------------------*/

class parser
{
public:
    parser() = default;

    parser(const char* s, size_t n) {
        load_grammar(s, n);
    }

    parser(const char* s)
        : parser(s, strlen(s)) {}

    operator bool() {
        return grammar_ != nullptr;
    }

    bool load_grammar(const char* s, size_t n) {
        grammar_ = ParserGenerator::parse(
            s, n,
            start_,
            [&](const char* a_s, size_t a_n, size_t a_id, const std::string& a_name) {
                if (match_action) match_action(a_s, a_n, a_id, a_name);
            },
            log);

        return grammar_ != nullptr;
    }

    bool load_grammar(const char* s) {
        auto n = strlen(s);
        return load_grammar(s, n);
    }

    bool parse_n(const char* s, size_t n, const char* path = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, n, path);
            output_log(s, n, r);
            return r.ret && r.len == n;
        }
        return false;
    }

    bool parse(const char* s, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse_n(s, n, path);
    }

    bool parse_n(const char* s, size_t n, any& dt, const char* path = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse(s, n, dt, path);
            output_log(s, n, r);
            return r.ret && r.len == n;
        }
        return false;
    }

    bool parse(const char* s, any& dt, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse_n(s, n, dt, path);
    }

    template <typename T>
    bool parse_n(const char* s, size_t n, T& val, const char* path = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse_and_get_value(s, n, val, path);
            output_log(s, n, r);
            return r.ret && r.len == n;
        }
        return false;
    }

    template <typename T>
    bool parse(const char* s, T& val, const char* path = nullptr) const {
        auto n = strlen(s);
        return parse_n(s, n, val, path);
    }

    template <typename T>
    bool parse_n(const char* s, size_t n, any& dt, T& val, const char* path = nullptr) const {
        if (grammar_ != nullptr) {
            const auto& rule = (*grammar_)[start_];
            auto r = rule.parse_and_get_value(s, n, dt, val, path);
            output_log(s, n, r);
            return r.ret && r.len == n;
        }
        return false;
    }

    template <typename T>
    bool parse(const char* s, any& dt, T& val, const char* /*path*/ = nullptr) const {
        auto n = strlen(s);
        return parse_n(s, n, dt, val);
    }

    bool search(const char* s, size_t n, size_t& mpos, size_t& mlen) const {
        const auto& rule = (*grammar_)[start_];
        if (grammar_ != nullptr) {
            size_t pos = 0;
            while (pos < n) {
                size_t len = n - pos;
                auto r = rule.parse(s + pos, len);
                if (r.ret) {
                    mpos = pos;
                    mlen = len;
                    return true;
                }
                pos++;
            }
        }
        mpos = 0;
        mlen = 0;
        return false;
    }

    bool search(const char* s, size_t& mpos, size_t& mlen) const {
        auto n = strlen(s);
        return search(s, n, mpos, mlen);
    }

    Definition& operator[](const char* s) {
        return (*grammar_)[s];
    }

    void enable_packrat_parsing() {
        if (grammar_ != nullptr) {
            auto& rule = (*grammar_)[start_];
            rule.enablePackratParsing = true;
        }
    }

    template <typename T = Ast>
    parser& enable_ast() {
        for (auto& x: *grammar_) {
            const auto& name = x.first;
            auto& rule = x.second;

            if (!rule.action) {
                auto is_token = rule.is_token;
                rule.action = [=](const SemanticValues& sv) {
                    auto line = line_info(sv.ss, sv.c_str());

                    if (is_token) {
                        return std::make_shared<T>(sv.path, line.first, line.second, name.c_str(), sv.token());
                    }

                    auto ast = std::make_shared<T>(sv.path, line.first, line.second, name.c_str(), sv.transform<std::shared_ptr<T>>());

                    for (auto node: ast->nodes) {
                        node->parent = ast;
                    }
                    return ast;
                };
            }
        }
        return *this;
    }

    void enable_trace(Tracer tracer) {
        if (grammar_ != nullptr) {
            auto& rule = (*grammar_)[start_];
            rule.tracer = tracer;
        }
    }

    MatchAction match_action;
    Log         log;

private:
    void output_log(const char* s, size_t n, const Definition::Result& r) const {
        if (log) {
            if (!r.ret) {
                if (r.message_pos) {
                    auto line = line_info(s, r.message_pos);
                    log(line.first, line.second, r.message);
                } else {
                    auto line = line_info(s, r.error_pos);
                    log(line.first, line.second, "syntax error");
                }
            } else if (r.len != n) {
                auto line = line_info(s, s + r.len);
                log(line.first, line.second, "syntax error");
            }
        }
    }

    std::shared_ptr<Grammar> grammar_;
    std::string              start_;
};

/*-----------------------------------------------------------------------------
 *  Simple interface
 *---------------------------------------------------------------------------*/

struct match
{
    struct Item {
        const char* s;
        size_t      n;
        size_t      id;
        std::string name;

        size_t length() const { return n; }
        std::string str() const { return std::string(s, n); }
    };

    std::vector<Item> matches;

    typedef std::vector<Item>::iterator iterator;
    typedef std::vector<Item>::const_iterator const_iterator;

    bool empty() const {
        return matches.empty();
    }

    size_t size() const {
        return matches.size();
    }

    size_t length(size_t n = 0) {
        return matches[n].length();
    }

    std::string str(size_t n = 0) const {
        return matches[n].str();
    }

    const Item& operator[](size_t n) const {
        return matches[n];
    }

    iterator begin() {
        return matches.begin();
    }

    iterator end() {
        return matches.end();
    }

    const_iterator begin() const {
        return matches.cbegin();
    }

    const_iterator end() const {
        return matches.cend();
    }

    std::vector<size_t> named_capture(const std::string& name) const {
        std::vector<size_t> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            if (matches[i].name == name) {
                ret.push_back(i);
            }
        }
        return ret;
    }

    std::map<std::string, std::vector<size_t>> named_captures() const {
        std::map<std::string, std::vector<size_t>> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            ret[matches[i].name].push_back(i);
        }
        return ret;
    }

    std::vector<size_t> indexed_capture(size_t id) const {
        std::vector<size_t> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            if (matches[i].id == id) {
                ret.push_back(i);
            }
        }
        return ret;
    }

    std::map<size_t, std::vector<size_t>> indexed_captures() const {
        std::map<size_t, std::vector<size_t>> ret;
        for (auto i = 0u; i < matches.size(); i++) {
            ret[matches[i].id].push_back(i);
        }
        return ret;
    }
};

inline bool peg_match(const char* syntax, const char* s, match& m) {
    m.matches.clear();

    parser pg(syntax);
    pg.match_action = [&](const char* a_s, size_t a_n, size_t a_id, const std::string& a_name) {
        m.matches.push_back(match::Item{ a_s, a_n, a_id, a_name });
    };

    auto ret = pg.parse(s);
    if (ret) {
        auto n = strlen(s);
        m.matches.insert(m.matches.begin(), match::Item{ s, n, 0, std::string() });
    }

    return ret;
}

inline bool peg_match(const char* syntax, const char* s) {
    parser parser(syntax);
    return parser.parse(s);
}

inline bool peg_search(parser& pg, const char* s, size_t n, match& m) {
    m.matches.clear();

    pg.match_action = [&](const char* a_s, size_t a_n, size_t a_id, const std::string& a_name) {
        m.matches.push_back(match::Item{ a_s, a_n, a_id, a_name });
    };

    size_t mpos, mlen;
    auto ret = pg.search(s, n, mpos, mlen);
    if (ret) {
        m.matches.insert(m.matches.begin(), match::Item{ s + mpos, mlen, 0, std::string() });
        return true;
    }

    return false;
}

inline bool peg_search(parser& pg, const char* s, match& m) {
    auto n = strlen(s);
    return peg_search(pg, s, n, m);
}

inline bool peg_search(const char* syntax, const char* s, size_t n, match& m) {
    parser pg(syntax);
    return peg_search(pg, s, n, m);
}

inline bool peg_search(const char* syntax, const char* s, match& m) {
    parser pg(syntax);
    auto n = strlen(s);
    return peg_search(pg, s, n, m);
}

class peg_token_iterator : public std::iterator<std::forward_iterator_tag, match>
{
public:
    peg_token_iterator()
        : s_(nullptr)
        , l_(0)
        , pos_((std::numeric_limits<size_t>::max)()) {}

    peg_token_iterator(const char* syntax, const char* s)
        : peg_(syntax)
        , s_(s)
        , l_(strlen(s))
        , pos_(0) {
        peg_.match_action = [&](const char* a_s, size_t a_n, size_t a_id, const std::string& a_name) {
            m_.matches.push_back(match::Item{ a_s, a_n, a_id, a_name });
        };
        search();
    }

    peg_token_iterator(const peg_token_iterator& rhs)
        : peg_(rhs.peg_)
        , s_(rhs.s_)
        , l_(rhs.l_)
        , pos_(rhs.pos_)
        , m_(rhs.m_) {}

    peg_token_iterator& operator++() {
        search();
        return *this;
    }

    peg_token_iterator operator++(int) {
        auto it = *this;
        search();
        return it;
    }

    match& operator*() {
        return m_;
    }

    match* operator->() {
        return &m_;
    }

    bool operator==(const peg_token_iterator& rhs) {
        return pos_ == rhs.pos_;
    }

    bool operator!=(const peg_token_iterator& rhs) {
        return pos_ != rhs.pos_;
    }

private:
    void search() {
        m_.matches.clear();
        size_t mpos, mlen;
        if (peg_.search(s_ + pos_, l_ - pos_, mpos, mlen)) {
            m_.matches.insert(m_.matches.begin(), match::Item{ s_ + mpos, mlen, 0, std::string() });
            pos_ += mpos + mlen;
        } else {
            pos_ = (std::numeric_limits<size_t>::max)();
        }
    }

    parser      peg_;
    const char* s_;
    size_t      l_;
    size_t      pos_;
    match       m_;
};

struct peg_token_range {
    typedef peg_token_iterator iterator;
    typedef const peg_token_iterator const_iterator;

    peg_token_range(const char* syntax, const char* s)
        : beg_iter(peg_token_iterator(syntax, s))
        , end_iter() {}

    iterator begin() {
        return beg_iter;
    }

    iterator end() {
        return end_iter;
    }

    const_iterator cbegin() const {
        return beg_iter;
    }

    const_iterator cend() const {
        return end_iter;
    }

private:
    peg_token_iterator beg_iter;
    peg_token_iterator end_iter;
};

} // namespace peg


template<class _Elem, class _Traits, typename Annotation>
std::basic_ios<_Elem, _Traits> &operator << (std::basic_ios<_Elem, _Traits> &ios,
      peg::AstBase<Annotation> &node)
{
   return ios << node.token.c_str();
}

#endif

// vim: et ts=4 sw=4 cin cino={1s ff=unix
