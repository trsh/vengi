/**
 * @file
 */

#pragma once

#include "core/Common.h"
#include "core/Trace.h"
#include "voxel/RawVolume.h"

namespace voxelutil {

enum class VisitorOrder { XYZ, ZYX, ZXY, XZY, YXZ, YZX, mYZX, YZmX };

/**
 * @brief Will skip air voxels on volume
 */
struct SkipEmpty {
	inline bool operator()(const voxel::Voxel &voxel) const {
		return !isAir(voxel.getMaterial());
	}
};

struct VisitAll {
	inline bool operator()(const voxel::Voxel &voxel) const {
		return true;
	}
};

template <class Volume, class Visitor, typename Condition = SkipEmpty>
int visitVolume(const Volume &volume, const voxel::Region &region, int xOff, int yOff, int zOff, Visitor &&visitor,
				Condition condition = Condition(), VisitorOrder order = VisitorOrder::ZYX) {
	core_trace_scoped(VisitVolume);
	int cnt = 0;

#define LOOP                                                                                                           \
	{                                                                                                                  \
		const voxel::Voxel &voxel = volume.voxel(x, y, z);                                                             \
		if (!condition(voxel)) {                                                                                       \
			continue;                                                                                                  \
		}                                                                                                              \
		visitor(x, y, z, voxel);                                                                                       \
		++cnt;                                                                                                         \
	}

	switch (order) {
	case VisitorOrder::XYZ:
		for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x += xOff)
			for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
				for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
					LOOP
		break;
	case VisitorOrder::ZYX:
		for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
			for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
				for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x += xOff)
					LOOP
		break;
	case VisitorOrder::ZXY:
		for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
			for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x += xOff)
				for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
					LOOP
		break;
	case VisitorOrder::XZY:
		for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x += xOff)
			for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
				for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
					LOOP
		break;
	case VisitorOrder::YXZ:
		for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
			for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x += xOff)
				for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
					LOOP
		break;
	case VisitorOrder::YZX:
		for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
			for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
				for (int32_t x = region.getLowerX(); x <= region.getUpperX(); x += xOff)
					LOOP
		break;
	case VisitorOrder::YZmX:
		for (int32_t y = region.getLowerY(); y <= region.getUpperY(); y += yOff)
			for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
				for (int32_t x = region.getUpperX(); x >= region.getLowerX(); x -= xOff)
					LOOP
		break;
	case VisitorOrder::mYZX:
		for (int32_t y = region.getUpperY(); y <= region.getLowerY(); y += yOff)
			for (int32_t z = region.getLowerZ(); z <= region.getUpperZ(); z += zOff)
				for (int32_t x = region.getLowerX(); x >= region.getUpperX(); x -= xOff)
					LOOP
		break;
	}

#undef LOOP
	return cnt;
}

template <class Volume, class Visitor, typename Condition = SkipEmpty>
int visitVolume(const Volume &volume, int xOff, int yOff, int zOff, Visitor &&visitor, Condition condition = Condition(), VisitorOrder order = VisitorOrder::ZYX) {
	const voxel::Region &region = volume.region();
	return visitVolume(volume, region, xOff, yOff, zOff, visitor, condition, order);
}

template <class Volume, class Visitor, typename Condition = SkipEmpty>
int visitVolume(const Volume &volume, Visitor &&visitor, Condition condition = Condition(), VisitorOrder order = VisitorOrder::ZYX) {
	return visitVolume(volume, 1, 1, 1, visitor, condition, order);
}

template <class Volume, class Visitor, typename Condition = SkipEmpty>
int visitVolume(const Volume &volume, const voxel::Region &region, Visitor &&visitor, Condition condition = Condition(), VisitorOrder order = VisitorOrder::ZYX) {
	return visitVolume(volume, region, 1, 1, 1, visitor, condition, order);
}

} // namespace voxelutil
