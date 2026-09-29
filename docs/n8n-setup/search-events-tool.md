# Calendar Worker — SEARCH Branch

## Overview

The **search** action routes through Switch output index `1` to the **Get many events1** Google
Calendar node. Search is independent from the Update and Delete branches; it does not share the
Update event-ID resolution logic.

---

## Node Chain

```
Switch (output 1)
  └─► Get many events1  [Google Calendar, Get Many operation]
        └─► Code in JavaScript  [maps raw events to id/title/start/end]
              └─► Edit Fields1  [passthrough — no additional field overrides]
```

---

## n8n Node Configuration (from workflow export)

### Get many events1

**Node name:** `Get many events1`  
**Type:** `n8n-nodes-base.googleCalendar`  
**Operation:** `getAll`

| Parameter | n8n Expression | Description |
|:---|:---|:---|
| Calendar | `anshsoni702@gmail.com` (list mode) | Target Google Calendar |
| Limit | `10` | Maximum events returned |
| Time Min (`timeMin`) | `={{$json.startTime}}` | Search window start (from Worker trigger via Switch) |
| Time Max (`timeMax`) | `={{$json.endTime}}` | Search window end |

> **Note:** `$json.startTime` / `$json.endTime` here refer to the node's input data (from the Switch),
> which flows from the Worker trigger fields.

---

### Code in JavaScript (Search result mapper)

This Code node normalises the raw Google Calendar event objects into a consistent shape:

```javascript
return $input.all().map(item => {
  const e = item.json;

  return {
    json: {
      id: e.id || "",
      title: e.summary || "",
      start: e.start?.dateTime || e.start?.date || "",
      end: e.end?.dateTime || e.end?.date || ""
    }
  };
});
```

> Note: This Search mapper accepts both `dateTime` (timed events) and `date` (all-day events) for
> broad search result display. This is distinct from the Update event-ID resolver, which only uses
> `dateTime`.

---

### Edit Fields1

A passthrough Set node (no additional field overrides) that passes the Code output forward.

---

## Input Contract for SEARCH

| Field | Required | Value |
|:---|:---|:---|
| `action` | ✅ | `"search"` |
| `title` | — | Optional title hint (not used for filtering in this branch) |
| `startTime` | ✅ | Search window start (absolute ISO 8601) |
| `endTime` | ✅ | Search window end (absolute ISO 8601) |
| `eventId` | ✅ | `""` (not applicable for search) |
| `originalStartTime` | — | Not used for search |
| `originalEndTime` | — | Not used for search |

---

## Agent Interaction Workflow

1. User says: *"What meetings do I have tomorrow?"*
2. AI Agent resolves "tomorrow" → `2026-09-30`
3. AI Agent calls Calendar Worker with `action=search`,
   `startTime=2026-09-30T00:00:00+05:30`, `endTime=2026-09-30T23:59:59+05:30`
4. Switch routes to output 1 → **Get many events1**
5. Results are mapped by the Code node
6. Main Agent reads the events and responds: *"Your Team meeting is tomorrow at 10 AM."*

---

## Verified Status

Search was **successfully tested** in the current development workflow.
