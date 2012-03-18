#ifndef  REGION_COMMON_H
#define  REGION_COMMON_H

#include "region_manager.hpp"
#include "region_d_water.h"


static inline CRegionManager<CRegion<RegionDescriptionWater> > *GetWaterRegionManager(){
	return CRegionManager<CRegion<RegionDescriptionWater> >::GetManager();
}

static inline vector<TileIndex> YapfRegionWater(const Ship* v, TileIndex start, TileIndex end, uint regions_ahead, bool *path_not_found)
{
	return CYapfRegionWater::ChooseIntermediateDestinations(v, start, end, regions_ahead, path_not_found);
}

extern set<TileIndex> show_route_tiles;

#endif
