# Calendar Worker — UPDATE Branch

## Overview

The **update** action routes through Switch output index `2` to an **If** node that checks whether
a real `eventId` is already known. This gives the Update branch two execution paths:

- **Path A (eventId known):** Update the event directly — no search needed.
- **Path B (eventId unknown):** Search the original event's calendar day, resolve the real event ID
  via a Code node, then update the event.

This is the most architecturally significant branch because it handles event-move scenarios
where the existing event must be found on its *original* date before being moved to a new date.

---

## Node Chain

```
Switch (output 2)
  └─► If  [checks: eventId not empty]
        ├─► (TRUE)  Update an event  [direct update — no search]
        └─► (FALSE) Get many events  [searches original event day]
                      └─► Code in JavaScript1  [resolves real event ID]
                            └─► Update an event  [update with resolved ID]
```

---

## The FIND-USING-ORIGINAL, UPDATE-USING-NEW Invariant

> **This is the central design rule of the Update branch.**

Update operations have two distinct concepts:

| Concept | Fields | Purpose |
|:---|:---|:---|
| **Original location** | `originalStartTime`, `originalEndTime` | Where the existing event currently lives — used to find it |
| **New location** | `startTime`, `endTime` | Where the event should be moved to — used to write to Google Calendar |

**These must never be confused.** Using the new `startTime` to search for an event being moved
will fail because the event does not yet exist at the new time.

**Example — "Move my Team meeting tomorrow from 10 AM to day after tomorrow":**

If `current_date = 2026-09-29`:

```
originalStartTime = 2026-09-30T10:00:00+05:30  ← find the event HERE
originalEndTime   = 2026-09-30T11:00:00+05:30
startTime         = 2026-10-01T10:00:00+05:30  ← update TO HERE
endTime           = 2026-10-01T11:00:00+05:30
```

---

## n8n Node Configuration (from workflow export)

### If (eventId check)

**Node name:** `If`  
**Type:** `n8n-nodes-base.if`

| Condition | Expression | Operator |
|:---|:---|:---|
| eventId not empty | `={{ $('When Executed by Another Workflow').first().json.eventId }}` | `string` → `notEmpty` |

- **TRUE output** → `Update an event` (direct update)
- **FALSE output** → `Get many events` (event-ID resolution)

---

### Get many events (Update event-ID lookup)

**Node name:** `Get many events`  
**Type:** `n8n-nodes-base.googleCalendar`, operation `getAll`

| Parameter | n8n Expression | Description |
|:---|:---|:---|
| Limit | `10` | Max events returned |
| Time Min | `={{ DateTime.fromISO($('When Executed by Another Workflow').first().json.originalStartTime).startOf('day').toFormat("yyyy-MM-dd'T'HH:mm:ss") }}` | Start of the original event's calendar day |
| Time Max | `={{ DateTime.fromISO($('When Executed by Another Workflow').first().json.originalStartTime).plus({ days: 1 }).startOf('day').toFormat("yyyy-MM-dd'T'HH:mm:ss") }}` | Start of the following day (exclusive upper bound) |

**Why search the whole day?**
Searching a full calendar day (midnight-to-midnight) is more robust than a narrow 1-hour window.
If the event's stored time has millisecond precision or minor timezone serialisation differences
from what was sent to Google Calendar, a narrow window might miss it. The full-day search retrieves
all events on the original date; the Code node then performs exact title + timestamp matching
to identify the correct event among all day's events.

**Why `originalStartTime` and not `startTime`?**
Because `startTime` contains the *destination* time (where the event is being moved to).
Searching `startTime` would look for an event that does not yet exist.

---

### Code in JavaScript1 (Update event-ID resolver)

**Node name:** `Code in JavaScript1`  
**Type:** `n8n-nodes-base.code`

This node finds the exact matching event among all events returned for the original day.

