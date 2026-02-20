# EmbeddedSecurity Documentation (Single-Board Release)

This documentation set is rewritten for the final single-board submission.

- Active architecture: `main-board` only
- Documentation style: business-rule-driven
- Legacy content (old multi-board docs): `docs/legacy_2026-02-19/`

## Document Index

- `docs/business_rules.md`
  - Authoritative business rules (BR-xx) for runtime behavior.
- `docs/mode_event_response_matrix.md`
  - Presentation-friendly mapping from mode+event to action and LINE output path.
- `docs/full_system_flow_detailed.mmd`
  - Detailed flowchart in business-rule view.
- `docs/full_system_flow_detailed.svg`
  - Rendered flowchart SVG (submission-ready layout).
- `docs/flowchart_summary_compact.svg`
  - Compact flowchart SVG for presentation and quick walkthrough.
- `docs/block_diagram.md`
  - System and internal block diagrams for single-board architecture.
- `docs/block_diagram.svg`
  - Rendered system-level block diagram SVG.
- `docs/circuit.md`
  - Wiring-level circuit reference and pin map.
- `docs/circuit.svg`
  - Rendered circuit wiring diagram SVG.
- `docs/pin_allocation_overview.svg`
  - Presentation-ready pin allocation table for single-board scope.
- `docs/waterfall_plan_to_final.svg`
  - Waterfall plan from scope lock to final submission.
- `docs/project_description_requirements_details_testcases.md`
  - Requirements and acceptance tests mapped to business rules.
- `docs/project_description_requirements_details_testcases_th.md`
  - Thai version for submission/reporting.
- `docs/serial_test_codes.md`
  - Serial-based test injection codes for no-hardware sensor simulation.
- `docs/line_response_selected_testcases.md`
  - Selected high-confidence no-board test scenarios with expected LINE response type.
- `docs/submission_pack/README.md`
  - Submission pack index.
- `docs/workbench/README.md`
  - Temporary draft workspace for non-final artifacts.
- `docs/project_structure.md`
  - Project folder placement rules and what belongs where.

## Scope of This Revision

- Removed auto-board coupling from active documentation scope.
- Focused rules on detection, arming/disarming, remote command security, and lock control.
- Kept old documents unchanged in `docs/legacy_2026-02-19/`.

## Folder Rules

- Keep final submission files directly under `docs/` (or `docs/submission_pack/`).
- Keep historical/retired files under `docs/legacy_2026-02-19/`.
- Keep scratch and temporary diagram experiments under `docs/workbench/`.
- Do not keep `tmp_*` working files at repository root.
