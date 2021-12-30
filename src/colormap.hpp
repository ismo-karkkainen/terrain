//
//  colormap.hpp
//
//  Created by Ismo Kärkkäinen on 9.6.2020.
//  Copyright © 2020 Ismo Kärkkäinen. All rights reserved.
//
// Licensed under Universal Permissive License. See License.txt.

#if !defined(COLORMAP_HPP)
#define COLORMAP_HPP

// Helpers to deal with color map and gettting interpolated values.

#include <vector>


void SortColorMap(std::vector<std::vector<float>>& Map);

std::size_t IndexInMap(float V, const std::vector<std::vector<float>>& Map);

std::vector<float> Interpolated(
    float V, const std::vector<std::vector<float>>& Map);

#endif
