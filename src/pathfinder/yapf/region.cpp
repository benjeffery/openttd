#include "region.h"

#include "../../stdafx.h"
#include "../../tile_map.h"
#include "../../openttd.h"
#include "region_d_water.h"
#include "region_common.h"
#include <stack>
using std::stack;

CRegion<RegionDescriptionWater>* region_to_recheck;
TileIndex tile_to_check;

set<TileIndex> show_route_tiles = set<TileIndex>();

void StartTileModification(TileIndex tile)
{
	if (RegionDescriptionWater::updates_active){
		/*Failure of these mean that EndTileModification has not been called on the last change*/
		assert(!region_to_recheck && !tile_to_check);
		
		if (RegionDescriptionWater::GetRegion(tile) == NULL){
			//We don't have a water region, remember tile to check at end of tile mod for adding
			tile_to_check = tile;
		}
		else{
			//We have a water region, remember region to check at end of tile mod
			region_to_recheck = RegionDescriptionWater::GetRegion(tile);
		}
	}
}

void EndTileModification()
{
	if (RegionDescriptionWater::updates_active){
		/*Failure of this assertion indicates we didn't call StartTileModification first*/
		assert(region_to_recheck || tile_to_check);
		
		/*We now go back and check all the tiles and regions that might have been modified
		We recheck regions first*/
		if (region_to_recheck)
			GetWaterRegionManager()->RefindRegion(region_to_recheck, false);
		region_to_recheck = NULL;

		if (tile_to_check)
			GetWaterRegionManager()->AddNewTile(tile_to_check);
		tile_to_check = 0;
		
		if (_debug_yapf_level >= 5)
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

