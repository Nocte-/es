
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

namespace es
{

template<>
void serialize<std::string>(const std::string& s, std::vector<char>& buf)
{
    uint16_t size (s.size());
    buf.push_back(size & 0xff);
    buf.push_back(size >> 8);
    buf.insert(buf.end(), s.begin(), s.end());
}

template<>
std::vector<char>::const_iterator
deserialize<std::string>(std::string& obj, std::vector<char>::const_iterator first, std::vector<char>::const_iterator last)
{
    if (std::distance(first, last) < 2)
        throw  std::runtime_error("cannot deserialize string: no length field");

    uint16_t size (uint8_t(*first++));
    size += (uint16_t(uint8_t(*first++)) << 8 );

    if (std::distance(first, last) < size)
        throw  std::runtime_error("cannot deserialize string: not enough data");

    last = first + size;
    obj.assign(first, last);
    return last;
}

}

//------------------------------------------------------------------------

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

BOOST_AUTO_TEST_CASE (make_test)
{
    storage s;

    BOOST_CHECK_EQUAL(s.size(), 0);
    s.make(0);
    BOOST_CHECK_EQUAL(s.size(), 1);
    s.make(2);
    BOOST_CHECK_EQUAL(s.size(), 2);
    s.make(2);
    BOOST_CHECK_EQUAL(s.size(), 2);
    s.make(1);
    BOOST_CHECK_EQUAL(s.size(), 3);
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

BOOST_AUTO_TEST_CASE (many_test)
{
    storage s;

    std::vector<storage::component_id> ci;
    for (int i (0); i < 32; ++i)
    {
        ci.push_back(s.register_component<uint16_t>(std::to_string(i*2)));
        ci.push_back(s.register_component<uint32_t>(std::to_string(i*2 + 1)));
    }

    entity e1 (s.new_entity());
    entity e2 (s.new_entity());
    entity e3 (s.new_entity());
    entity e4 (s.new_entity());

    s.set(e1, ci[0], uint16_t(1));
    s.set(e1, ci[63], uint32_t(2));

    s.set(e2, ci[33], uint32_t(3));

    s.set(e3, ci[60], uint16_t(4));
    s.set(e3, ci[1], uint32_t(5));

    for (int j (0); j < 32; ++j)
    {
        s.set(e4, ci[j*2  ], uint16_t(10+j*2));
        s.set(e4, ci[j*2+1], uint32_t(11+j*2));
    }

    BOOST_CHECK_EQUAL(s.get<uint16_t>(e1, ci[ 0]), 1);
    BOOST_CHECK_EQUAL(s.get<uint32_t>(e1, ci[63]), 2);
    BOOST_CHECK_EQUAL(s.get<uint32_t>(e2, ci[33]), 3);
    BOOST_CHECK_EQUAL(s.get<uint16_t>(e3, ci[60]), 4);
    BOOST_CHECK_EQUAL(s.get<uint32_t>(e3, ci[ 1]), 5);
    BOOST_CHECK_EQUAL(s.get<uint16_t>(e4, ci[60]), 70);
    BOOST_CHECK_EQUAL(s.get<uint32_t>(e4, ci[51]), 61);
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

    s.for_each<int>(health, [](storage::iterator i, int& var)
        {
            var += 3;
            return true;
        });

    BOOST_CHECK_EQUAL(s.get<int>(0, health), 13);
    BOOST_CHECK_EQUAL(s.get<int>(1, health), 23);

    s.for_each<vector>(pos, [](storage::iterator i, vector& var)
        {
            var.x += 1;
            return true;
        });

    BOOST_CHECK_EQUAL(s.get<vector>(1, pos).x, 2);
    BOOST_CHECK_EQUAL(s.get<vector>(2, pos).x, 3);
}

BOOST_AUTO_TEST_CASE (serialization_test)
{
    storage s;

    auto health (s.register_component<int>("health"));
    auto name   (s.register_component<std::string>("name"));
    auto pos    (s.register_component<vector>("position"));

    s.new_entities(3);

    s.set(0, health, 10);
    s.set(1, health, 20);
    s.set(1, pos, vector{1.f, 2.f, 3.f});
    s.set(2, health, 30);
    s.set(2, pos, vector{2.f, 5.f, 9.f});
    s.set(2, name, std::string("abcdefg"));

    std::vector<char> buf1, buf2, buf3;

    s.serialize(s.find(0), buf1);
    BOOST_CHECK_EQUAL(buf1.size(), 8 + sizeof(int));
    //std::vector<char> expected1 {{1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0}};
    //BOOST_CHECK_EQUAL(buf1, expected1);

    s.serialize(s.find(1), buf2);
    BOOST_CHECK_EQUAL(buf2.size(), 8 + sizeof(int) + sizeof(vector));

    s.serialize(s.find(2), buf3);
    BOOST_CHECK_EQUAL(buf3.size(), 8 + sizeof(int) + sizeof(vector) + 9);

    auto check1 (s.new_entity());
    auto c1i (s.find(check1));
    s.deserialize(c1i, buf1);
    BOOST_CHECK(s.entity_has_component(c1i, health));
    BOOST_CHECK(!s.entity_has_component(c1i, pos));
    BOOST_CHECK(!s.entity_has_component(c1i, name));
    BOOST_CHECK_EQUAL(s.get<int>(c1i, health), 10);

    auto check2 (s.new_entity());
    auto c2i (s.find(check2));
    s.deserialize(c2i, buf2);
    BOOST_CHECK(s.entity_has_component(c2i, health));
    BOOST_CHECK(s.entity_has_component(c2i, pos));
    BOOST_CHECK(!s.entity_has_component(c2i, name));
    BOOST_CHECK_EQUAL(s.get<int>(c2i, health), 20);
    BOOST_CHECK_EQUAL(s.get<vector>(c2i, pos).x, 1.f);

    auto check3 (s.new_entity());
    auto c3i (s.find(check3));
    s.deserialize(c3i, buf3);
    BOOST_CHECK(s.entity_has_component(c3i, health));
    BOOST_CHECK(s.entity_has_component(c3i, pos));
    BOOST_CHECK(s.entity_has_component(c3i, name));
    BOOST_CHECK_EQUAL(s.get<int>(c3i, health), 30);
    BOOST_CHECK_EQUAL(s.get<vector>(c3i, pos).z, 9.f);
    BOOST_CHECK_EQUAL(s.get<std::string>(c3i, name), std::string("abcdefg"));
}
