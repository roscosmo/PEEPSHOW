# RTOS Index

This section defines ThreadX ownership, queue topology, ISR discipline, and deterministic object lifetime.

## Core Notes

- [[RTOS_Ownership_and_Queue_Topology]]
- [[Interface_Control_Document]]
- [[Subsystem_State_Machines]]
- [[Authority_and_Invariants]]

## Required Rule

Every peripheral and shared datapath has exactly one owner thread.

All other firmware layers communicate through typed queues, events, or approved APIs.

