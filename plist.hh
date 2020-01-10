#ifndef __CRZ_PLIST_HH__
#define __CRZ_PLIST_HH__

#include <algorithm>
#include <type_traits>
#include <typeinfo>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <sstream>

namespace crz {

// 格式化字符串的极简实现
#define format(__stream) (dynamic_cast<std::ostringstream & >(std::ostringstream() __stream).str())

// 以下为一些自定义的异常

// 进行两个不同类型的比较
class bad_comparison : public std::logic_error {
public:
    explicit bad_comparison(const std::type_info &lhs, const std::type_info &rhs, const char *opt) :
            std::logic_error(
                    format(<< "bad comparison: "<< lhs.name() << " " << opt << " " << rhs.name())) {}
};

// 访问一个空值
class bad_pcell_access : public std::logic_error {
public:
    bad_pcell_access() : std::logic_error("bad pcell access") {}
};

// 将pcell转换到错误的类型
class bad_pcell_cast : public std::logic_error {
public:
    bad_pcell_cast(const std::type_info &from, const std::type_info &to) :
            std::logic_error(
                    format(<< "bad pcell cast: from "<< from.name() << " to " << to.name())) {}
};


// 实现一些必要的工具
namespace detail {

// 利用SFINAE机制，辅助模板偏特化根据类型的操作集合来派发对应的特化模板
template<typename ...>
using __void_t = void;

// 若给定类型未重载<<运算符，则输出给定的字符串
template<typename T, typename = __void_t<>>
struct __default_print {
    static std::ostream &print_with_default(std::ostream &os, const T &t, const std::string &s) {
        (void) t;
        return os << s;
    }
};

// 若给定类型重载了<<运算符，则输出该类型的对象
template<typename T>
struct __default_print<T, __void_t<decltype(std::cout << std::declval<T>())>> {
    static std::ostream &print_with_default(std::ostream &os, const T &t, const std::string &s) {
        (void) s;
        return os << t;
    }
};

// 利用宏批量生成模板
// 若给定类型重载了给定的比较运算符，则正常进行比较，否则抛出异常
#define DEFAULT_COMPARER(opt, name)\
template<typename T, typename = __void_t<>>\
struct __default_##name {\
    static bool compare(const T &a, const T &b) {\
        throw bad_comparison(typeid(T), typeid(T), #opt);\
    }\
};\
template<typename T>\
struct __default_##name <T, __void_t<decltype(std::declval<T>() opt std::declval<T>())>> {\
    static bool compare(const T &a, const T &b) {\
        return a opt b;\
    }\
};

DEFAULT_COMPARER(==, equal)

DEFAULT_COMPARER(<, less)

DEFAULT_COMPARER(>, greater)

#undef DEFAULT_COMPARER

// 获得给定函数类型的第n个参数类型。其中函数类型可以为普通函数、函数指针、函数对象（重载了operator()的对象，包括lambda表达式）
template<unsigned N, typename T, typename ...Args>
struct __nth_type_wrapper {
    using type = typename __nth_type_wrapper<N - 1, Args...>::type;
};
template<typename T, typename ...Args>
struct __nth_type_wrapper<0, T, Args...> {
    using type = T;
};
template<unsigned N, typename ...Args>
using __nth_type = typename __nth_type_wrapper<N, Args...>::type;

template<unsigned N, typename Ret, typename ...Args>
auto __nth_arg_infer_fun_ptr(Ret(*)(Args...)) -> __nth_type<N, Args...>;
template<unsigned N, typename Ret, typename F, typename ...Args>
auto __nth_arg_infer_callable(Ret(F::*)(Args...)) -> __nth_type<N, Args...>;
template<unsigned N, typename Ret, typename F, typename ...Args>
auto __nth_arg_infer_callable(Ret(F::*)(Args...) const) -> __nth_type<N, Args...>;

template<unsigned N, typename F, typename = __void_t<>>
struct __nth_arg_type_wrapper {
    using type = decltype(__nth_arg_infer_fun_ptr<N>(std::declval<typename std::decay<F>::type>()));
};
template<unsigned N, typename F>
struct __nth_arg_type_wrapper<N, F, __void_t<decltype(&F::operator())>> {
    using type = decltype(__nth_arg_infer_callable<N>(&F::operator()));
};
template<unsigned N, typename F>
using __nth_arg_type = typename __nth_arg_type_wrapper<N, typename std::remove_reference<F>::type>::type;


// 获得给定函数类型的返回值类型
template<typename Ret, typename ...Args>
auto __ret_infer_fun_ptr(Ret(*)(Args...)) -> Ret;
template<typename Ret, typename F, typename ...Args>
auto __ret_infer_callable(Ret(F::*)(Args...)) -> Ret;
template<typename Ret, typename F, typename ...Args>
auto __ret_infer_callable(Ret(F::*)(Args...) const) -> Ret;

template<typename F, typename = __void_t<>>
struct __ret_type_wrapper {
    using type = decltype(__ret_infer_fun_ptr(std::declval<typename std::decay<F>::type>()));
};
template<typename F>
struct __ret_type_wrapper<F, __void_t<decltype(&F::operator())>> {
    using type = decltype(__ret_infer_callable(&F::operator()));
};
template<typename F>
using __ret_type = typename __ret_type_wrapper<typename std::remove_reference<F>::type>::type;

}

