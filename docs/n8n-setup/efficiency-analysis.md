# Efficiency Analysis — Old vs. New Architecture

## Overview

This document compares the **old VoxCal architecture** (single ReAct AI Agent with four direct
Google Calendar tools) against the **current architecture** (Main Agent + Calendar Worker
subworkflow). The analysis distinguishes LLM token usage, Google Calendar API operation counts,
and architectural reasoning quality.

> **Methodology note:** No production token telemetry was available at the time of writing.
> Where exact numbers cannot be derived from prompt/tool schema sizes or execution traces,
> claims are labelled as **Architectural** (structurally provable), **Estimated** (reasoned
> approximation), or **Insufficient data**.

---

## Architecture Comparison Summary

| Metric | Old Architecture | New Architecture | Evidence |
|:---|:---|:---|:---|
| Tools exposed to Main Agent | 4 (Create, Search, Update, Delete) | 1 (Calendar Worker) | Architectural |
| LLM tool-routing decisions | LLM chooses among 4 tools | LLM chooses 1 tool; Switch routes deterministically | Architectural |
| LLM prompt/tool schema exposure | 4 separate tool descriptions | 1 combined tool description | Architectural |
| Token savings (exact %) | — | — | Insufficient data (no telemetry) |
| Unnecessary Create pre-search | Present (if Get Many placed before Switch) | Eliminated by Switch-first design | Architectural |
| Update with original/new time pair | Not documented in old README | Implemented | Architectural |
| Ambiguous multi-match destructive ops | Not addressed | Rejected with explicit error | Architectural |
| Event-ID resolution location | LLM/tool level | Centralised in Worker Code nodes | Architectural |
| Google Calendar API ops (Create) | Not documented (old README) | 1 (Create Event) | Verified |
| Google Calendar API ops (Search) | Not documented (old README) | 1 (Get Many) | Verified |
| Google Calendar API ops (Update, known ID) | Not documented (old README) | 1 (Update Event) | Verified |
| Google Calendar API ops (Update, unknown ID) | Not documented (old README) | 2 (Get Many + Update) | Verified |
| Google Calendar API ops (Delete, known ID) | Not documented (old README) | 1 (Delete Event) | Verified |
| Google Calendar API ops (Delete, unknown ID) | Not documented (old README) | 2 (Get Many + Delete) | Verified |

---

## Token Analysis

### What generates LLM tokens

Three quantities should be distinguished:

1. **Model input tokens**: System prompt + user message template + tool schema descriptions +
   conversation history in context.
2. **Model output tokens**: The model's response (intent, tool call arguments, final answer).
3. **Total n8n workflow tokens**: Sum of all LLM model calls in the entire n8n execution.

### Does the Calendar Worker subworkflow consume LLM tokens?

**No.** The Calendar Worker (`enfAYZyRbaMrYIol`) contains only:
- `executeWorkflowTrigger` (trigger node — no LLM)
- `switch` (deterministic routing — no LLM)
- `if` (condition check — no LLM)
- `n8n-nodes-base.googleCalendar` (API call — no LLM)
- `n8n-nodes-base.code` (JavaScript execution — no LLM)
- `n8n-nodes-base.set` (field assignment — no LLM)

**The Calendar Worker executes zero LLM calls.** Moving logic into a subworkflow does not by
itself reduce LLM token usage if the subworkflow contains no AI model nodes.

### Where token reduction is architectural

The reduction comes from exposing **one compact tool schema** to the Main Agent instead of
**four separate tool schemas**. In the old architecture, the AI Agent received four tool
descriptions in its context on every execution. In the new architecture, it receives one
combined Calendar Worker tool description.

Whether this represents a meaningful token reduction depends on the relative sizes of:
- (old) four individual tool descriptions
- (new) one Calendar Worker tool description

The new Calendar Worker tool description is longer than any single old tool description because
it encodes all four operation rules. However, it is almost certainly shorter than the combined
length of four separate tool descriptions with individual parameter explanations.

**This is an architectural improvement with estimated (not measured) token reduction.**
Exact measurement requires n8n execution telemetry before and after.

### Reasoning/Decision Surface

| Dimension | Old Architecture | New Architecture |
|:---|:---|:---|
| LLM tool selection | LLM must choose 1 of 4 calendar tools | LLM chooses 1 tool; sends action string |
| LLM responsibility | Select tool + resolve dates + produce params | Resolve dates + produce action contract |
| Deterministic routing | None (LLM decides tool) | Switch node routes action to branch |
| Event-ID resolution | LLM/tool level (search → extract ID → call update/delete) | Worker Code nodes (deterministic) |

The new architecture **reduces the LLM's decision surface**: the model no longer selects among
four execution tools. It interprets intent and produces a structured action string. Execution
routing is handled deterministically by the Switch node.

This is an **architectural reduction in LLM decision complexity**, not a claim that the model
is objectively more accurate. It means fewer failure modes attributable to incorrect tool
selection.

---

## Google Calendar API Operation Analysis (Scenario-Based)

These counts represent Google Calendar API operations inside the Calendar Worker.
They are not LLM calls.

