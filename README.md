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
      radius_histogram:
        description: |
          Distribution of radius values within the radius range as histogram.
          See description of offset_histogram.
        format: [ StdVector, Float ]
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
      offset_histogram:
        description: |
          Distribution of offset values within the offset range as histogram.
          Values are divided by the sum to get relative amount of values that
          end up in each bin. Defaults to uniform distribution: [1]. To only
          get values near offset minimum or maximum, use [1,0,0,...,0,1].
          To exclude values near zero, use value like [1,1,1,0,1,1,1]. Using
          zero at the end effective shrinks range.
        format: [ StdVector, Float ]
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
        format: [ ContainerStdVector, StdVector, Double ]
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

## examples/simplecolormap

Takes color map name and outputs an array containing arrays of threshold
followed by 1 or 3 color component values depending on map name.

Run the script without arguments to display brief help.

## heightfield2model

Takes a height field and produces an array of XYZ vertices and triangle strips
as indexes to the array.

```
---
heightfield2model_io:
  namespace: io
  types:
    HeightField2ModelIn:
      heightfield:
        description: Input height field.
        format: [ ContainerStdVectorEqSize, StdVector, Float ]
      width:
        description: |
          Length in units of the StdVector for coordinates, in [0.0, width].
          Defaults to width of the heightfield - 1.
        format: Float
        required: false
      range:
        description: Height range length. Defaults to same as height field.
        format: Float
        required: false
      colormap:
        description: |
          Array of arrays of relative value in [0, 1] range and the color-value
          to use. If not given, colors are not produced.
        format: [ ContainerStdVectorEqSize, StdVector, Float ]
        required: false
    HeightField2ModelOut:
      vertices:
        description: Array of vertices.
        format: [ ContainerStdVector, StdVector, Float ]
        accessor: vertices
      colors:
        description: Array of RGB colors.
        format: [ ContainerStdVector, StdVector, Float ]
        accessor: colors
        checker: "colors.size() != 0"
      tristrips:
        description: Array of arrays of triangle strip indexes.
        format: [ ContainerStdVector, StdVector, UInt32 ]
        accessor: tristrips
  generate:
    HeightField2ModelIn:
      parser: true
    HeightField2ModelOut:
      writer: true
...
```

## heightfield2color

Takes a height field and produces a matching image with height value mapped to
given value vector.

```
---
heightfield2color_io:
  namespace: io
  types:
    HeightField2ColorIn:
      heightfield:
        description: Input height field. Any array of arrays of floats will do.
        format: [ ContainerStdVector, StdVector, Float ]
      colormap:
        description: |
          Array of arrays of relative value in [0, 1] range and the color-value
          to use to replace height values with.
        format: [ ContainerStdVectorEqSize, StdVector, Float ]
    HeightField2ColorOut:
      image:
        description: |
          Image with inner-most vector having as many components as the
          colormap with threshold omitted.
        format: [ ContainerStdVector, ContainerStdVector, StdVector, Float ]
        accessor: image
  generate:
    HeightField2ColorIn:
      parser: true
    HeightField2ColorOut:
      writer: true
...
```

## heightfield2texture

Takes a height field and produces matching texture coordinates and a texture
given value vector.

```
---
heightfield2texture_io:
  namespace: io
  types:
    HeightField2TextureIn:
      heightfield:
        description: Input height field. Any array of arrays of floats will do.
        format: [ ContainerStdVector, StdVector, Float ]
      colormap:
        description: |
          Array of arrays of relative value in [0, 1] range and the color-value
          to use to replace height values with.
        format: [ ContainerStdVectorEqSize, StdVector, Float ]
    HeightField2TextureOut:
      texture:
        description: An image that represents the colormap.
        format: [ ContainerStdVector, ContainerStdVector, StdVector, Float ]
        accessor: texture
      coordinates:
        description: Texture coordinates for the height values.
        format: [ ContainerStdVector, StdVector, Float ]
        accessor: coordinates
  generate:
    HeightField2TextureIn:
      parser: true
    HeightField2TextureOut:
      writer: true
...
```

# Examples

Under directory examples, there are subdirectories. You need to have installed

    https://github.com/ismo-karkkainen/datalackey
    https://github.com/ismo-karkkainen/datalackeyshell
    https://github.com/ismo-karkkainen/fileio

In the subdirectories, the example can be run by running the run.sh script.

If you want to see how the execution proceeds, arguments to the script are
passed to datalackey-make, so for example "--follow 3" will show you the rule
names and commands that are executed.

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
