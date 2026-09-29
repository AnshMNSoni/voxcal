# n8n Step-by-Step Configuration Guide

This directory contains the exact prompts, system instructions, node configurations, and tool
descriptions for the **current VoxCal n8n architecture**, which uses two logical workflow layers:

1. **Main VoxCal Workflow** (`Voxcal`, id: `jVxIMtua9iTDiASX`) — Webhook → Edit Fields → AI Agent → Respond to Webhook
2. **Calendar Worker Subworkflow** (`Calendar Worker`, id: `enfAYZyRbaMrYIol`) — deterministic CRUD execution layer

---

## Table of Configuration Guides

### Main Workflow

1. [User Message Template](user-prompt.md)  
   Verbatim template from AI Agent node. Injects current_date, current_datetime, timezone
   (computed by Edit Fields from `$now`), and the raw ESP32/test command.

2. [AI Agent System Prompt](system-prompt.md)  
   Verbatim system prompt from AI Agent node. Defines identity, authoritative clock rules,
   relative-date resolution invariants, Calendar Worker contract, event-ID safety, and voice
   response constraints.

### Calendar Worker

3. [Calendar Worker Tool Contract](calendar-worker-tool.md)  
   Verbatim tool description exposed to the Main Agent. Includes all seven input fields,
   action rules, originalStartTime/endTime semantics, and timezone handling.

4. [Calendar Worker Subworkflow Architecture](calendar-worker-workflow.md)  
   Full node-by-node documentation of the Calendar Worker, including Mermaid canvas diagram,
   Switch routing logic, If/Get Many/Code chains for Update and Delete, and architecture decisions.

5. [CREATE Branch](create-event-tool.md)  
   Direct Google Calendar Create Event. Switch output 0. No pre-search. Explicit Start/End from trigger.

6. [SEARCH Branch](search-events-tool.md)  
   Google Calendar Get Many Events. Switch output 1. Independent from Update/Delete logic.

7. [UPDATE Branch](update-event-tool.md)  
   Two-path: direct update (known eventId) or Get Many → Code (resolve ID) → Update.
   Documents the FIND-USING-ORIGINAL, UPDATE-USING-NEW invariant.

8. [DELETE Branch](delete-event-tool.md)  
   Two-path: direct delete (known eventId) or Get Many → Code (resolve ID) → Delete.
   Documents multiple-match safety behaviour.

### Analysis

9. [Efficiency Analysis](efficiency-analysis.md)  
   Conservative comparison of old vs. new architecture. Covers LLM token exposure,
   Google Calendar API operation counts, architectural improvements, and known limitations.

---

## Architectural Context

### Main Workflow

```
Webhook  →  Edit Fields  →  AI Agent  →  Respond to Webhook
                                ↕ (tool)
                      Call 'Calendar Worker'
```

- **Webhook**: Listens at `POST /webhook/voxcal/test`
- **Edit Fields**: Computes `current_date`, `current_datetime`, `timezone` from `$now` (Asia/Kolkata).
  Passes through `body` from the original request.
- **AI Agent**: Primary model `models/gemini-3.6-flash`; fallback model `openai/gpt-oss-20b` via Groq.
  Interprets natural language, resolves dates, produces action contract, invokes Calendar Worker.
- **Respond to Webhook**: Returns `{ "status": "success", "message": <agent output>, "source": "n8n" }`

### Calendar Worker

```
When Executed by Another Workflow  →  Switch
    Switch output 0  →  Create an event
    Switch output 1  →  Get many events1  →  Code  →  Edit Fields1
    Switch output 2  →  If (eventId?)
                            TRUE  →  Update an event
                            FALSE →  Get many events (originalStartTime day)  →  Code  →  Update an event
    Switch output 3  →  If1 (eventId?)
                            TRUE  →  Delete an event
                            FALSE →  Get many events2 (startTime day)  →  Code  →  Delete an event
```

The Calendar Worker is a deterministic execution layer — it contains no AI model nodes.
All routing is performed by Switch, If, Code, and Google Calendar n8n nodes.
