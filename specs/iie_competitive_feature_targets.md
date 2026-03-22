# Apple IIe Competitive Feature Targets

## Purpose
Capture practical feature targets inspired by major Apple IIe-era software ecosystems, so roadmap choices can be compared against concrete historical baselines.

## Reference Platforms
1. **Apple DOS 3.3 / ProDOS**
- Baseline for file, disk, and launcher reliability.

2. **GEOS**
- Baseline for GUI workflow, app integration, and desktop usability.

3. **AppleWorks**
- Baseline for productivity responsiveness and integrated workflows.

4. **CP/M on Apple IIe (via Z80 card)**
- Baseline for ecosystem/toolchain compatibility expectations.

## Feature Targets To Match or Exceed

### 1) DOS/ProDOS-Class Targets
- Fast, reliable load/save for binary and data files.
- Predictable block-level storage behavior and error handling.
- Stable slot/device interaction model.
- Straightforward launch path for custom programs.
- Under RAM pressure, support app suspend-to-disk save states and resume to preserve multi-app workflow continuity.

### 2) GEOS-Class Targets
- Serviceable desktop/app UX (window/page management and app switching).
- Shared system services for display, input, storage, and printing/output abstraction.
- App lifecycle conventions (init, run, suspend/resume, shutdown).
- Smooth handoff between shell/services/apps without mode corruption.

### 3) AppleWorks-Class Targets
- Fast startup and low-overhead runtime behavior.
- Strong responsiveness on common editing/data tasks.
- Integrated workflow ergonomics (text/data/report loops).
- High stability and recoverability over long sessions.

### 4) CP/M-Class Targets
- Clear and scriptable command/tool execution model.
- Predictable console and file semantics for developer tooling.
- Compatibility bridge story (import/export, data interchange, tooling adapters).

## Mapping to Current Kernel Direction
1. **Trap ABI + hot path ABI**
- Compete with ProDOS reliability and CP/M-style tooling expectations through explicit service contracts and deterministic call behavior.

2. **Hot-swappable pages/modules**
- Compete with GEOS-style app/service flexibility while preserving small-memory practicality.

3. **Daisy-chain checkpoint scheduler**
- Compete with AppleWorks responsiveness and improve stability under mixed workloads.

4. **Privileged emulation tier**
- Unify system/user process model, simplify clock ownership, and improve reset-resume consistency.

## Minimum Competitive Milestone (v1)
1. Reliable file load/save + crash-safe recovery metadata.
2. Deterministic service dispatch + panic log/report path.
3. Incremental page work (`start/step/status`) with VBL-aware commit.
4. Responsive input/audio under concurrent page activity.
5. Documented custom-program launch flow (monitor/BASIC/loader path).

## Competitive Gaps To Watch
- GUI shell maturity vs GEOS.
- Built-in productivity workflow depth vs AppleWorks.
- Existing software compatibility breadth vs CP/M ecosystem expectations.
- End-user discoverability and operational polish vs ProDOS-era familiarity.
