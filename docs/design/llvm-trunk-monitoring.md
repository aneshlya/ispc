# LLVM Trunk Monitoring System Design

This document describes the automated system for monitoring and fixing LLVM trunk CI failures in ISPC.

## Overview

The system monitors the "Nightly Linux tests / LLVM trunk" GitHub Actions workflow and automatically analyzes failures, categorizes them, and proposes fixes.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     LLVM Trunk Monitoring System                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌────────────────────────┐      ┌────────────────────────────────────────┐│
│  │ linux-nightly-trunk.yml│      │  llvm-trunk-failure-analyzer.yml       ││
│  │ (modified)             │─────▶│  (new workflow - V1)                   ││
│  │                        │      │                                        ││
│  │ + Log LLVM commit      │      │  - Triggers on nightly failure         ││
│  │ + Accept SHA input     │      │  - Invokes Claude Code CLI             ││
│  └────────────────────────┘      │  - Runs /analyze-llvm-trunk skill      ││
│                                  └────────────────────────────────────────┘│
│                                                │                            │
│                                                ▼                            │
│  ┌────────────────────────┐      ┌────────────────────────────────────────┐│
│  │ llvm-bisect.yml        │◀─────│  llvm-trunk-analyzer agent             ││
│  │ (new workflow - V2)    │      │                                        ││
│  │                        │      │  - Categorizes failure type            ││
│  │ - Build LLVM at SHA    │      │  - Analyzes logs and proposes fixes    ││
│  │ - Build ISPC           │      │  - Orchestrates bisection (V2)         ││
│  │ - Run specific test    │      │  - Creates fix proposals               ││
│  │ - Parallel execution   │      │                                        ││
│  └────────────────────────┘      └────────────────────────────────────────┘│
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Failure Categories

| Category | Detection | Resolution |
|----------|-----------|------------|
| **ISPC Regression** | Recent ISPC commits (last 24h) related to failure | Fix ISPC code or revert problematic commit |
| **Lit Test Failures** | `check-all` fails, ISPC builds OK, no recent related ISPC changes | Update CHECK patterns with LLVM version guards |
| **LLVM API Changes** | ISPC build fails with compilation errors | Update ISPC code with `ISPC_LLVM_VERSION` guards |
| **LLVM Regressions** | Functional tests fail or incorrect codegen, no ISPC changes related | Bisect LLVM, draft upstream issue |

### Detection Priority

The agent should check categories in this order:

1. **ISPC Regression First**: Check git log for last 24 hours (~5 commits typical). If any commit touches files related to the failure, investigate as ISPC regression.

2. **LLVM API Changes**: If ISPC build fails with compilation errors, it's an API change.

3. **Lit Test vs LLVM Regression**: If build succeeds but tests fail:
   - Lit test failures with changed CHECK patterns → Lit test update needed
   - Functional test failures or incorrect codegen → Likely LLVM regression

## Components

### 1. Modified `linux-nightly-trunk.yml`

**Changes:**
- Add `workflow_dispatch` input for optional LLVM SHA
- Capture and store LLVM commit hash as artifact
- Pass SHA to Docker build when provided

**New inputs:**
```yaml
workflow_dispatch:
  inputs:
    llvm_sha:
      description: 'Specific LLVM commit SHA to build (leave empty for trunk HEAD)'
      required: false
      type: string
```

**New artifacts:**
- `llvm-commit-sha.txt` - Contains the actual LLVM commit hash used

### 2. `llvm-trunk-failure-analyzer.yml` (V1)

**Trigger:** `workflow_run` on `linux-nightly-trunk.yml` completion

**Flow:**
1. Check if nightly workflow failed
2. Download LLVM commit artifact from failed run
3. Fetch previous successful run's LLVM commit
4. Check recent ISPC commits (last 24 hours)
5. Set up Claude Code CLI via `npx @anthropic-ai/claude-code`
6. Run `/analyze-llvm-trunk` skill with context

**Required secrets:**
- `ANTHROPIC_API_KEY` - Claude API key

### 3. `llvm-bisect.yml` (V2 - Future)

**Purpose:** Single-point LLVM build and test for bisection

**Inputs:**
```yaml
inputs:
  llvm_sha:
    description: 'LLVM commit SHA to build and test'
    required: true
    type: string
  test_filter:
    description: 'Specific test pattern to run'
    required: false
    type: string
  test_type:
    description: 'Type of test: build, lit, functional'
    required: true
    type: choice
    options:
      - build
      - lit
      - functional
```

**Outputs:**
- `result` - pass/fail
- `build_success` - whether ISPC built
- `test_output` - logs

### 4. `llvm-trunk-analyzer` Agent

**Location:** `.claude/agents/llvm-trunk-analyzer.md`

