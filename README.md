# Intra-Domain Routing Protocols

## Author

- Andrew Negrut; agn5
- Murad Imre; mi15;
- Michael Yu; zy53

## Project Overview

This project implements intra-domain routing protocols for the Bisco GSR9999 router simulator. The implementation includes a Distance Vector (DV) routing protocol that handles dynamic network topologies with support for link failures, recovery, and cost changes.

  - Handles neighbor discovery via PING/PONG
  - Implements periodic and triggered updates
  - Uses poison reverse for loop prevention
  - Supports dynamic topology changes

- **Link-State (LS) Protocol**: Not currently implemented

## Testing

The test.txt file contains:

- Component-level testing approach
- Integration testing strategy
- System-level testing procedures
- Detailed descriptions of all test cases
- Verification methods and expected results

### Running Provided Tests

```bash
# Test 1: Two nodes exchanging data (provided)
./Simulator simpletest1 DV > test1_output.txt
diff test1_output.txt simpletest1.out

# Test 2: Link failure and recovery (provided)
./Simulator simpletest2 DV > test2_output.txt
diff test2_output.txt simpletest2.out
```

### Running Additional Test Cases

```bash
# Multi-hop routing (4 nodes in a line)
./Simulator test_multihop DV > test_multihop.out

# Diamond topology (multiple paths)
./Simulator test_diamond DV > test_diamond.out

# Link failure with rerouting
./Simulator test_reroute DV > test_reroute.out

# Route timeout verification (45 seconds)
./Simulator test_timeout DV > test_timeout.out

# Periodic update timing (30 second intervals)
./Simulator test_periodic DV > test_periodic.out
```

Each test case has a corresponding `.desc` file explaining its purpose and expected behavior.

**Additional test files included:**

- `test_multihop` - Multi-hop routing test
- `test_diamond` - Multiple path selection test
- `test_reroute` - Link failure and rerouting test
- `test_timeout` - Route timeout verification
- `test_periodic` - Periodic update timing test
- `*.desc` files - Descriptions for each test case
