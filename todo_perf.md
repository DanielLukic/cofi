# COFI Performance Optimization TODO

## Critical Performance Issues

### 1. D-Bus Instance Check Timeout (Impact: ~900ms) ✓ COMPLETED
- [x] Reduce timeout from 1000ms to 150ms
- [x] Add lock file pre-check to skip D-Bus when no instance exists
- [ ] Consider async D-Bus call during startup (future enhancement)
- [x] Profile actual D-Bus response times (0.08ms cold start, 3.9ms with existing instance)

### 2. Window Recreation on D-Bus Reopen (Impact: ~200-300ms)
- [ ] Keep GTK window instance alive, just hide/show
- [ ] Avoid re-enumerating windows on reopen
- [ ] Skip filter_windows() call if no search text
- [ ] Simplify focus grab strategy (remove 50ms delay)

### 3. X11 Window Enumeration (Impact: ~100-500ms)
- [ ] Implement lazy property loading (fetch only titles initially)
- [ ] Batch X11 requests using XGrabServer/XUngrabServer
- [ ] Cache static properties (class, instance) between runs
- [ ] Consider async X11 property fetching

## Secondary Optimizations

### 4. Progressive Display
- [ ] Show window with partial data immediately
- [ ] Load remaining window properties in background
- [ ] Update display incrementally as data arrives

### 5. Configuration Loading
- [ ] Load config/harpoon asynchronously after window shown
- [ ] Cache parsed configuration in memory
- [ ] Defer config saves (batch on exit)

### 6. GTK Optimization
- [ ] Pre-create GTK widgets and reuse them
- [ ] Minimize CSS styling calculations
- [ ] Use fixed-size text buffer to avoid reallocations

### 7. Initial Filtering
- [ ] Skip full filtering when search is empty
- [ ] Use simple memcpy for initial display
- [ ] Defer sorting until user types

## Measurement Strategy

### Timing Points to Add:
1. Start of main()
2. After D-Bus check
3. After X11 connection
4. After window enumeration
5. After GTK setup
6. When window becomes visible
7. When entry widget receives focus

### Metrics to Track:
- Time from launch to window visible
- Time from D-Bus call to window visible (reopen case)
- Number of X11 round-trips during startup
- Memory allocations during startup

## Investigation Order

1. **D-Bus timeout** - Easiest fix with biggest impact
2. **Window reuse** - High impact for reopen case
3. **Lazy X11 loading** - Most complex but significant impact
4. **Progressive display** - Better perceived performance