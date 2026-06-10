# Chapter 3 - Event Loops and Readiness-Based Dispatch

This chapter introduces event-driven software architectures and demonstrates how readiness-based dispatch can simplify embedded systems while improving scalability and maintainability.

The examples build portions of the Weather Reporter Device using a single-threaded event loop architecture.

## Examples

The examples in this chapter demonstrate:

* Event loop fundamentals.
* Readiness-based dispatch.
* Timers and periodic processing.
* Handling multiple communication channels.
* Integrating serial, UDP, and TCP inputs into a unified processing model.
* Building responsive systems without excessive threading.

## Projects

### weather_reporter

A simplified implementation of the Weather Reporter Device used throughout the book.

### event_loop_demo

Small focused examples illustrating event loop concepts and dispatch techniques.

## Learning Objectives

After completing this chapter, readers should be able to:

* Understand readiness-based event processing.
* Design systems around events rather than threads.
* Integrate multiple I/O sources into a single processing loop.
* Evaluate when event-driven architectures are preferable to thread-per-component designs.

## Related Chapters

* Chapter 4 – Queues, Buffers, and Backpressure
* Chapter 6 – Task Scheduling and Timing Strategies
* Chapter 11 – Communication Framing and Message Integrity