// 可以存放不同类型对象的容器。
class pcell {

    // 对象持有者的基类，定义了一些虚函数用于运行时的动态操作
    struct holder {
        virtual ~holder() noexcept = default;
        virtual holder *clone() const = 0; // 返回自身的拷贝（持有的对象也进行了拷贝）
        virtual const std::type_info &type() const noexcept = 0;
        virtual std::ostream &print(std::ostream &os) const = 0;
        virtual std::string id() const = 0;
        virtual bool equal(const holder &h) const = 0;
        virtual bool less(const holder &h) const = 0;
        virtual bool greater(const holder &h) const = 0;
        friend std::ostream &operator<<(std::ostream &os, const holder &h) {
            return h.print(os);
        }
    };

    // 真正的对象持有者
    template<typename T>
    struct holder_impl : public holder {
        T val;

        holder_impl() = default;
        template<typename ...Args>
        explicit holder_impl(Args &&...args):val(std::forward<Args>(args)...) {}
        holder *clone() const override {
            return new holder_impl<T>(val);
        }
        const std::type_info &type() const noexcept override {
            return typeid(T);
        }
        std::string id() const override {
            return format(<< type().name() << this);
        }
        std::ostream &print(std::ostream &os) const override {
            return detail::__default_print<T>::print_with_default(os, val, id());
        }
        bool equal(const holder &h) const override {
            return type() == h.type()
                   ? detail::__default_equal<T>::compare(val,
                                                         dynamic_cast<const holder_impl <T> &>(h).val)
                   : false;
        }
        bool less(const holder &h) const override {
            if (type() != h.type())
                throw bad_comparison(type(), h.type(), "<");
            return detail::__default_less<T>::compare(val,
                                                      dynamic_cast<const holder_impl <T> &>(h).val);
        }
        bool greater(const holder &h) const override {
            if (type() != h.type())
                throw bad_comparison(type(), h.type(), ">");
            return detail::__default_greater<T>::compare(val,
                                                         dynamic_cast<const holder_impl <T> &>(h).val);
        }
    };

    holder *hdr{nullptr};
    holder *clone() const { return hdr ? hdr->clone() : nullptr; }

    template<typename T>
    using deref_type = typename std::remove_reference<T>::type;

    template<typename T>
    using decay_type = typename std::decay<T>::type;

    template<typename T>
    using base_type = decay_type<deref_type<T>>;

public:
    pcell() = default;
    // 隐式构造函数，可以进行其他类型到pcell的隐式转换
    template<typename T, typename B = base_type<T>,
            typename = typename std::enable_if<!std::is_same<B, pcell>::value>::type>
    pcell(T &&t) : hdr(new holder_impl<B>(std::forward<T>(t))) {}
    // 拷贝构造函数，调用clone函数来动态地拷贝值
    pcell(const pcell &rhs) : hdr(rhs.clone()) {}
    // 移动构造函数，直接交换指针
    pcell(pcell &&rhs) noexcept : hdr(rhs.hdr) { rhs.hdr = nullptr; }

    ~pcell() { reset(); }

    // 拷贝运算符。rhs构造时会根据情况调用拷贝构造和或移动构造，因此只要实现一个拷贝运算即可。
    pcell &operator=(pcell rhs) {
        swap(rhs);
        return *this;
    }
    template<typename T, typename B = base_type<T>,
            typename = typename std::enable_if<!std::is_same<B, pcell>::value>::type>
    pcell &operator=(T &&t) {
        reset(), hdr = new holder_impl<B>(std::forward<T>(t));
        return *this;
    }

