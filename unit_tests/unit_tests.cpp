
#define BOOST_TEST_MODULE ecs_unittests test
#include <boost/test/unit_test.hpp>

#include <string>

#include <ecs/storage.hpp>

using namespace ecs;

struct vector
{
    vector () { }
    vector (float ix, float iy, float iz) : x(ix), y(iy), z(iz) { }

    float x, y, z;
};

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
    s.set(player, pos,    vector(2, 3, 4));

    s.set(bullet, pos,    vector(5, 6, 7));

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

    BOOST_CHECK(s.components()[health].is_pod());
    BOOST_CHECK(s.components()[pos].is_pod());
    BOOST_CHECK(s.components()[name].is_pod());
}
