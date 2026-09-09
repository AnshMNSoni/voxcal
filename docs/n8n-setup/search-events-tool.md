# Google Calendar - Search Events Tool Configuration

## Overview

The Search Events tool allows the AI Agent to query scheduled calendar events within a specified date and time window. It serves two critical functions:
1. Answering user schedule inquiries (e.g., "What do I have scheduled for today?").
2. Resolving event names to real Google Calendar Event IDs prior to updating or deleting events.

## n8n Node Configuration

- **Node Type**: Google Calendar Tool
- **Resource**: `Event`
- **Operation**: `Get Many` / `Search`
- **Connection**: Connected as a Tool to the AI Agent node

## Tool Description

```text
Search the user's Google Calendar for events within a specific date/time range. Use this tool when the user asks what events are scheduled on a particular day or time period. Always search the requested date range in Asia/Kolkata unless the user specifies another timezone.
```

## Prerequisite for Update and Delete Workflows

When users interact via voice, they refer to meetings naturally rather than providing technical IDs:

```text
User Request: "Delete my meeting tomorrow"
      |
      v
Search Calendar Events (Find events on target date)
      |
      v
Match Event by Title / Description
      |
      v
Retrieve Genuine Event ID (e.g. "ev_982401")
      |
      v
Execute Update Event or Delete Event Tool
```

Without this search step, the agent would either fail or hallucinate an Event ID, which violates the strict calendar integrity rules.
