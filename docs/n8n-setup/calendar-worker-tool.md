# Calendar Worker — Tool Contract

## Overview

The **Calendar Worker** is a separate n8n subworkflow (`Calendar Worker`, id: `enfAYZyRbaMrYIol`)
invoked by the Main VoxCal AI Agent as a single tool named **"Call 'Calendar Worker'"**.

The tool description below is reproduced **verbatim** from the live n8n workflow export
(`jVxIMtua9iTDiASX`, version `v0.1.54`, exported 2026-09-29). It is configured in the
`@n8n/n8n-nodes-langchain.toolWorkflow` node connected to the AI Agent.

---

## Tool Description (verbatim from n8n export)

```text
Manage the user's Google Calendar.

Actions:
- create = create a new event
- search = find events
- update = modify an existing event
- delete = remove an event

INPUT RULES:

action:
Must be exactly one of:
create, search, update, delete

title:
The event title.

startTime:
Absolute ISO 8601 datetime.
For create/search: requested event/search time.
For update: NEW event start time after the modification.
Example:
2026-10-01T10:00:00+05:30

endTime:
Absolute ISO 8601 datetime.
For create/search: requested event/search time.
For update: NEW event end time after the modification.
Example:
2026-10-01T11:00:00+05:30

originalStartTime:
For UPDATE only.
The absolute ISO 8601 start time of the EXISTING event that must be found.
Example:
2026-09-30T10:00:00+05:30

originalEndTime:
For UPDATE only.
The absolute ISO 8601 end time of the EXISTING event that must be found.
Example:
2026-09-30T11:00:00+05:30

eventId:
Use the real Google Calendar event ID when known.
If unknown, use "".
Never invent an eventId.

DATE/TIME RULE:
Resolve all relative dates and times BEFORE calling Calendar Worker.

Never send:
- today
- tomorrow
- yesterday
- next Monday
- next week
- "at 10 AM"

Send absolute ISO 8601 timestamps instead.

CREATE:
Send the resolved title, startTime and endTime.
eventId should normally be "".
originalStartTime and originalEndTime are not required.

SEARCH:
Use startTime and endTime as the search window when applicable.
originalStartTime and originalEndTime are not required.

UPDATE:
If a real eventId is known, send it.

If eventId is unknown:
- originalStartTime and originalEndTime MUST identify the EXISTING event.
- startTime and endTime MUST contain the NEW time/date for the event.
- Do not use the new startTime/endTime to locate the existing event.

Example:
User: "Move my Team meeting tomorrow from 10 AM to the day after tomorrow."

If current_date is 2026-09-29, send:

originalStartTime:
2026-09-30T10:00:00+05:30

originalEndTime:
2026-09-30T11:00:00+05:30

startTime:
2026-10-01T10:00:00+05:30

endTime:
2026-10-01T11:00:00+05:30

DELETE:
If a real eventId is known, send it.
If eventId is unknown, send eventId="" and provide the event title and the resolved current
startTime/endTime so Calendar Worker can locate the event.

TIMEZONE:
Use the supplied timezone.
Default timezone: Asia/Kolkata.
Preserve the timezone offset in the ISO timestamp.
```

---

## Input Field Schema (from workflow export)

These are the seven fields the Calendar Worker subworkflow accepts, as defined in both the
`toolWorkflow` node and the Worker's `executeWorkflowTrigger` trigger node:

| Field | Type | Description |
|:---|:---|:---|
| `action` | string | One of `create`, `search`, `update`, `delete` |
| `title` | string | Event title |
| `startTime` | string | Absolute ISO 8601 datetime (new time for update; search/create time otherwise) |
| `endTime` | string | Absolute ISO 8601 datetime |
| `eventId` | string | Real Google Calendar event ID, or `""` if unknown |
| `originalStartTime` | string | For update: existing event start time (locate by this) |
| `originalEndTime` | string | For update: existing event end time |

---

## AI-Generated Field Hints (from $fromAI wrappers in toolWorkflow node)

The `toolWorkflow` node uses `$fromAI()` expressions to pass AI-generated values. The embedded
field hints visible to the model are:

| Field | Hint |
|:---|:---|
| `action` | `Calendar action: create, search, update, or delete.` |
| `title` | `Event title. Required for create; use to identify events for search, update, or delete.` |
| `startTime` | `Event start date/time in ISO 8601 format. Resolve relative dates using current_date and current_datetime. "tomorrow" means current_date + 1 day. Timezone: Asia/Kolkata. Example: if current_date is 2026-09-29 and user says tomorrow at 10 AM, use 2026-09-30T10:00:00+05:30.` |
| `endTime` | `Event end date/time in ISO 8601 format. Resolve relative dates using current_date and current_datetime. "tomorrow" means current_date + 1 day. Timezone: Asia/Kolkata.` |
| `eventId` | `Google Calendar event ID. Always provide this field. If the event ID is unknown, use an empty string "". Never invent an event ID. For delete/update requests without a known ID, send "" so the Calendar Worker can search for the matching event first.` |
| `originalStartTime` | *(no hint — field is available but hint is empty in current export)* |
| `originalEndTime` | *(no hint — field is available but hint is empty in current export)* |

> **TODO:** Consider adding explicit hints for `originalStartTime` and `originalEndTime` in the
> `toolWorkflow` node to make their purpose more explicit to the model at the field level.
> The system prompt and tool description already explain them, but field-level hints provide
> additional reinforcement.
