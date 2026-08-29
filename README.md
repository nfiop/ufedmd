# ufedmd - A userspace flash ECC & data management daemon

`ufedmd` is a userspace daemon that uses the `ufedm` kernel module to
manage ECC and data layout of a NAND flash chip.

It uses a chain of codecs - a set of plugin-like encoder and decoders
that can transform the data, according to a known policy, during setup
with a specified configuration for each codec.

Together with `ufedm` and `mtdpartikr`, a `ufedmd` instance be spawned on
an MTD with specific set of erase blocks to handle. 

## Why do you write this program?

The program allows a user to specify their own layout, error correction
engine and specific parameters about the pipeline, to experiment with
management of data and metadata on a flash chip, and discover best paths
and new possible ways of optimizing data capacity & retention.

After `ufedm` proved to work fine (with Hamming SECDED algorithm), I
realized I'd want to learn more about the trade-offs of using algorithms
like BCH and Reed-Solomon and what works best in many use-cases I'd like
to experiment on.

## How can I use this program?

This project is under heavy development.
However, presumably there would be a YAML file containing the chains of
codecs and their parameters, for read requests and write requests.

Then, together, with incoming read and write requests, these chains will
be invoked according to the I/O request type.

## So what is the development plan?

See [development](DEVELOPMENT.md) document for more details.

## License

This project is licensed under the GNU General Public License, version 2 only (GPL-2.0-only), except where otherwise noted.

The files `src/common/hashtable.c` and `src/common/hashtable.h` are derived from/copy code from the original work by Joshua J. Baker and are licensed under the MIT License. See the copyright and license notices in those files for details.

The ufedm project is part of the nfiop project, and is licensed under the
GPL-2.0-only license. 