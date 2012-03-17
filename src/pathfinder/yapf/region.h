#ifndef  REGION_H
#define  REGION_H

#include "../../stdafx.h"
#include "../../tile_map.h"
#include "../../openttd.h"

void StartTileModification(TileIndex tile);
void EndTileModification();

void ActivateWaterRegions();
void DeactivateWaterRegions();

#endif

