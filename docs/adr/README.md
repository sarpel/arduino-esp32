# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records for the ESP32 Audio Streamer project.

## What are ADRs?

Architecture Decision Records document important architectural decisions made during the development of this project, including:
- Context and motivation for the decision
- Decision made
- Consequences (positive and negative)
- Alternatives considered

## ADR Index

| ID | Title | Status | Date |
|----|-------|--------|------|
| [001](001-event-driven-architecture.md) | Event-Driven Architecture with EventBus | Accepted | 2025-11-01 |
| [002](002-memory-pool-strategy.md) | Memory Pool Allocation Strategy | Accepted | 2025-11-01 |
| [003](003-static-buffer-i2s.md) | Static Buffer for I2S Audio | Accepted | 2025-11-01 |
| [004](004-multi-wifi-failover.md) | Multi-WiFi Failover Design | Accepted | 2025-11-01 |
| [005](005-circuit-breaker-pattern.md) | Circuit Breaker for Reliability | Accepted | 2025-11-01 |

## Template

Use the [ADR template](template.md) when creating new ADRs.

## Status Values

- **Proposed**: Decision under consideration
- **Accepted**: Decision approved and implemented
- **Deprecated**: Decision superseded by newer ADR
- **Rejected**: Decision considered but not implemented
