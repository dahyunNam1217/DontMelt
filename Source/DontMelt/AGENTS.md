# Don't Melt — Codex Instructions

## Project
- Engine: Unreal Engine 5.5
- Language: C++
- Multiplayer: 2–4 player Listen Server
- Sessions: Steam
- Goal: Game client programmer portfolio

## Core Architecture
- Server Authority
- Single Source of Truth for gameplay state
- Minimal Replication
- Event-driven gameplay where appropriate
- C++-first gameplay logic
- Blueprint primarily for presentation and UI

## Required Development Process
1. Read relevant documentation.
2. Analyze existing code.
3. Compare design and implementation.
4. Propose architecture and implementation plan.
5. Wait for explicit approval.
6. Modify only approved files and functionality.
7. Review code changes.
8. Build and test if available.
9. Report results and remaining risks.
10. Wait for final approval.

## Unreal Rules
- Do not change gameplay rules without approval.
- Preserve Blueprint-facing APIs and asset references.
- Review RPC ownership and execution context.
- Verify replicated state has an authoritative owner.
- Do not add unnecessary Tick or Replication.
- Consider Host and Client behavior separately.
- Do not modify binary Unreal assets without explicit approval.

## Safety
- Never delete project files without approval.
- Never commit, push, or merge without approval.
- Do not change plugins, dependencies, or project settings without approval.
- Stop when a task requires changing approved architecture.
- Never claim successful build or testing unless actually executed.

## Response Format
For each task report:
1. Files inspected
2. Current implementation
3. Problems and evidence
4. Proposed changes
5. Files to modify
6. Tests needed
7. Risks
8. Approval required

## Documentation
- Resolve these paths from the project root (`DontMelt/`), not this module directory.
- Read the root `AGENTS.md` and `CodexContext/README.md` first.
- Read `CodexContext/ProjectOverview.md`, `CodexContext/Architecture.md`, `CodexContext/GameplayRules.md`, and `CodexContext/NetworkPolicy.md`.
- Read `CodexContext/ImplementationStatus.md`; distinguish its historical ZIP record from its Git-baseline analysis.
- Read relevant specifications in `CodexContext/FeatureSpecs/`.
- Consult `Docs/Design/`, `Docs/Technical/`, and relevant `Docs/Debugging/`, `Docs/Refactoring/`, `Docs/Performance/`, and `Docs/Verification/` documents for supporting evidence.
- Report missing or conflicting documents.
- Do not treat unverified design documents as implemented code.
- Evaluate requirements, user approval evidence, C++ evidence, Blueprint/assets, compile/runtime verification, and unresolved decisions separately.
- Keep designs without an available explicit user approval source marked as approval history required (F). A document, source implementation, or Git commit is not approval of the gameplay rule.
- Record design/code differences without silently changing either into the final design. Follow the current user's approved scope before editing or running validation.
