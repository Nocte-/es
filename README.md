Entity-Component Framework
==========================

A fast and compact [entity-component framework](http://t-machine.org/index.php/2007/09/03/entity-systems-are-the-future-of-mmog-development-part-1/) in C++.

Example
-------

```c++

#include <ecs/entity.hpp>
#include <ecs/component.hpp>
#include <ecs/storage.hpp>

using namespace ecs;

main()
{
    storage s;

    // Register some components.
    auto health   (s.register_component<float>("health"));
    auto pos      (s.register_component<hexa::vector>("position"));
    auto name     (s.register_component<std::string>("name"));

    // Create some new, empty entities.
    entity player (s.new_entity());
    entity bullet (s.new_entity());

    // Assign some data to the entities.
    s.set(player, health, 20.0f);
    s.set(player, name,   std::string("Timmy"));
    s.set(player, pos,    hexa::vector(2, 5, 7));

    s.set(bullet, pos,    hexa::vector(4, 8, -1));

    // Retrieve the data.
    std::cout << "Player: " << s.get<std::string>(player, name) << std::endl;
}
```

