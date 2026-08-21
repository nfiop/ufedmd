# Development

## General idea

The idea is to support algorithms like Reed-solomon, Hamming codes like
Hamming (7,4) and BCH (its most popular variants - BCH4,BCH8, BCH16).

I also want to add support for LDPC, but that algorithm is CPU-intensive
and heavy on computational power, so I need to see how it will fit in.

Last but not least, I also want to support codecs like XORing (according
to the Linux kernel, some flash chips must be written with XORed data to
avoid data corruption) and byte swaps, to aid better data retention and
experimentation on different layouts on some flash chips.

## Pipelines, codecs and YAML

The current plan is likewise - a `ufedmd` is spawned with a command like:
```sh
./ufedmd -f mtd-process-cfg.yaml /dev/mtd0
```

A YAML configuration file consists of `codecs`, `pipelines` and `partitions`
sections.

## Basic flow

The `ufedmd` opens the MTD, and starts parsing the YAML file -
It starts with finding all codecs, putting each on a global list
with its properties, with a temporary configuration object.
It generates temporary configuration objects for the pipelines and lastly for
partitions that will use the pipelines.

It then parses the properties of each codec and checks that each codec is set correctly.

After that, it tries to initialize the pipelines. Each pipeline is assigned
an index starting at 1. Index 0 is reserved for the automatic NACK pipeline.
The max index is 0xff so an index number occupies a `u16`.

The reasoning for using a `u16` is that modern TLC/QLC NAND flash chips might
have more than 256 pages per eraseblocks. For such cases, while not very recommended -
a user can write their configuration, adding specific partitions and specifying specific
page offsets to be handled with specific pipelines.

When evaluating the pipelines' section, 3 scenarios are possible:
- There's one pipeline for all erase blocks and pages in the MTD
- There are multiple pipelines, which might be needed for multiple erase blocks.
- There are multiple pipelines, which might be needed for multiple pages in multiple erase blocks.

