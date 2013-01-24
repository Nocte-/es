
#define BOOST_TEST_MODULE ecs_unittests test
#include <boost/test/unit_test.hpp>

#include <string>

#include <ecs/traits.hpp>
#include <ecs/storage.hpp>

using namespace ecs;

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

namespace ecs {

template<>
struct is_flat<flat_test>
{
    static constexpr bool value = true;
};

}

BOOST_AUTO_TEST_CASE (prerequisites)
{
    BOOST_CHECK(ecs::is_flat<vector>::value);
    BOOST_CHECK(ecs::is_flat<int>::value);
    BOOST_CHECK(!ecs::is_flat<std::string>::value);
    BOOST_CHECK(ecs::is_flat<flat_test>::value);
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

class tester
{
public:
    static int count_constr;
    static int count_destr;

public:
    tester() : data("test")
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