    // 清除容器
    void reset() noexcept {
        if (hdr)
            delete (hdr), hdr = nullptr;
    }
    void swap(pcell &rhs) noexcept {
        std::swap(hdr, rhs.hdr);
    }
    // 判断容器内是否包含着对象
    bool has_value() const noexcept {
        return hdr != nullptr;
    }
    // 返回容器包含对象的类型的type_info
    const std::type_info &type() const noexcept {
        return has_value() ? hdr->type() : typeid(void);
    }
    std::string id() const {
        return has_value() ? hdr->id() : std::string("None");
    }
    // 返回容器内的对象的值
    // 目标的底层类型不为pcell
    template<typename T, typename B = base_type<T>, typename = void,
            typename = typename std::enable_if<
                    !std::is_same<B, pcell>::value
            >::type>
    T cast() {
        return get_holder_impl<T>()->val;
    }
    // 目标的底层类型不为pcell且目标不为非常量引用
    template<typename T, typename B = base_type<T>, typename = void,
            typename = typename std::enable_if<
                    !std::is_same<B, pcell>::value &&
                    (!std::is_reference<T>::value ||
                     std::is_const<deref_type<T>>::value)
            >::type>
    T cast() const {
        return get_holder_impl<T>()->val;
    }
    // 目标的底层类型为pcell
    template<typename T, typename B = base_type<T>,
            typename = typename std::enable_if<
                    std::is_same<B, pcell>::value
            >::type>
    T cast() {
        return *this;
    }
    // 目标的底层类型为pcell且目标不为非常量引用
    template<typename T, typename B = base_type<T>,
            typename = typename std::enable_if<
                    std::is_same<B, pcell>::value &&
                    (!std::is_reference<T>::value ||
                     std::is_const<deref_type<T>>::value)
            >::type>
    T cast() const {
        return *this;
    }
    // 判断容器内的对象是否为给定类型（其实可以直接用type_info判断）
    template<typename T>
    bool is() const {
        return has_value() ? false : dynamic_cast<holder_impl <T> *>(hdr) != nullptr;
    }
    // 显式类型转换函数
    template<typename T>
    explicit operator T() const {
        return cast<T>();
    }

    // 重载<<运算；如果容器内为空，则输出"None"
    friend std::ostream &operator<<(std::ostream &os, const pcell &cell) {
        return cell.has_value() ? (os << *cell.hdr) : (os << "None");
    }
    // 重载==和!=；当前仅当两者都为空或都含有相同的值视作相等，其他情况视作不相等（并不会抛出异常）
    // 不过，如果两者类型相同，但其类型没有重载==，则会抛出异常
    friend bool operator==(const pcell &a, const pcell &b) {
        if (a.has_value() && b.has_value())
            return a.hdr->equal(*b.hdr);
        return a.has_value() == b.has_value();
    }
    friend bool operator!=(const pcell &a, const pcell &b) {
        return !(a == b);
    }
    // 重载< > <= >=；注意如果其中一方为空或者两者类型不相同则会抛出异常
    // 两者类型相同但没有重载相应运算符，也会抛出异常
    friend bool operator<(const pcell &a, const pcell &b) {
        if (a.has_value() && b.has_value())
            return a.hdr->less(*b.hdr);
        throw bad_comparison(a.type(), b.type(), "<");
    }
    friend bool operator>(const pcell &a, const pcell &b) {
        if (a.has_value() && b.has_value())
            return a.hdr->greater(*b.hdr);
        throw bad_comparison(a.type(), b.type(), ">");
    }
    friend bool operator<=(const pcell &a, const pcell &b) {
        return a < b || a == b;
    }
    friend bool operator>=(const pcell &a, const pcell &b) {
        return a > b || a == b;
    }

private:
    template<typename T, typename B = base_type<T>>
    holder_impl<B> *get_holder_impl() const {
        if (!has_value())
            throw bad_pcell_access();
        auto hdr_impl = dynamic_cast<holder_impl <B> *>(hdr);
        if (!hdr_impl)
            throw bad_pcell_cast(type(), typeid(T));
        return hdr_impl;
    }
};


// python-like list，继承自vector以复用其大部分的函数
class plist : public std::vector<pcell> {
    // 类似于python中的slice类型
    class maybe_int {
        int val{0};
        bool has_val{false};

    public:
        maybe_int(int v) : val(v), has_val(true) {}
        maybe_int() = default;
        bool has_value() const { return has_val; }
        operator int() const { return val; }
    };

    class pslice {
        maybe_int start_{}, stop_{};
        int step_{1};

    public:
        pslice(maybe_int b, maybe_int e, int s = 1) : start_(b), stop_(e), step_(s) {
            if (s == 0)
                throw std::logic_error("slice step must be non-zero");
        }

        maybe_int start() const { return start_; }
        maybe_int stop() const { return stop_; }
        int step() const { return step_; }
    };

    int index_trans(int i) const {
        if (i < 0)
            i += size();
        if (i < 0)
            throw std::out_of_range("index out of range");
        return i;
    }
public:
    using std::vector<pcell>::vector;

