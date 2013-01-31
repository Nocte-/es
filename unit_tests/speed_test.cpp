#include <ecs/entity.hpp>
#include <ecs/component.hpp>
#include <ecs/storage.hpp>

using namespace ecs;

struct vec
{
    float x, y;

    void operator += (const vec& a) { x += a.x; y += a.y; }
};

namespace ecs
{
template <> struct is_flat<vec> { static constexpr bool value = true; };
}

int main (void)
{
    storage s;

    auto pos (s.register_component<vec>("position"));
    auto vel (s.register_component<vec>("velocity"));

    for (int i (0); i != 10000; ++i)
    {
        auto j (s.new_entity());
        s.set(j, pos, vec{ 0, 0 });
        s.set(j, vel, vec{ 0.1, 0.1 });
    }

    for (int i (0); i != 10000; ++i)
    {
        s.for_each<vec, vec>(pos, vel, [](storage::iterator, storage::var_ref<vec> p, storage::var_ref<vec> v){
            p += (vec)v;
        });
    }
}

