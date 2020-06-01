# Terrain

Various tools for generating random terrains using datalackey-tools.

All inputs and outputs are JSON objects with indicated keys. Each object must
be in a single line, as the programs read input one line at a time and produce
output for each line before reading the next line.

# Usage

## generatechanges

Program generates and array of coordinates and change radius and offset
magnitude pairs. Coordinates are in [0, 1] range.

Radius is in [0, 1] range and represents a radius of a circle in plane. Values
can be modified using radius_min, radius_range, and radius_max.

Offset is in range [-1, 1] and represents change in height. Values can be
modified using offset_min, offset_range, and offset_max.

Limiting radius allows you to generate rough terrain by using small values for
radius. Smooth terrain can be generated using large radiuses.

The last row and column of the 2-dimensional arrays are used for edges. If you
plan to render the changes as wrap-around map, the first row should match the
last row and the first column should match the last column or you risk having
a visible discontinuity. Due to random nature of the process as long as the
first and last row/column are close enough the output should be ok.

The array minimum size is 1x1. Row sizes can vary row to row.
Each array is treated independently so they can be of different size.
Bi-linear interpolation is used to obtain the value.

Output:
- changes: Array of number arrays containing x, y, radius, and offset.

```
---
generate_io:
  namespace: io
  types:
    GenerateIn:
      count:
        description: Change count.
        format: UInt32
      radius_min:
        description: |
          Map of minimum radius values. Minimum size is 2x2 and inner vectors
          are rows, as with other maps. Default is [[0,0],[0,0]]
        format: [ ContainerStdVector, StdVector, Float ]
        required: false
      radius_max:
        description: Map of maximum radius values. Default is [[1,1],[1,1]].
        format: [ ContainerStdVector, StdVector, Float ]
        required: false
      radius_range:
        description: |
          Map of radius value range. Ignored if both radius_min and radius_max
          have been given. Default is [[1,1],[1,1]].
        format: [ ContainerStdVector, StdVector, Float ]
        required: false
      offset_min:
        description: Map of minimum offset values. Default is [[-1,-1],[-1,-1]].
        format: [ ContainerStdVector, StdVector, Float ]
        required: false
      offset_max:
        description: Map of maximum offset values. Default is [[1,1],[1,1]].
        format: [ ContainerStdVector, StdVector, Float ]
        required: false
      offset_range:
        description: |
          Map of offset value ranges. Ignored if both offset_min and offset_max
          have been given. Default is [[2,2],[2,2]].
        format: [ ContainerStdVector, StdVector, Float ]
        required: false
      seed:
        description: Seed for random number generator.
        format: UInt32
        required: false
  generate:
    GenerateIn:
      parser: true
...
```

## renderchanges

Outputs height field as array of rows of height values in JSON object under
key "heightfield". Renders the changes to a square height field.

```
---
render_io:
  namespace: io
  types:
    RenderChangesIn:
      size:
        description: Side length of the height field.
        format: UInt32
      changes:
        description: Array of arrays of x, y, radius, and offset.
        format: [ ContainerStdVector, StdVector, Float ]
      left:
        description: Crop area low x-index, included. Defaults to 0.
        format: UInt32
        required: false
      right:
        description: Crop area high x-index, not included. Defaults to size.
        format: UInt32
        required: false
      low:
        description: Crop area low y-index, included. Defaults to 0.
        format: UInt32
        required: false
      high:
        description: Crop area high y-index, not included. Defaults to size.
        format: UInt32
        required: false
  generate:
    RenderChangesIn:
      parser: true
...
```

# Building

For unit tests, you need https://github.com/onqtam/doctest to compile them.
Install into location for which `#include <doctest/doctest.h>` works.

You need edicta to extract the input and output specifications from this file.
You need specificjson to generate source code from the specifications.

    https://github.com/ismo-karkkainen/edicta
    https://github.com/ismo-karkkainen/specificjson

You need cmake and a C++ compiler that supports 2017 standard. Assuming a build
directory parallel to the terrain directory, you can use:

    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=DEBUG ../terrain
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RELEASE ../terrain
    cmake -G Xcode

To specify the compiler, set for example:

    CXX=clang++
    CXX=g++

To build, assuming Unix Makefiles, run:

    make
    make test
    sudo make install

To run unit tests and to see the output you can "make unittest" and then run
the resulting executable.

# License

Copyright (C) 2020 Ismo Kärkkäinen

Licensed under Universal Permissive License. See License.txt.