### Create

| Scenario | New Architecture |
|:---|:---|
| Create with no prior ID | **1** operation: Create Event |

Old architecture: The old README documents four direct tools but does not specify whether a
Get Many was executed before Create. If a Get Many was placed before the Switch (an observed
failure mode during development), this would be **2** operations for Create.
The current architecture guarantees **1** by routing Create directly via Switch.

### Search

| Scenario | New Architecture |
|:---|:---|
| Search any date range | **1** operation: Get Many Events |

No change expected; Search has always been a single Get Many call.

### Update

| Scenario | New Architecture |
|:---|:---|
| Update with known eventId | **1** operation: Update Event |
| Update without known eventId | **2** operations: Get Many + Update Event |

### Delete

| Scenario | New Architecture |
|:---|:---|
| Delete with known eventId | **1** operation: Delete Event |
| Delete without known eventId | **2** operations: Get Many + Delete Event |

---

## Token Efficiency Table

| Concern | Old Architecture | New Architecture | Evidence | Confidence |
|:---|:---|:---|:---|:---|
| Number of tool descriptions in agent context | 4 | 1 | Verified from exports | High |
| Combined tool description token count (old vs new) | Larger (4 separate descriptions) | Smaller (1 combined description) | Architectural estimate | Estimated |
| Exact % token reduction | — | — | No telemetry available | Insufficient data |
| LLM calls per workflow execution | 1 (Main Agent) | 1 (Main Agent) | Verified — Worker has no LLM | High |
| Worker LLM token cost | — | 0 (no AI model in Worker) | Verified from Worker export | High |
| System prompt length | Shorter (4-tool rules) | Longer (single Worker + all rules) | Partially verified | Estimated |
| Avoid unnecessary Create API call | Not guaranteed | Guaranteed by Switch | Architectural | High |
| Avoid ambiguous multi-match delete | Not addressed | Explicit error thrown | Architectural | High |

---

## What Changed from Old to New Architecture

| Dimension | Old Architecture | New Architecture |
|:---|:---|:---|
| Calendar tool exposure | 4 direct calendar tools (Create, Search, Update, Delete) | 1 Calendar Worker tool |
| Tool routing | LLM selects tool on each request | Deterministic Switch routes by action string |
| Event-ID resolution | Duplicated in each tool's description; LLM-driven | Centralised in Worker If + Code nodes |
| Update time pairs | Single time pair (no originalStartTime/endTime) | Dual time pair (original to find, new to write) |
| Agent responsibility | Broad: tool selection + date resolution + execution | Focused: date resolution + action contract production |
| Create pre-search | Possible if node order incorrect | Eliminated: Switch routes Create before any search |
| Destructive operation safety | Not formally addressed | Multiple-match rejected with explicit error |
| Google Calendar mechanics | Embedded in agent prompt | Isolated inside Worker nodes |

---

## Known Limitations

The following limitations exist in the current implementation and are not currently solved:

1. **Title-based deletion ambiguity.** If multiple events with the same title exist on the same
   calendar day, the Delete Code node throws an error. The user must provide more specificity
   (e.g. exact time) to disambiguate. This is the intended safe behaviour, not a bug.

2. **Timed events only in Update/Delete resolution.** The Update and Delete event-ID resolver
   Code nodes use `event.start.dateTime` exclusively. All-day events use `event.start.date` instead
   and will not be matched by the current timestamp-comparison logic. All-day event support
   would require additional handling.

3. **Recurring events.** Google Calendar may return instance IDs and `recurringEventId` values for
   recurring event instances. The current Code nodes match on `event.id` (the instance ID).
   Edge cases involving recurring event series may require additional handling.

4. **Fuzzy title matching intentionally excluded.** The Code nodes use exact normalised equality
   (`trim().toLowerCase()`). Fuzzy matching is not used because it could select the wrong event.

5. **Exact runtime token savings require telemetry.** The token efficiency claims in this document
   are architectural or estimated. Actual runtime token counts require n8n execution telemetry
   (model input/output token logs) before and after the architecture change.

6. **Single calendar only.** The current implementation targets one Google Calendar account
   (`anshsoni702@gmail.com`). Multi-calendar support would require changes to the Worker nodes.

---

## Security and Safety Invariants

- **Event IDs must never be invented.** `eventId` must be a real Google Calendar event ID or
  the empty string `""`. The system prompt, tool description, and field hints all enforce this.
- **Destructive actions must not silently choose among multiple matches.** The Delete (and Update)
  Code nodes throw explicit errors when multiple events match the search criteria.
- **Missing required dates/times must be clarified, not guessed.** The system prompt instructs
  the model to ask for missing information rather than assuming.
- **Success responses are emitted only after the calendar operation succeeds.** The agent uses
  the `output` field from the Worker response; if the Worker throws, the agent receives an error
  and must handle it rather than claiming success.
- **Credentials and secrets are git-ignored.** All `.env` files, `secrets.h`, OAuth credentials,
  and Google Calendar API tokens are excluded from version control. Example files
  (`.env.example`, `secrets.h.example`) are provided as templates.
