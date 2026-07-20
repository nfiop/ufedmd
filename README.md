# ufedmd - A userspace flash ECC & data management daemon

`ufedmd` is a userspace daemon that uses the `ufedm` kernel module to
manage ECC and data layout of a NAND flash chip.

It uses a chain of codecs - a set of plugin-like encoder and decoders
that can transform the data, according to a known policy, during setup
with a specified configuration for each codec.

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

The idea is to support algorithms like Reed-solomon, Hamming codes like
Hamming (7,4) and BCH (its most popular variants - BCH4,BCH8, BCH16).

I also want to add support for LDPC, but that algorithm is CPU-intensive
and heavy on computational power, so I need to see how it will fit in.

Last but not least, I also want to support codecs like XORing (according
to the Linux kernel, some flash chips must be written with XORed data to
avoid data corruption) and byte swaps, to aid better data retention and
experimentation on different layouts on some flash chips.

