#define CC_UR_NETWORK_NO_MAIN
#include "test_support.h"

#include <string.h>

#include "../tools/underroad_network.c"

int main(void)
{
    UrNetGraph a;
    UrNetGraph b;
    UrNetGraph c;
    UrNetBuild(&a, 7);
    UrNetBuild(&b, 7);
    UrNetBuild(&c, 8);

    CC_CHECK(a.generated);
    CC_CHECK(a.node_count == 10); /* 6 settlements + 3 lairs + 1 hoard */
    CC_CHECK(a.road_count >= 9);  /* a spanning tree over 10 nodes */
    CC_CHECK(a.road_count <= UR_NET_MAX_ROADS);

    CC_CHECK(UrNetConnected(&a) == 1);
    CC_CHECK(UrNetHash(&a) == UrNetHash(&b));
    CC_CHECK(UrNetHash(&a) != UrNetHash(&c));

    for (int32_t i = 0; i < a.node_count; i++) {
        int32_t degree = 0;
        for (int32_t j = 0; j < a.road_count; j++) {
            if (a.roads[j].from_node == i || a.roads[j].to_node == i) {
                degree++;
            }
        }
        CC_CHECK(degree >= 1);
    }

    for (int32_t j = 0; j < a.road_count; j++) {
        CC_CHECK(a.roads[j].from_node >= 0 && a.roads[j].from_node < a.node_count);
        CC_CHECK(a.roads[j].to_node >= 0 && a.roads[j].to_node < a.node_count);
        CC_CHECK(a.roads[j].from_node != a.roads[j].to_node);
        CC_CHECK(a.roads[j].depth >= 0 && a.roads[j].depth < UR_NET_LAYERS);
        CC_CHECK(a.roads[j].condition >= 0);
        CC_CHECK(a.roads[j].clearance >= 1);
    }

    UrNetCrawl crawl;
    UrNetCrawl crawl_again;
    UrNetBuildCrawl(&a, 0, &crawl);
    UrNetBuildCrawl(&a, 0, &crawl_again);
    CC_CHECK(crawl.length > 0);
    CC_CHECK(crawl.length <= UR_NET_CRAWL_MAX_LEN);
    CC_CHECK(crawl.width == a.roads[0].clearance + 1);
    CC_CHECK(UrNetCrawlReachable(&crawl));
    CC_CHECK(memcmp(&crawl, &crawl_again, sizeof crawl) == 0);
    for (int32_t x = 0; x < crawl.length; x++) {
        CC_CHECK(crawl.tiles[x][crawl.py] == UR_NET_TILE_FLOOR);
    }
    UrNetCrawl missing;
    UrNetBuildCrawl(&a, a.road_count + 1, &missing);
    CC_CHECK(missing.length == 0);

    printf("underroad network: ok\n");
    return 0;
}
