# limit-order-book
A single-instrument limit order book implementing price-time priority matching, built in C++ to learn low-latency systems design.

# Limit Order Book

A C++ implementation of a single-instrument limit order book, supporting order
placement, cancellation, and price-time priority matching.

## Status: 🚧 In Progress

Currently building this to learn low-latency systems design fundamentals ahead
of applying to backend/systems engineering roles. This is a learning project —
design decisions and code below reflect where I currently am, not a finished
product.

## What it does (planned / in progress)

- [ ] Add a limit order (buy or sell) to the book
- [ ] Cancel an existing order
- [ ] Match incoming orders against the book using price-time priority
- [ ] Support partial fills
- [ ] Print/query current book state (best bid, best ask, depth)

## Design

- Orders are grouped by price level, using a `std::map` to keep price levels
  sorted (ascending for asks, descending for bids).
- Each price level holds a FIFO queue of orders, preserving time priority
  within the same price.
- (This section will be filled in with actual rationale as the design solidifies.)

## Build & Run

```bash
g++ -std=c++17 main.cpp -o orderbook
./orderbook
```

(Instructions will be updated as the project structure develops.)

## Example

(To be filled in with real sample output once matching logic works.)

## Motivation

Built as part of learning C++ and low-latency systems design, following
foundational study in *A Tour of C++* and low-latency programming principles
(Carl Cook, CppCon).

## Next steps

- Finish core matching logic
- Add unit tests
- Benchmark performance
- Explore multi-instrument support / FPGA acceleration as a future extension

