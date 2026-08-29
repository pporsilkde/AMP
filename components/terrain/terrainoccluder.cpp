#include "terrainoccluder.hpp"
#include "storage.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <osg/Array>

namespace Terrain
{
    TerrainOccluder::TerrainOccluder(Storage* storage, float cellWorldSize)
        : mStorage(storage)
        , mCellWorldSize(cellWorldSize)
    {
    }

    bool TerrainOccluder::hasTerrainData() const
    {
        return mStorage != nullptr;
    }

    void TerrainOccluder::setLodLevel(int lod)
    {
        lod = std::max(0, lod);
        if (mLodLevel == lod)
            return;

        mLodLevel = lod;
        invalidateCache();
    }

    void TerrainOccluder::setCellBuildBudget(int cellsPerBuild)
    {
        mCellBuildBudget = cellsPerBuild;
    }

    void TerrainOccluder::invalidateCache()
    {
        mRegionCacheValid = false;
        mCachedRadius = -1;
        mCellCache.clear();
        mPublishedCachedCellCount.store(0, std::memory_order_relaxed);
        mPublishedLastBuiltCells.store(0, std::memory_order_relaxed);
    }

    TerrainOccluder::CachedCellMesh TerrainOccluder::buildCell(int cellX, int cellY) const
    {
        CachedCellMesh result;

        const osg::Vec2f center(cellX + 0.5f, cellY + 0.5f);
        osg::ref_ptr<osg::Vec3Array> fullRes(new osg::Vec3Array);
        osg::ref_ptr<osg::Vec3Array> normals(new osg::Vec3Array);
        osg::ref_ptr<osg::Vec4ubArray> colors(new osg::Vec4ubArray);
        colors->setNormalize(true);
        mStorage->fillVertexBuffers(0, 1.0f, center, fullRes, normals, colors);
        if (fullRes->empty())
            return result;

        const int fullPerSide = static_cast<int>(std::sqrt(static_cast<float>(fullRes->size())));
        if (fullPerSide < 2)
            return result;

        // Avoid undefined behaviour on a corrupt/external setting while preserving
        // the old semantics for all practical LOD values.
        const int safeLod = std::min(mLodLevel, 15);
        const int step = std::max(1, 1 << safeLod);
        const int coarsePerSide = (fullPerSide - 1) / step + 1;
        if (coarsePerSide < 2)
            return result;

        std::vector<float> quadMins((coarsePerSide - 1) * (coarsePerSide - 1));
        for (int qj = 0; qj < coarsePerSide - 1; ++qj)
        {
            for (int qi = 0; qi < coarsePerSide - 1; ++qi)
            {
                const int startI = qi * step;
                const int startJ = qj * step;
                const int endI = std::min((qi + 1) * step, fullPerSide - 1);
                const int endJ = std::min((qj + 1) * step, fullPerSide - 1);
                float minH = std::numeric_limits<float>::max();
                for (int fj = startJ; fj <= endJ; ++fj)
                    for (int fi = startI; fi <= endI; ++fi)
                        minH = std::min(minH, (*fullRes)[fj * fullPerSide + fi].z());
                quadMins[qj * (coarsePerSide - 1) + qi] = minH;
            }
        }

        const osg::Vec3f worldOffset(center.x() * mCellWorldSize, center.y() * mCellWorldSize, 0.0f);
        result.mPositions.reserve(static_cast<std::size_t>(coarsePerSide * coarsePerSide));
        for (int cj = 0; cj < coarsePerSide; ++cj)
        {
            for (int ci = 0; ci < coarsePerSide; ++ci)
            {
                float minH = std::numeric_limits<float>::max();
                for (int dj = -1; dj <= 0; ++dj)
                    for (int di = -1; di <= 0; ++di)
                    {
                        const int qi = ci + di;
                        const int qj = cj + dj;
                        if (qi >= 0 && qi < coarsePerSide - 1 && qj >= 0 && qj < coarsePerSide - 1)
                            minH = std::min(minH, quadMins[qj * (coarsePerSide - 1) + qi]);
                    }

                const int srcI = std::min(ci * step, fullPerSide - 1);
                const int srcJ = std::min(cj * step, fullPerSide - 1);
                osg::Vec3f pos = (*fullRes)[srcJ * fullPerSide + srcI];
                pos.z() = minH;
                result.mPositions.push_back(pos + worldOffset);
            }
        }

        result.mIndices.reserve(static_cast<std::size_t>((coarsePerSide - 1) * (coarsePerSide - 1) * 6));
        for (int row = 0; row < coarsePerSide - 1; ++row)
        {
            for (int col = 0; col < coarsePerSide - 1; ++col)
            {
                const unsigned int tl = static_cast<unsigned int>(row * coarsePerSide + col);
                const unsigned int tr = tl + 1;
                const unsigned int bl = tl + static_cast<unsigned int>(coarsePerSide);
                const unsigned int br = bl + 1;
                result.mIndices.push_back(tl);
                result.mIndices.push_back(bl);
                result.mIndices.push_back(tr);
                result.mIndices.push_back(tr);
                result.mIndices.push_back(bl);
                result.mIndices.push_back(br);
            }
        }

        return result;
    }

