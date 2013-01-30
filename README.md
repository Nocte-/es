Entity-Component Framework
==========================

A fast and compact [entity-component framework](http://t-machine.org/index.php/2007/09/03/entity-systems-are-the-future-of-mmog-development-part-1/)
in C++.  Its main features are:

* Really, really fast.
* Tries hard to conserve memory.
* Components can be defined at runtime.
* Plays nice with existing code: multiple components can have the same type, and there's no need to inherit them from a base class.

Example
-------

```c++

#include <ecs/entity.hpp>
#include <ecs/component.hpp>
#include <ecs/storage.hpp>


main()
{
    using namespace ecs;
    typedef hexa::vector3<float> vec;

    storage s;

    // Register some components.
    auto health   (s.register_component<float>("health"));
    auto pos      (s.register_component<vec>("position"));
    auto name     (s.register_component<std::string>("name"));

    // Create some new, empty entities.
    entity player (s.new_entity());
    entity bullet (s.new_entity());

    // Assign some data to the entities.
    s.set(player, health, 20.0f);
    s.set(player, name,   std::string("Timmy"));
    s.set(player, pos,    vec(2, 5, 7));

    s.set(bullet, pos,    vec(4, 8, -1));

    // Retrieve the data.
    std::cout << "Player: " << s.get<std::string>(player, name) << std::endl;

    // Build a "System" that moves everything with a position diagonally.
    s.for_each<vec>(pos, [](storage::iterator, storage::var_ref<vec> p)
    {
        p += vec(1, 1, 1);
    });
}
```

License
-------
This library is released under the terms and conditions of the MIT License.
See the file 'LICENSE' for more information.

