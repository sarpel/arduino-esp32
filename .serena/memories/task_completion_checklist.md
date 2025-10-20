# Task Completion Checklist

## Before Marking Task Complete
- [ ] Code follows project naming conventions (UPPER_SNAKE_CASE for constants, camelCase for functions)
- [ ] Code uses appropriate data types (uint8_t, uint32_t, etc.)
- [ ] Comments use section separators: `// ===== Section Name =====`
- [ ] No compiler warnings or errors
- [ ] Changes respect existing code patterns
- [ ] Memory safety (no buffer overflows, proper cleanup)

## Build Validation
```bash
# Always run before marking complete:
pio run  # Must compile without errors/warnings
```

## Documentation
- [ ] Updated config.h comments if adding new constants
- [ ] Added brief inline comments for new functions
- [ ] No TODO comments left in core functionality

## Git Hygiene
- [ ] Changes committed to feature branch (not main)
- [ ] Commit message is descriptive and follows pattern
- [ ] No temporary files committed
