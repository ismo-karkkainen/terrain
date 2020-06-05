# Check against reference

Set PATH to include the build directory and then run datalackey-make with
suitable parameters. For example:

    PATH=/path/to/build:$PATH datalackey-make tgt -m -f 1 --time -r Reference
    PATH=/path/to/build:$PATH datalackey-make tgt -m -f 1 --time -r Render

After you have run both, you can compare the height fields using:

    ../hfdiff reference.json render.json
