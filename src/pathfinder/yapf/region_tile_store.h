#ifndef REGION_TILE_STORE_H
#define REGION_TILE_STORE_H

#include "../../stdafx.h"
#include "../../tile_map.h"
#include "../../openttd.h"
#include <vector>

using std::vector;

class CRegionTileStore{
public:
	void Init(uint size_x, uint size_y, TileIndex middle);
	bool Has(TileIndex tile) const;
	void Add(TileIndex tile);
	void Remove(TileIndex tile);
	vector<TileIndex> GetTiles() const;
	void Clear();
	uint16 NumberOfTiles() const;
	uint WidthX() const;
	uint HeightY() const;
private:
	void ReserveSize(uint x, uint y);
	void ResizeToContain(TileIndex tile);
	bool Contains(TileIndex tile) const;
	void SetStorage(TileIndex tile,bool value);
	bool GetStorage(TileIndex tile) const;

	uint m_left_x;
	uint m_bottom_y;
	uint16 m_size_x;
	uint16 m_size_y;
	/*First index is row (x)*/
	vector<vector<bool> > m_storage;
};

#endif

