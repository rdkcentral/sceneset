## 1. Telemetry Plumbing

- [x] 1.1 Identify and include the existing T2 telemetry publish dependency used by related app-management components
- [x] 1.2 Add a SceneSet telemetry helper abstraction that defines marker names and payload field keys
- [x] 1.3 Implement compile-time telemetry guard only inside the helper's final publish path (no-op when disabled)

## 2. Launch Timing Capture

- [x] 2.1 Record SceneSet startup timestamp and preinstall start/end timestamps in the current startup flow
- [x] 2.2 Record home app launch-request timestamp for each launch attempt
- [x] 2.3 On home app `ACTIVE`, compute and publish `total_start_to_active_ms`, `preinstall_duration_ms`, and `launch_to_active_ms`

## 3. Relaunch and Termination Metrics

- [x] 3.1 Track cumulative relaunch count for the SceneSet process lifetime
- [x] 3.2 Classify last termination as `crash` or `intentional_kill` from lifecycle transition context
- [x] 3.3 Publish relaunch telemetry on successful post-termination `ACTIVE` with cumulative count and termination nature

## 4. Verification

- [x] 4.1 Add/update unit tests for timing computation and telemetry trigger points in startup/lifecycle handlers
- [x] 4.2 Add/update unit tests for cumulative relaunch counting and termination-nature classification
- [ ] 4.3 Validate telemetry marker fields and values in runtime logs for initial active, kill-and-relaunch, and crash-and-relaunch scenarios
