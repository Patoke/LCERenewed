# dumper

This is a python script from https://github.com/NessieHax/msscmp-poc modified to allow converting .binka files to ogg originally it only allowed dumping .msscmp files to oggs.

Run `python parse.py -h` for commands.

# example commands

## mss banks
`python parse.py bank "M:\LCERenewed\Minecraft.Client\Windows64Media\Sound\Minecraft.msscmp" -d mc`

## binka (works for entire directories & sub dirs)

`python parse.py convert "M:\LCERenewed\Minecraft.Client\music"`