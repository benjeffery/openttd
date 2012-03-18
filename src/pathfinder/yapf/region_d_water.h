#ifndef  REGION_D_WATER_H
#define  REGION_D_WATER_H

#include "../../stdafx.h"
#include "../../tile_map.h"
#include "../../landscape.h"
#include "../../water_map.h"
#include "../../openttd.h"
#include "../../ship.h"
#include "region.hpp"
#include "yapf_region.hpp"

#include <iostream>

class RegionDescriptionWater
{
public:
	enum{
		MAX_TILES_PER_REGION = 114,
		MIN_REGION_SIZE = 12
	};

	typedef Ship RegionVehicleType;
	
	static bool updates_active;

	static inline bool IsRoutable(TileIndex tile)
	{
		if (tile > MapSize()) return false;

		if (IsTileType(tile, MP_WATER))
			return (GetTileSlope(tile,NULL) == SLOPE_FLAT ||
			GetTileSlope(tile,NULL) == SLOPE_W ||
			GetTileSlope(tile,NULL) == SLOPE_E ||
			GetTileSlope(tile,NULL) == SLOPE_N ||
			GetTileSlope(tile,NULL) == SLOPE_S ||
			GetWaterTileType(tile)==WATER_TILE_LOCK);
		if (IsTileType(tile, MP_STATION))
			return (IsDock(tile) || IsBuoy(tile) || IsOilRig(tile));
		return false;
	}

	static bool IsPassable(TileIndex tile, TileIndex neighbour)
	{
		/*We want to know if we can pass from a tile to its neighbour,
		 note the order of the conditions is important!*/

		 assert(tile < MapSize() && neighbour < MapSize());
		assert(IsRoutable(tile) && IsRoutable(neighbour));

		/*Most often we have two flat water tiles so do a quick check for that first*/
		if (IsTileType(tile, MP_WATER) && IsTileType(neighbour, MP_WATER) && GetTileSlope(tile,NULL) == SLOPE_FLAT && GetTileSlope(neighbour,NULL) == SLOPE_FLAT)
			return true;

		/*We need to check that a ship could pass in the direction between the two tiles, first work out that direction*/
		int diffx = TileX(neighbour)-TileX(tile);
		int diffy = TileY(neighbour)-TileY(tile);
		/*This assertion checks that the tiles are indeed neighbours and not diagonal neighbours*/
		assert((abs(diffx) + abs(diffy)) < 2 && (diffx+diffy != 0));

		/*if one of the tiles is a station its passable, both not, unless at least one is a buoy*/
		if (IsTileType(tile, MP_STATION) && IsTileType(neighbour, MP_STATION)) return (IsBuoy(tile) || IsBuoy(neighbour));
				
		if (IsTileType(tile,MP_STATION)){
			if (GetWaterTileType(neighbour) == WATER_TILE_DEPOT)
				return ((diffx != 0 && GetShipDepotAxis(neighbour)==AXIS_X) || (diffy != 0 && GetShipDepotAxis(neighbour)==AXIS_Y));
			else
				return true;
		}

		if (IsTileType(neighbour, MP_STATION)){
			if (GetWaterTileType(tile) == WATER_TILE_DEPOT)
				return ((diffx != 0 && GetShipDepotAxis(tile)==AXIS_X) || (diffy != 0 && GetShipDepotAxis(tile)==AXIS_Y));
			else
				return true;
		}

		/*By now we should just have water tiles*/
		assert(IsTileType(tile, MP_WATER) && IsTileType(neighbour, MP_WATER));

		/*if we are travelling in the depot axis direction we can pass through depots! (check for water first!)*/
		if (GetWaterTileType(tile) == WATER_TILE_DEPOT || GetWaterTileType(neighbour) == WATER_TILE_DEPOT)
		{
			TileIndex depot_tile = GetWaterTileType(tile) == WATER_TILE_DEPOT ? tile : neighbour;
			return ((diffx != 0 && GetShipDepotAxis(depot_tile)==AXIS_X) || (diffy != 0 && GetShipDepotAxis(depot_tile)==AXIS_Y));
		}

		/*if either is flat or a lock, this is fine*/
		Slope temp;
		Slope t = GetTileSlope(tile,NULL);
		if (SLOPE_FLAT == t || GetWaterTileType(tile)==WATER_TILE_LOCK) return true;
		Slope n = GetTileSlope(neighbour,NULL);
		if (SLOPE_FLAT == n || GetWaterTileType(neighbour)==WATER_TILE_LOCK) return true;

		assert(t == SLOPE_S || t == SLOPE_W || t == SLOPE_E || t == SLOPE_N);
		assert(n == SLOPE_S || n == SLOPE_W || n == SLOPE_E || n == SLOPE_N);
		/*travel in +ve just swaps t and n*/
		if (diffx + diffy > 0) {temp = t;t = n;n = temp;}

		if (0 == diffy){
			/*travel in -X allow s->e,w->n,w->e,s->n*/
			return ((SLOPE_S == t && SLOPE_E == n) ||
				(SLOPE_W == t && SLOPE_N == n) ||
				(SLOPE_W == t && SLOPE_E == n) ||
				(SLOPE_S == t && SLOPE_N == n));
		}
		else{
			/*travel in -Y allow s->w,e->n,e->w,s->n*/
			return ((SLOPE_S == t && SLOPE_W == n) ||
				(SLOPE_E == t && SLOPE_N == n) ||
				(SLOPE_E == t && SLOPE_W == n) ||
				(SLOPE_S == t && SLOPE_N == n));
		}
	}

