#include <iostream>
#include "plist.hh"
#include <string>
#include <functional>
#include <list>
#include <utility>
#include <cstdlib>
#include <ctime>

std::list<std::pair<const char *, std::function<void(void)>>> test_list;

#define CAT_(a, b) a ## b
#define CAT(a, b) CAT_(a, b)
#define STR_(x) # x
#define STR(x) STR_(x)
#define TEST(__name, __open) \
void CAT(test_, __name) (); \
const int CAT(__registry_, __name) = []()->int{if (__open) test_list.emplace_back(STR(__name), CAT(test_, __name)); return 0;}();\
void CAT(test_, __name) ()

struct UserType {
    int x;
    friend std::ostream &operator<<(std::ostream &os, const UserType &ut) {
        return os << "User(" << ut.x << ")";
    }
};

template<typename T>
void println(const T &t) {
    std::cout << t << std::endl;
}

TEST(create_plist, true) {
    crz::plist l1{1, 1.2, std::string("???"), UserType{1}}; // 用初始化列表构造
    println(l1); // [1, 1.2, ???, User(1)]

    crz::plist l2(3); // 构造含有3个空元素的列表
    println(l2); // [None, None, None]

    crz::plist l3(2, l1); // 构造含有2个l1的列表
    println(l3); // [[1, 1.2, ???, User(1)], [1, 1.2, ???, User(1)]]

    std::vector<int> vi{1, 2, 3};
    crz::plist l4(vi.begin(), vi.end()); // 用迭代器构造列表
    println(l4); // [1, 2, 3]

    crz::plist l5 = l4; // 拷贝构造
    println(l5); // [1, 2, 3]

    crz::plist l6 = std::move(l5); // 移动构造
    println(l5); // []
    println(l6); // [1, 2, 3]
}

TEST(add_element, true) {
    crz::plist l;
    println(l); // []

    l.push_back(1);
    println(l); // [1]

    l.insert(l.begin(), "wow");
    println(l); // [wow, 1]

    l.append(UserType{2});
    println(l); // [wow, 1, User(2)]
}

TEST(remove_element, true) {
    crz::plist l{1, 1.2, std::string("hello"), std::string("hello"), UserType{3}};
    println(l); // [1, 1.2, hello, hello, User(3)]

    l.erase(l.begin());
    println(l); // [1.2, hello, hello, User(3)]

    l.pop_back();
    println(l); // [1.2, hello, hello]

    l.remove(std::string("hello"));
    println(l); // [1.2]
}

TEST(concat_and_duplicate_list, true) {
    crz::plist l{1, 1.2};
    println(l); // [1, 1.2]

    l += {"wow", UserType{3}};
    println(l); // [1, 1.2, wow, User(3)]
    println(crz::plist{"???", 2.2} + l); // [???, 2.2, 1, 1.2, wow, User(3)]
    println(2 * l); // [1, 1.2, wow, User(3), 1, 1.2, wow, User(3)]
    println(l * 2); // [1, 1.2, wow, User(3), 1, 1.2, wow, User(3)]

    l.extend(l);
    println(l); // [1, 1.2, wow, User(3), 1, 1.2, wow, User(3)]
}

TEST(embed_list, true) {
    crz::plist l{"禁止套娃"};
    for (int i = 0; i < 5; ++i)
        l = {"禁止", l};
    println(l); // [禁止, [禁止, [禁止, [禁止, [禁止, [禁止套娃]]]]]]
}

TEST(list_index_and_slice, true) {
    crz::plist l{1, 1.2, "wow", std::string("???"), UserType{3}};
    // l[2]
    println(l[2]); // wow
    // l[-2]
    println(l[-2]); // ???
    // l[1:3]
    println(l[{1, 3}]); // [1.2, wow]
    // l[1:-1]
    println(l[{1, -1}]); // [1.2, wow, ???]
    // l[:]
    println(l[{{},
               {}}]); // [1, 1.2, wow, ???, User(3)]
    // l[-1:]
    println(l[{-1, {}}]); // [User(3)]
    // l[:1]
    println(l[{{}, 1}]); // [1]
    // l[::-1]
    println(l[{{}, {}, -1}]); // [User(3), ???, wow, 1.2, 1]
    // l[::2]
    println(l[{{}, {}, 2}]); // [1, wow, User(3)]
    // l[1:3:-1]
    println(l[{1, 3, -1}]); // []
    // l[3:1:1]
    println(l[{3, 1, 1}]); // []
}

TEST(element_query, true) {
    crz::plist l{1, 2, std::string("???"), 2, std::string("!!!"), 2};
    // 查找元素第一次出现的索引
    println(l.index(std::string("???"))); // 2
    println(l.index(15)); // -1
    // 元素出现的次数
    println(l.count(2)); // 3
    println(l.count(std::string("..."))); // 0
}