    // 直接索引访问，支持负数索引。返回对应pcell的引用
    pcell &operator[](int i) {
        return std::vector<pcell>::operator[](index_trans(i));
    }
    const pcell &operator[](int i) const {
        return std::vector<pcell>::operator[](index_trans(i));
    }
    // 切片索引访问，返回一个新的plist
    plist operator[](pslice sl) const {
        plist res;
        int step = sl.step(), len = size();
        if (!sl.start().has_value() && !sl.stop().has_value()) {
            if (step > 0) {
                for (int i = 0; i < len; i += step)
                    res.push_back(std::vector<pcell>::operator[](i));
            } else {
                for (int i = len - 1; i >= 0; i += step)
                    res.push_back(std::vector<pcell>::operator[](i));
            }
        } else {
            int start = sl.start().has_value() ? index_trans(sl.start()) : 0;
            int stop = sl.stop().has_value() ? index_trans(sl.stop()) : len;
            if (step > 0 && start <= stop) {
                for (int i = start; i < stop; i += step)
                    res.push_back(std::vector<pcell>::operator[](i));
            } else if (step < 0 && start >= stop) {
                for (int i = start; i > stop; i += step)
                    res.push_back(std::vector<pcell>::operator[](i));
            }
        }
        return res;
    }

    // 列表连接
    friend plist operator+(const plist &a, const plist &b) {
        auto res = a;
        return res += b;
    }
    plist &operator+=(const plist &pl) {
        size_t len = pl.size();
        for (size_t i = 0; i < len; ++i)
            push_back(pl[i]);
        return *this;
    }
    // 列表拷贝
    friend plist operator*(size_t time, const plist &pl) {
        return pl * time;
    }
    friend plist operator*(const plist &pl, size_t time) {
        auto res = pl;
        return res *= time;
    }
    plist &operator*=(size_t time) {
        size_t old_len = size();
        for (size_t t = 1; t < time; ++t) {
            for (size_t i = 0; i < old_len; ++i) {
                push_back((*this)[i]);
            }
        }
        return *this;
    }
    // 列表输出
    friend std::ostream &operator<<(std::ostream &os, const plist &pl) {
        if (pl.empty())
            return os << "[]";
        os << '[';
        auto it = pl.begin(), stop = pl.end();
        os << *it++;
        for (; it != stop; ++it)
            os << ", " << *it;
        os << ']';
        return os;
    }
    // 列表转换为字符串
    explicit operator std::string() const {
        return format(<< *this);
    }

    // python API
    size_t count(const pcell &pc) const {
        return std::count(begin(), end(), pc);
    }
    int index(const pcell &pc) const {
        auto it = std::find(begin(), end(), pc);
        return it == end() ? -1 : static_cast<int>(it - begin());
    }
    void append(const pcell &pc) {
        push_back(pc);
    }
    void extend(const plist &pl) {
        *this += pl;
    }
    void remove(const pcell &pc) {
        erase(std::remove(begin(), end(), pc), end());
    }
    void reverse() {
        std::reverse(begin(), end());
    }

    // 排序函数。其中key是一个一元函数（类型为A => B），和python中的一样。rvs代表是否逆序排序
    template<typename F = std::function<const pcell &(const pcell &)>>
    plist &sort(bool rvs = false, F key = [](const pcell &x) -> const pcell & { return x; }) {
        using arg_type = detail::__nth_arg_type<0, F>;
        !rvs
        ? std::sort(begin(), end(),
                    [&](const pcell &a, const pcell &b) {
                        return key(a.cast<arg_type>()) < key(b.cast<arg_type>());
                    })
        : std::sort(begin(), end(),
                    [&](const pcell &a, const pcell &b) {
                        return !(key(a.cast<arg_type>()) < key(b.cast<arg_type>()));
                    });
        return *this;
    }

    // 其他常用的列表操作
    template<typename F>
    plist &for_each(F trans) {
        using arg_type = detail::__nth_arg_type<0, F>;
        for (auto &x: *this)
            trans(x.cast<arg_type>());
        return *this;
    }

    template<typename F>
    plist map(F mapping) {
        using arg_type = detail::__nth_arg_type<0, F>;
        plist res;
        res.reserve(size());
        for (const auto &x: *this)
            res.push_back(mapping(x.cast<arg_type>()));
        return res;
    }

    template<typename F>
    plist filter(F pred) {
        using arg_type = detail::__nth_arg_type<0, F>;
        plist res;
        for (const auto &x: *this) {
            if (pred(x.cast<arg_type>()))
                res.push_back(x);
        }
        return res;
    }
};

}

namespace std {
// 特化std中的swap函数
void swap(crz::pcell &a, crz::pcell &b) {
    a.swap(b);
}

#undef format

}

#endif //__CRZ_PLIST_HH__
