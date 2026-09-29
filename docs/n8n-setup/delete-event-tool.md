# Calendar Worker — DELETE Branch

## Overview

The **delete** action routes through Switch output index `3` to an **If1** node that checks whether
a real `eventId` is already known. This gives the Delete branch two execution paths:

- **Path A (eventId known):** Delete the event directly — no search needed.
- **Path B (eventId unknown):** Search the event's calendar day by `startTime`, resolve the real
  event ID via a Code node, then delete the event.

Delete does not use `originalStartTime`/`originalEndTime` because deletion has no destination —
the event simply needs to be found and removed.

---

## Node Chain

```
Switch (output 3)
  └─► If1  [checks: eventId not empty]
        ├─► (TRUE)  Delete an event  [direct delete — no search]
        └─► (FALSE) Get many events2  [searches event day by startTime]
                      └─► Code in JavaScript2  [resolves real event ID by title]
                            └─► Delete an event  [delete with resolved ID]
```

---

## n8n Node Configuration (from workflow export)

### If1 (eventId check)

**Node name:** `If1`  
**Type:** `n8n-nodes-base.if`

| Condition | Expression | Operator |
|:---|:---|:---|
| eventId not empty | `={{ $('When Executed by Another Workflow').first().json.eventId }}` | `string` → `notEmpty` |

- **TRUE output** → `Delete an event` (direct delete)
- **FALSE output** → `Get many events2` (event-ID resolution)

---

### Get many events2 (Delete event-ID lookup)

**Node name:** `Get many events2`  
**Type:** `n8n-nodes-base.googleCalendar`, operation `getAll`

| Parameter | n8n Expression | Description |
|:---|:---|:---|
| Limit | `10` | Max events returned |
| Time Min | `={{ DateTime.fromISO($('When Executed by Another Workflow').first().json.startTime).startOf('day').toFormat("yyyy-MM-dd'T'HH:mm:ss") }}` | Start of the event's calendar day |
| Time Max | `={{ DateTime.fromISO($('When Executed by Another Workflow').first().json.startTime).plus({ days: 1 }).startOf('day').toFormat("yyyy-MM-dd'T'HH:mm:ss") }}` | Start of the following day |

> **Note:** Delete uses `startTime` (not `originalStartTime`) for the day window, because
> deletion has no destination. `startTime` represents the current location of the event to delete.

---

### Code in JavaScript2 (Delete event-ID resolver)

**Node name:** `Code in JavaScript2`  
**Type:** `n8n-nodes-base.code`

```javascript
const input = $('When Executed by Another Workflow').first().json;

const requestedTitle = (input.title || '').trim().toLowerCase();

const events = $input.all().map(item => item.json);

const matches = events.filter(event => {
  const eventTitle = (event.summary || '').trim().toLowerCase();

  return eventTitle === requestedTitle;
});

if (matches.length === 0) {
  throw new Error(
    `Event not found: "${input.title}"`
  );
}

if (matches.length > 1) {
  throw new Error(
    `Multiple events found with title "${input.title}".`
  );
}

const event = matches[0];

return [
  {
    json: {
      eventId: event.id,
      title: event.summary,
      startTime: event.start?.dateTime || '',
      endTime: event.end?.dateTime || ''
    }
  }
];
```

**What this Code node does:**

1. Reads `title` from the Worker trigger input.
2. Normalises the requested title to lowercase/trimmed for case-insensitive comparison.
3. Iterates all events returned by `Get many events2`, checking normalised title equality.
4. Throws an explicit error if zero matches are found.
5. Throws an explicit error if multiple events with the same title are found — refuses to guess.
6. Returns the resolved `eventId`, `title`, `startTime`, `endTime` for the matched event.

**Why title-only matching (no timestamp comparison) for Delete?**

Delete is simpler than Update: the user is removing an event from a specific day, so searching
the full day and matching by title is sufficient. Timestamp comparison is not needed here
because the user's intent is to delete an event on a given day by its name.

**Safety behaviour — multiple matches:**

If two events with the same title exist on the selected day, the workflow throws an error
rather than silently choosing one. This prevents accidental deletion of the wrong event.
The caller (AI Agent) should surface the ambiguity to the user for disambiguation.

---

### Delete an event

**Node name:** `Delete an event`  
**Type:** `n8n-nodes-base.googleCalendar`, operation `delete`

| Parameter | n8n Expression | Description |
|:---|:---|:---|
| Event ID | `={{ $json.eventId }}` | Real event ID (from If1 TRUE path or from Code node output) |

---

## Input Contract for DELETE

| Field | Required | Value |
|:---|:---|:---|
| `action` | ✅ | `"delete"` |
| `title` | ✅ (if eventId `""`) | Event title — used to locate event |
| `startTime` | ✅ (if eventId `""`) | Current event start time — defines which day to search |
| `endTime` | — | Not used for day-window search in delete |
| `eventId` | ✅ | Real Google Calendar event ID if known; `""` if unknown |
| `originalStartTime` | — | Not used for delete |
| `originalEndTime` | — | Not used for delete |

---

## Verified Status

Delete was **successfully tested** in the current development workflow after correcting the
event-ID resolution logic.