    const TerrainOccluder::CachedCellMesh* TerrainOccluder::getCell(int cellX, int cellY, int& budget)
    {
        const CellKey key(cellX, cellY);
        const std::map<CellKey, CachedCellMesh>::iterator found = mCellCache.find(key);
        if (found != mCellCache.end())
            return &found->second;

        // X029: cells with no LAND record are cached as empty meshes on purpose,
        // so a missing cell costs its decode attempt exactly once. That attempt is
        // still the expensive part, so it counts against the budget.
        if (budget <= 0)
            return nullptr;

        --budget;
        ++mLastBuiltCells;
        return &mCellCache.emplace(key, buildCell(cellX, cellY)).first->second;
    }

    void TerrainOccluder::pruneCellCache(const osg::Vec2i& center, int radiusCells)
    {
        // Keep two extra rings so a normal one-cell movement can reuse the full
        // overlap and only generate the newly exposed edge. This bounds memory
        // while avoiding the old all-or-nothing rebuild.
        const int keepRadius = std::max(1, radiusCells) + 2;
        for (std::map<CellKey, CachedCellMesh>::iterator it = mCellCache.begin(); it != mCellCache.end();)
        {
            const int dx = std::abs(it->first.first - center.x());
            const int dy = std::abs(it->first.second - center.y());
            if (dx > keepRadius || dy > keepRadius)
                it = mCellCache.erase(it);
            else
                ++it;
        }
    }

    bool TerrainOccluder::build(const osg::Vec3f& eyePoint, int radiusCells,
        std::vector<osg::Vec3f>& outPositions, std::vector<unsigned int>& outIndices)
    {
        // Per-frame counter: zero unless this call actually decoded new cells.
        mLastBuiltCells = 0;
        mPublishedLastBuiltCells.store(0, std::memory_order_relaxed);

        if (!hasTerrainData())
        {
            outPositions.clear();
            outIndices.clear();
            mRegionCacheValid = false;
            mPublishedCachedCellCount.store(static_cast<unsigned int>(mCellCache.size()), std::memory_order_relaxed);
            return true;
        }

        radiusCells = std::max(1, radiusCells);
        const int cellX = static_cast<int>(std::floor(eyePoint.x() / mCellWorldSize));
        const int cellY = static_cast<int>(std::floor(eyePoint.y() / mCellWorldSize));
        const osg::Vec2i cellPos(cellX, cellY);

        if (mRegionCacheValid && cellPos == mCachedCellPos && radiusCells == mCachedRadius)
        {
            mPublishedCachedCellCount.store(static_cast<unsigned int>(mCellCache.size()), std::memory_order_relaxed);
            return false;
        }

        outPositions.clear();
        outIndices.clear();

        // X029: budget the expensive part. Cells already in the cache are always
        // assembled; only newly decoded ones are rationed. When the budget runs
        // out the region is marked incomplete, so the next frame continues where
        // this one stopped instead of declaring the region done.
        int budget = mCellBuildBudget > 0 ? mCellBuildBudget : std::numeric_limits<int>::max();
        bool complete = true;

        // X028: region assembly still walks all cells, but expensive LAND decode +
        // coarse-mesh construction happens only once per cached cell. At radius 8
        // a one-cell move reuses 272/289 cells and builds only the new 17-cell edge.
        for (int cy = cellY - radiusCells; cy <= cellY + radiusCells; ++cy)
        {
            for (int cx = cellX - radiusCells; cx <= cellX + radiusCells; ++cx)
            {
                const CachedCellMesh* cell = getCell(cx, cy, budget);
                if (!cell)
                {
                    // Not built yet and out of budget. Leaving it out only removes
                    // an occluder, which can never hide something that is visible.
                    complete = false;
                    continue;
                }

                if (cell->mPositions.empty() || cell->mIndices.empty())
                    continue;

                const unsigned int baseIndex = static_cast<unsigned int>(outPositions.size());
                outPositions.insert(outPositions.end(), cell->mPositions.begin(), cell->mPositions.end());
                outIndices.reserve(outIndices.size() + cell->mIndices.size());
                for (std::vector<unsigned int>::const_iterator it = cell->mIndices.begin(); it != cell->mIndices.end(); ++it)
                    outIndices.push_back(baseIndex + *it);
            }
        }

        mCachedCellPos = cellPos;
        mCachedRadius = radiusCells;
        mRegionCacheValid = complete;

        // X029-safe: prune even while filling. pruneCellCache keeps radius+2,
        // therefore it cannot evict any cell required by the current radius, and
        // this prevents cache growth during rapid teleports where a region may not
        // become complete before the next center is requested.
        pruneCellCache(cellPos, radiusCells);
        mPublishedCachedCellCount.store(static_cast<unsigned int>(mCellCache.size()), std::memory_order_relaxed);
        mPublishedLastBuiltCells.store(mLastBuiltCells, std::memory_order_relaxed);
        return true;
    }
}