	static inline void SetRegion(TileIndex tile, CRegion<RegionDescriptionWater> *region)
	{
		if (!(IsTileType(tile, MP_WATER) || IsTileType(tile, MP_STATION))) return;

		uint16 index = 0;
		if (region != NULL)
			index = region->GetIndex();
		if (IsTileType(tile, MP_WATER)) {
			if (GetWaterTileType(tile) == WATER_TILE_DEPOT) {
				/*Depots use m2 so we have to use m3/m4*/
				SB(_m[tile].m3, 0, 8, index);
				SB(_m[tile].m4, 8, 8, index);
			}
			else
				_m[tile].m2 = index;
		}
		else
			Station::GetByTile(tile)->region_index = index;

		assert(GetRegion(tile) == region);
	}

	static inline CRegion<RegionDescriptionWater> *GetRegion(TileIndex tile)
	{
		if (!(IsTileType(tile, MP_WATER) || IsTileType(tile, MP_STATION))) return NULL;

		uint16 index = GetRegionID(tile);
		assert(index == 0 || index < CRegion<RegionDescriptionWater>::m_region_index.size());
		if (index == 0) return NULL;

		return CRegion<RegionDescriptionWater>::m_region_index[index];			
	}

	static inline uint16 GetRegionID(TileIndex tile)
	{
		if (!(IsTileType(tile, MP_WATER) || IsTileType(tile, MP_STATION))) return 0;
		uint16 index;
		if (IsTileType(tile, MP_WATER)) {
			if (GetWaterTileType(tile) == WATER_TILE_DEPOT) {
				/*Depots use m2 so we have to use m3/m4*/
				index = GB(_m[tile].m3, 0, 8) | GB(_m[tile].m4, 8, 8);
			}
			else
				index = _m[tile].m2;
		}
		else
			index = Station::GetByTile(tile)->region_index;
		return index;			
	}
};

typedef CNodeList_HashTableT<CYapfRegionNodeT<CYapfNodeKeyRegion<RegionDescriptionWater> >, 12, 12> CRegionNodeListWater;

struct  CYapfRegionWater : CYapfT<CYapfRegion_TypesT<CYapfRegionWater, CRegionNodeListWater> > {};

#endif