```javascript
const input = $('When Executed by Another Workflow').first().json;

const requestedTitle = (input.title || '').trim().toLowerCase();
const requestedStart = input.originalStartTime || '';

const requestedStartMs = Date.parse(requestedStart);

const events = $input.all().map(item => item.json);

const matches = events.filter(event => {
  const eventTitle = (event.summary || '').trim().toLowerCase();

  const eventStart = event.start?.dateTime || '';

  if (!eventStart) return false;

  const eventStartMs = Date.parse(eventStart);

  return (
    eventTitle === requestedTitle &&
    eventStartMs === requestedStartMs
  );
});

if (matches.length === 0) {
  throw new Error(
    `Event not found: "${input.title}" at ${requestedStart}`
  );
}

if (matches.length > 1) {
  throw new Error(
    `Multiple events found matching "${input.title}" at ${requestedStart}`
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

1. Reads `title` and `originalStartTime` from the Worker trigger input.
2. Normalises the requested title to lowercase/trimmed for case-insensitive comparison.
3. Parses `originalStartTime` with `Date.parse()` → milliseconds since epoch.
4. Iterates all events returned by `Get many events`, checking both normalised title and parsed
   start timestamp equality.
5. Throws an explicit error if zero matches are found (event not on that day).
6. Throws an explicit error if multiple matches are found (ambiguous — refuses to guess).
7. Returns the resolved `eventId`, `title`, `startTime`, `endTime` for the matched event.

**Why parse timestamps instead of comparing ISO strings directly?**

`Date.parse()` normalises both timestamps to milliseconds since epoch before comparing them.
This makes the comparison insensitive to differences in how the timezone offset is serialised
(e.g. `+05:30` vs. `+0530` vs. equivalent UTC offset), trailing zeros in sub-seconds, or
other ISO 8601 encoding variations. It is a normalisation technique that makes the comparison
more robust across different serialisation styles, not merely a workaround for one specific failure.

**Why not fuzzy title matching?**

Fuzzy matching could select the wrong event. The Code node uses exact normalised equality
to prevent silent misidentification.

---

### Update an event

**Node name:** `Update an event`  
**Type:** `n8n-nodes-base.googleCalendar`, operation `update`

| Parameter | n8n Expression | Description |
|:---|:---|:---|
| Event ID | `={{ $json.eventId }}` | Real event ID (from If TRUE path or from Code node output) |
| Start | `={{ $('When Executed by Another Workflow').first().json.startTime }}` | **NEW** start time from Worker trigger |
| End | `={{ $('When Executed by Another Workflow').first().json.endTime }}` | **NEW** end time from Worker trigger |
| Summary | `={{ $('When Executed by Another Workflow').first().json.title }}` | Event title from Worker trigger |

> **Critical:** Start and End reference the Worker **trigger** (not the Code node output).
> The Code node output contains the event's *existing* times (used only for ID resolution).
> The new times must come directly from the trigger's `startTime`/`endTime` fields.

---

## Input Contract for UPDATE

| Field | Required | Value |
|:---|:---|:---|
| `action` | ✅ | `"update"` |
| `title` | ✅ | Event title (used for search if eventId unknown) |
| `startTime` | ✅ | **NEW** start time — where event will be moved to |
| `endTime` | ✅ | **NEW** end time — where event will be moved to |
| `eventId` | ✅ | Real Google Calendar event ID if known; `""` if unknown |
| `originalStartTime` | ✅ (if eventId `""`) | Current/original start time — used to find the existing event |
| `originalEndTime` | ✅ (if eventId `""`) | Current/original end time — used to find the existing event |

---

## Verified Status

Update was **successfully tested** with the following scenario:

- Existing event: "Team meeting" on 2026-09-30 from 10:00–11:00
- Command: Move to 2026-10-01 from 10:00–11:00
- Worker located the event using `originalStartTime = 2026-09-30T10:00:00+05:30`
- Worker updated the event to `startTime = 2026-10-01T10:00:00+05:30`
