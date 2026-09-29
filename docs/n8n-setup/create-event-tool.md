# Calendar Worker — CREATE Branch

## Overview

The **create** action routes through Switch output index `0` directly to the **Create an event**
Google Calendar node. No search or lookup is performed before creation.

This is a key architectural invariant: Create must not perform a prior event search, because no
existing event needs to be located. The Switch ensures Create bypasses all If/Get Many nodes.

---

## Node Chain

```
Switch (output 0)
  └─► Create an event  [Google Calendar, Create operation]
```

---

## n8n Node Configuration (from workflow export)

**Node name:** `Create an event`  
**Type:** `n8n-nodes-base.googleCalendar`  
**Operation:** `Create` (default)

| Parameter | n8n Expression | Description |
|:---|:---|:---|
| Calendar | `anshsoni702@gmail.com` (list mode) | Target Google Calendar |
| Start | `={{ $('When Executed by Another Workflow').first().json.startTime }}` | Absolute ISO 8601 start time from trigger |
| End | `={{ $('When Executed by Another Workflow').first().json.endTime }}` | Absolute ISO 8601 end time from trigger |
| Summary | `={{ $('When Executed by Another Workflow').first().json.title }}` | Event title from trigger |

> **Important:** Start and End are set to **explicit expressions referencing the Worker trigger**.
> If these fields are left empty or unset, the Google Calendar API (and n8n's default behaviour)
> may fall back to the current time, creating an event at the wrong time. Always supply explicit
> Start and End values.

---

## Input Contract for CREATE

The Main AI Agent sends the following fields to Calendar Worker for a create operation:

| Field | Required | Value |
|:---|:---|:---|
| `action` | ✅ | `"create"` |
| `title` | ✅ | Event title (e.g. `"Team meeting"`) |
| `startTime` | ✅ | Absolute ISO 8601 (e.g. `"2026-09-30T10:00:00+05:30"`) |
| `endTime` | ✅ | Absolute ISO 8601 (e.g. `"2026-09-30T11:00:00+05:30"`) |
| `eventId` | ✅ | `""` (empty — event does not exist yet) |
| `originalStartTime` | — | Not required for create |
| `originalEndTime` | — | Not required for create |

---

## Agent Interaction Workflow

1. User says: *"Schedule a Team meeting tomorrow at 10 AM."*
2. AI Agent resolves "tomorrow" against `current_date` → `2026-09-30T10:00:00+05:30`
3. AI Agent calls Calendar Worker with `action=create`, `title="Team meeting"`,
   `startTime=2026-09-30T10:00:00+05:30`, `endTime=2026-09-30T11:00:00+05:30`, `eventId=""`
4. Calendar Worker Switch routes to output 0 → **Create an event**
5. Google Calendar API creates the event and returns the new event object (including real event ID)
6. Calendar Worker returns result to Main Agent
7. Main Agent responds: *"Team meeting scheduled for tomorrow at 10 AM."*

---

## Verified Status

Create was **successfully tested** in the current development workflow.