TEST(list_reverse, true) {
    crz::plist l{"灵梦", "早苗", "魔理沙"};
    println(l); // [灵梦, 早苗, 魔理沙]
    l.reverse();
    println(l); // [魔理沙, 早苗, 灵梦]
}

TEST(list_sort, true) {
    crz::plist l{std::string("Reimu"), std::string("Marisa"), std::string("Sakuya"),
                 std::string("Sanae")};
    println(l); // [Reimu, Marisa, Sakuya, Sanae]
    l.sort();
    println(l); // [Marisa, Reimu, Sakuya, Sanae]
    l.sort(true); // 逆序排序
    println(l); // [Sanae, Sakuya, Reimu, Marisa]
    l.sort(false, [](const std::string &s) {
        return std::make_pair(s.length(), s); // 先比长度，再比字典序
    });
    println(l); // [Reimu, Sanae, Marisa, Sakuya]

    crz::plist il;
    int len = l.size();
    for (int i = 0; i < len; ++i)
        il.push_back(i); // il中保存l的索引
    println(il); // [0, 1, 2, 3]
    il.sort(false, [&](int i) { return l[i]; }); // 根据索引对应的l中的元素对索引排序
    println(il); // [2, 0, 3, 1]
}

TEST(list_operator, true) {
    crz::plist l{1, 2, 3, 4};
    println(l.map([](int x) { return x + 1; })); // [2, 3, 4, 5]
    println(l.map([](int x) { return std::to_string(x); })
             .map([](const std::string &s) { return s + "..."; })); // [1..., 2..., 3..., 4...]
    l.for_each([](int &x) { x += 1; }).for_each([](int x) { println(x); }); // 2\n3\n4\n5
    println(l); // [2, 3, 4, 5]
    println(l.map([](int x) { return x + 1; })
             .filter([](int x) { return x < 5; })); // [3, 4]
}


TEST(default_output, true) {
    struct A {
    };
    crz::plist l{A{}};
    // 没有重载<<操作符，则输出其对象id。id结构为类型的typeinfo加上对象的地址。
    println(l); // [Z19test_default_outputvE1A0xbf00b0]
    println(l[0].id()); // Z19test_default_outputvE1A0xbf00b0
    // 将l[0]容器内的值窃走
    auto a = std::move(l[0]);
    println(l); // [None]
    println(l[0].id()); // None
}

TEST(type_cast, true) {
    crz::plist l{1, 1.2, "???"};
    println(l); // [1, 1.2, ???]
    // 必须通过显示转换对容器内的值进行直接读写
    // 因为虽然对象行为可以在运行期动态确定（比如<<操作符的行为），但类型信息必须在编译期静态确定
    l[0].cast<int &>() = 2;
    println(l); // [2, 1.2, ???]
    const char *s = l.back().cast<const char *>();
    println(s); // ???
}

TEST(type_check, true) {
    crz::plist pl;
    std::srand(std::time(nullptr));
    for (int i = 0; i < 10; ++i) {
        if (std::rand() % 2) {
            pl.push_back(233);
        } else {
            pl.push_back(std::string("???"));
        }
    }
    for (auto &x: pl) {
        if (x.isa<int>()) {
            println(x.cast<int>());
        } else if (x.type() == typeid(std::string)) {
            println(x.cast<const std::string &>());
        } else {
            throw std::runtime_error("???");
        }
    }
}

TEST(some_exception, true) {
    struct A {
    };
    {
        crz::plist l{A{}, A{}, A{}};
        try {
            l.count(A{}); // A未定义==运算符
        } catch (crz::bad_comparison &e) {
            println(e.what()); // bad comparison: Z19test_some_exceptionvE1A == Z19test_some_exceptionvE1A
        }
    }
    {
        crz::plist l{1, 2, std::string("???")};
        try {
            l.sort(); // 不同类型元素比较
        } catch (crz::bad_comparison &e) {
            println(e.what()); // bad comparison: NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE < i
        }
        try {
            l.sort(true, [](int i) { return i; }); // 会将std::string转换为int
        } catch (crz::bad_pcell_cast &e) {
            println(e.what()); // bad pcell cast: from NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE to i
        }
    }
    {
        crz::plist l(2);
        l[0] = 1;
        try {
            auto s = l[0].cast<std::string>();
            println(s);
        } catch (crz::bad_pcell_cast &e) {
            println(e.what()); // bad pcell cast: from i to NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
        }
        try {
            auto i = l[1].cast<int>();
            println(i);
        } catch (crz::bad_pcell_access &e) {
            println(e.what()); // bad pcell access
        }
    }
}

void run_all_test() {
    for (auto &t: test_list) {
        std::cout << "test: " << t.first << std::endl;
        t.second();
        std::cout << std::endl;
    }
}

int main() {
    run_all_test();
}