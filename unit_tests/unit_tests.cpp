
#define BOOST_TEST_MODULE es_unittests test
#include <boost/test/unit_test.hpp>

#include <string>

#include "../es/traits.hpp"
#include "../es/storage.hpp"

using namespace es;

struct vector
{
    float x, y, z;
};

struct flat_test : public vector
{
    flat_test (float v) : a(v), b(v) { }
    ~flat_test() { }

    float a, b;
};

namespace es {

template<>
struct is_flat<flat_test>
{
    static constexpr bool value = true;
};

}

BOOST_AUTO_TEST_CASE (prerequisites)
{
    BOOST_CHECK(es::is_flat<vector>::value);
    BOOST_CHECK(es::is_flat<int>::value);
    BOOST_CHECK(!es::is_flat<std::string>::value);
    BOOST_CHECK(es::is_flat<flat_test>::value);
}

BOOST_AUTO_TEST_CASE (basic_test)
{
    storage s;

    auto health (s.register_component<float>("health"));
    auto pos    (s.register_component<vector>("position"));
    auto name   (s.register_component<std::string>("name"));

    BOOST_CHECK_EQUAL(health, 0);
    BOOST_CHECK_EQUAL(pos, 1);
    BOOST_CHECK_EQUAL(name, 2);

    entity player (s.new_entity());
    entity bullet (s.new_entity());
    entity deity  (s.new_entity());

    s.set(player, health, 20.0f);
    s.set(player, name,   std::string("Timmy"));
    s.set(player, pos,    vector{2, 3, 4});

    s.set(bullet, pos,    vector{5, 6, 7});

    s.set(deity, name,    std::string("FSM"));

    BOOST_CHECK_EQUAL(s.get<float>(player, health), 20.0f);
    BOOST_CHECK_EQUAL(s.get<std::string>(deity, name), "FSM");
}

BOOST_AUTO_TEST_CASE (pod_test)
{
    storage s;

    auto health (s.register_component<float>("health"));
    auto pos    (s.register_component<vector>("position"));
    auto name   (s.register_component<std::string>("name"));

    BOOST_CHECK(s.components()[health].is_flat());
    BOOST_CHECK(s.components()[pos].is_flat());
    BOOST_CHECK(!s.components()[name].is_flat());
}

BOOST_AUTO_TEST_CASE (delete_test)
{
    storage s;

    auto name   (s.register_component<std::string>("name"));

    entity player (s.new_entity());
    s.set(player, name,   std::string("Timmy"));
    BOOST_CHECK_EQUAL(s.size(), 1);
    s.delete_entity(player);
    BOOST_CHECK_EQUAL(s.size(), 0);
}

BOOST_AUTO_TEST_CASE (shuffle_test)
{
    // Almost the same as delete_test, but involves cleaning up a string
    // after it was moved to a different location.

    storage s;

    auto health (s.register_component<float>("health"));
    auto name   (s.register_component<std::string>("name"));

    entity player (s.new_entity());
    s.set(player, name,   std::string("Timmy"));
    s.set(player, health, 10.0f);
    BOOST_CHECK_EQUAL(s.size(), 1);
    s.delete_entity(player);
    BOOST_CHECK_EQUAL(s.size(), 0);
}

class tester
{
public:
    static int count_constr;
    static int count_destr;

public:
    tester() : data("test")
        { ++count_constr; }

    tester(const tester& copy) : data(copy.data)
        { ++count_constr; }

    tester(tester&& move) : data(std::move(move.data))
        { --count_destr; }

    ~tester()
        { ++count_destr; }

private:
    std::string data;
};

int tester::count_constr = 0;
int tester::count_destr = 0;

BOOST_AUTO_TEST_CASE (structor_test)
{
    {
    storage s;

    auto ctest  (s.register_component<tester>("tester"));

    BOOST_CHECK_EQUAL(tester::count_constr, 1);
    BOOST_CHECK_EQUAL(tester::count_destr, 0);

    entity foo (s.new_entity());

    s.set(foo, ctest, tester());

    BOOST_CHECK_EQUAL(tester::count_constr, 2);
    BOOST_CHECK_EQUAL(tester::count_destr, 0);

    s.delete_entity(foo);

    BOOST_CHECK_EQUAL(tester::count_constr, 2);
    BOOST_CHECK_EQUAL(tester::count_destr, 1);
    }
    // 's' goes out of scope here
    BOOST_CHECK_EQUAL(tester::count_constr, 2);
    BOOST_CHECK_EQUAL(tester::count_destr, 2);
}

BOOST_AUTO_TEST_CASE (system_test_1)
{
    storage s;

    auto health (s.register_component<int>("health"));
    auto pos    (s.register_component<vector>("position"));

    s.new_entities(4);

    s.set(0, health, 10);
    s.set(1, health, 20);
    s.set(1, pos, vector{1, 2, 3});
    s.set(2, pos, vector{2, 4, 8});
    s.set(3, pos, vector{5, 12, 23});

    s.for_each<int>(health, [](storage::iterator i, storage::var_ref<int> var)
        {
            var += 3;
        });

    BOOST_CHECK_EQUAL(s.get<int>(0, health), 13);
    BOOST_CHECK_EQUAL(s.get<int>(1, health), 23);

    s.for_each<vector>(pos, [](storage::iterator i, storage::var_ref<vector> var)
        {
            vector p (var);
            p.x += 1;
            var = p;
        });

    BOOST_CHECK_EQUAL(s.get<vector>(1, pos).x, 2);
    BOOST_CHECK_EQUAL(s.get<vector>(2, pos).x, 3);
}


