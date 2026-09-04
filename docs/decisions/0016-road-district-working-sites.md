# 0016 — Add working sites to every road district

**Status:** Accepted

## Context

The goods economy now joins farms, woodlots, mines, quarries, mills,
bakeries, smithies, markets, and carriers. The road book needs to show where
this work happens. Towns must keep their authored storybook maps.

## Decision

Each strategic route is also a road district. Every road district has three
saved anchors: one road house and two working sites. A working site records
its route, home town, name, kind, goods, mile position, side, road length,
condition, and access state.

The current world has twenty-four anchors across eight routes. They include
farms, pasture, woodlots, quarries, a mine, a mill, a bakery, a smithy, road
yards, and road houses. Their input and output goods connect them to the
settlement economy.

The world manifest derives a short side road from each anchor. The side road
uses the same seed and route geometry as the carriage road. It ends at a
stable future location point. Every side road is closed at present. Farms,
woodlots, mills, bakeries, smithies, pastures, and road houses use a fallen
tree. Mines, quarries, and road yards use rocks.

The road book draws a side road when the player knows that part of its main
road. A nearby label gives the site name. The barrier marks the current play
boundary. The location beyond it receives an authored local map in a later
change.

Schema 30 saves the road sites. Schema 29 campaigns receive the same stable
site set during load. The generated side-road geometry remains derived data.

## Consequences

- The kingdom reads as settled land between towns.
- Production chains have named physical sources and workshops.
- Road houses have one shared saved position for travel and the world view.
- New local maps can open one side road at a time.
- Existing town maps and main road geometry keep their current authority.