**Capabilities:**
- Fetch workflow run logs via `gh` CLI
- Check recent ISPC commits for potential regressions
- Parse and categorize failures
- Analyze LLVM commits to understand changes
- Generate fixes for lit tests
- Generate ISPC code fixes for API changes
- Orchestrate bisection (V2)
- Draft LLVM issue reports

### 5. `analyze-llvm-trunk` Skill

**Location:** `.claude/skills/analyze-llvm-trunk.md`

**Purpose:** Entry point triggered by GitHub Action or manually

**Arguments:**
- `--run-id <ID>` - Failed workflow run ID (optional, auto-detects latest)
- `--current-sha <SHA>` - Current LLVM commit (optional, fetched from artifacts)
- `--previous-sha <SHA>` - Previous LLVM commit (optional, fetched from previous run)

## Implementation Phases

### V1 - Analysis and Local Fixes (Current)

**Scope:**
- Modify nightly workflow to track LLVM commits
- Create failure analyzer workflow
- Create analyzer agent and skill
- Detect failure type from logs
- Check recent ISPC commits for regressions
- Propose fixes without bisection

**Capabilities:**
- Download and parse workflow logs
- Identify failing tests/files
- Check ISPC git log (last 24h) for related changes
- Categorize failure type
- For ISPC regressions: identify problematic commit, propose fix or revert
- For lit tests: analyze CHECK patterns, propose updates
- For API changes: identify changed APIs, propose `ISPC_LLVM_VERSION` guards
- For LLVM regressions: identify symptoms, draft issue template

### V2 - Automated Bisection

**Scope:**
- Create bisection workflow
- Implement parallel bisection in agent
- Automatic guilty commit identification

**Bisection Strategy (Parallel):**

For N commits between last-good and first-bad:
1. Calculate log2(N) midpoints for parallel testing
2. Trigger multiple bisect workflows simultaneously
3. Collect results and narrow range
4. Repeat until single commit identified

Example for 32 commits:
```
Round 1: Test commits at positions 8, 16, 24 (parallel)
Round 2: Narrow to 8-commit range, test at 2, 4, 6 (parallel)
Round 3: Narrow to 2-commit range, test at 1 (single)
Total: ~9 builds instead of 5 sequential = faster wall-clock time
```

**Estimated time:** ~4-6 hours for 50 commits (vs 10-12 hours sequential)

### V3 - Full Automation

**Scope:**
- Automatic PR creation for simple fixes
- Automatic issue filing for LLVM bugs
- Notification system (Slack/email)
- Historical tracking and analytics

## API Usage

### ISPC Version Guards

For API changes, use existing macros:
```cpp
#if ISPC_LLVM_VERSION >= ISPC_LLVM_21_0
    // New API
#else
    // Old API
#endif
```

Macro definitions in `src/ispc_version.h`:
```cpp
#define ISPC_LLVM_21_0 2100
#define ISPC_LLVM_20_1 2001
// etc.
```

### Lit Test Version Guards

For lit tests with different expected output:
```llvm
; RUN: %{ispc} %s --emit-llvm-text -o - | FileCheck %s

; CHECK: some_instruction
; LLVM21-CHECK: new_instruction_format
; LLVM20-CHECK: old_instruction_format
```

Or using CHECK-DAG for order-independent matching:
```llvm
; CHECK-DAG: instruction_a
; CHECK-DAG: instruction_b
```

## File Structure

```
.github/workflows/
├── linux-nightly-trunk.yml      # Modified: add SHA tracking/input
├── llvm-trunk-failure-analyzer.yml  # New: V1 analyzer workflow
└── llvm-bisect.yml              # New: V2 bisection workflow

.claude/
├── agents/
│   └── llvm-trunk-analyzer.md   # New: analyzer agent
└── skills/
    └── analyze-llvm-trunk.md    # New: analyzer skill

docs/design/
└── llvm-trunk-monitoring.md     # This file
```

## Environment Variables

| Variable | Description | Source |
|----------|-------------|--------|
| `ANTHROPIC_API_KEY` | Claude API key | GitHub Secret |
| `GITHUB_TOKEN` | GitHub API access | Automatic |
| `LLVM_SHA` | LLVM commit being tested | Workflow input/artifact |
| `FAILED_RUN_ID` | ID of failed nightly run | Workflow context |

## Error Handling

### Transient Failures
- Network issues during log fetch: Retry with exponential backoff
- API rate limits: Wait and retry

### Permanent Failures
- Cannot categorize failure: Report as "unknown" with full logs
- Fix proposal fails: Report analysis without fix

### Safety Guards
- Never auto-merge PRs
- Require human review for all changes
- Log all actions for audit

## Metrics and Monitoring (V3)

Track:
- Time to detection
- Time to fix proposal
- Fix accuracy (human acceptance rate)
- Bisection efficiency
- False positive rate

## Security Considerations

- Claude API key stored in GitHub Secrets
- No direct repository write access from Claude
- All changes proposed via standard PR process
- Workflow runs in isolated environment
