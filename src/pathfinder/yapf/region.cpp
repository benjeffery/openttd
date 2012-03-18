#include "region.h"

#include "../../stdafx.h"
#include "../../tile_map.h"
#include "../../openttd.h"
#include "region_d_water.h"
#include "region_common.h"
#include <stack>
using std::stack;

stack<vector<CRegion<RegionDescriptionWater>* > >regions_to_recheck;
stack<vector<TileIndex> >tiles_to_check;

set<TileIndex> show_route_tiles = set<TileIndex>();

void StartTileModification(TileIndex tile)
{
	if (RegionDescriptionWater::updates_active){
		if (RegionDescriptionWater::GetRegion(tile) == NULL){
			//We don't have a water region, remember tile to check at end of tile mod for adding
			tiles_to_check.push(vector<TileIndex>(1,tile));
			regions_to_recheck.push(vector<CRegion<RegionDescriptionWater>* >());
		}
		else{
			//We have a water region, remember region to check at end of tile mod
			tiles_to_check.push(vector<TileIndex>());
			regions_to_recheck.push(vector<CRegion<RegionDescriptionWater>* >(1,RegionDescriptionWater::GetRegion(tile)));
		}
	}
}

void EndTileModification()
{
	if (RegionDescriptionWater::updates_active){
		//We now go back and check all those tiles and regions that might have been modified
		//We recheck regions first!!
		assert(!regions_to_recheck.empty());
		assert(!tiles_to_check.empty());
		for (uint i = 0; i < regions_to_recheck.top().size(); ++i)
			GetWaterRegionManager()->RefindRegion(regions_to_recheck.top()[i],false);
		regions_to_recheck.pop();

		for (uint i = 0; i < tiles_to_check.top().size(); ++i)
			GetWaterRegionManager()->AddNewTile(tiles_to_check.top()[i]);
		tiles_to_check.pop();
		if (_debug_yapf_level >= 3)
			MarkWholeScreenDirty();
	}
}

void ActivateWaterRegions()
{
	RegionDescriptionWater::updates_active = true;
}	
void DeactivateWaterRegions()
{
	RegionDescriptionWater::updates_active = false;
}


bool RegionDescriptionWater::updates_active;

