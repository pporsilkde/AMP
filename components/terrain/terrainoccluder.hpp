#ifndef OPENMW_COMPONENTS_TERRAIN_TERRAINOCCLUDER_H
#define OPENMW_COMPONENTS_TERRAIN_TERRAINOCCLUDER_H

#include <atomic>
#include <map>
#include <utility>
#include <vector>

#include <osg/Vec2i>
#include <osg/Vec3f>

namespace Terrain
{
    class Storage;

    class TerrainOccluder
    {
    public:
        TerrainOccluder(Storage* storage, float cellWorldSize);

        // X028: changing the terrain occlusion LOD invalidates both the assembled
        // region and the per-cell cache. This keeps runtime setting changes safe.
        void setLodLevel(int lod);

        // X029: upper bound on how many *new* cells a single build() may decode.
        // The first entry into an exterior needs (2r+1)^2 cells - 289 at the
        // default radius 8 - and decoding all of them in one cull frame is the
        // largest remaining occlusion stutter. Spreading them over frames is
        // fail-open: a partially assembled occluder hides less, never more.
        void setCellBuildBudget(int cellsPerBuild);

        // X029: profiler readout. The engine already collected occlusion counters
        // that nothing ever read; these follow the same idea for the terrain side.
        unsigned int getCachedCellCount() const { return mPublishedCachedCellCount.load(std::memory_order_relaxed); }
        unsigned int getLastBuiltCellCount() const { return mPublishedLastBuiltCells.load(std::memory_order_relaxed); }
        bool isRegionComplete() const { return mRegionCacheValid; }

        // Rebuilds the output only when the eye crosses into a different terrain
        // cell or the requested radius changes. Returns true when output changed.
        bool build(const osg::Vec3f& eyePoint, int radiusCells, std::vector<osg::Vec3f>& outPositions,
            std::vector<unsigned int>& outIndices);
        bool hasTerrainData() const;

    private:
        struct CachedCellMesh
        {
            std::vector<osg::Vec3f> mPositions;
            std::vector<unsigned int> mIndices;
        };

        typedef std::pair<int, int> CellKey;

        CachedCellMesh buildCell(int cellX, int cellY) const;
        // Returns nullptr when the cell is not cached and the per-build budget is
        // already spent. Callers must treat that as "skip this cell for now".
        const CachedCellMesh* getCell(int cellX, int cellY, int& budget);
        void pruneCellCache(const osg::Vec2i& center, int radiusCells);
        void invalidateCache();

        Storage* mStorage;
        float mCellWorldSize;
        int mLodLevel = 3;

        bool mRegionCacheValid = false;
        osg::Vec2i mCachedCellPos;
        int mCachedRadius = -1;

        // X029: <= 0 means unlimited, which reproduces the X028 behaviour exactly.
        int mCellBuildBudget = 24;
        unsigned int mLastBuiltCells = 0;
        std::atomic<unsigned int> mPublishedCachedCellCount{0};
        std::atomic<unsigned int> mPublishedLastBuiltCells{0};

        // X028/MGE-inspired incremental terrain occluder cache. MGE XE avoids
        // regenerating unchanged distant-land units; here we apply the same idea
        // to OpenMW's software occlusion mesh without importing renderer code.
        std::map<CellKey, CachedCellMesh> mCellCache;
    };
}

#endif
